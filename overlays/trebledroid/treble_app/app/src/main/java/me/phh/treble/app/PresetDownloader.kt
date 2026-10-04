package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.net.ConnectivityManager
import android.net.Network
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemProperties
import android.preference.PreferenceManager
import android.util.Log
import org.json.JSONArray
import org.json.JSONObject
import java.io.File
import java.lang.Exception
import java.lang.ref.WeakReference
import java.net.HttpURLConnection
import java.net.URL

class PresetDownloader {
    companion object {
        private val handlerThread = HandlerThread("PresetDownloader").also { it.start() }
        val handler = Handler(handlerThread.looper)
        var ctxtWp: WeakReference<Context>? = null

        var alreadyDownloaded = false

        val jsonLock = Object() //covers json and matchedNodes
        var json: JSONArray? = null
        var matchedNodes = emptyList<JSONObject>()

        val downloadPresets = object: Runnable {
            override fun run() {
                try {
                    Log.i("PHH-Presets", "Downloading...")
                    val ctxt = ctxtWp?.get() ?: return
                    if (alreadyDownloaded) return

                    var presetsUrl = "https://raw.githubusercontent.com/TrebleDroid/treble_presets/master/infos.json"
                    val presetsProp = SystemProperties.get("ro.system.treble.presets", "")
                    if (presetsProp.trim() != "") {
                        presetsUrl = presetsProp
                    }

                    Log.d("PHH-Presets", "URL: $presetsUrl")
                    val connection = URL(presetsUrl).openConnection() as? HttpURLConnection
                            ?: return
                    if (connection.responseCode != 200) {
                        val error = connection.errorStream.bufferedReader().use { it.readText() }
                        Log.e("PHH-Presets", "Failed getting infos.json, received ${connection.responseCode}: $error")
                        return
                    }

                    val outputFile = File(ctxt.filesDir, "presets.json")
                    outputFile.outputStream().use { os ->
                        connection.inputStream.use { inS ->
                            inS.copyTo(os)
                        }
                    }

                    alreadyDownloaded = true
                    handler.removeCallbacks(parsePresets)
                    handler.post(parsePresets)
                } catch(e: Exception) {
                    Log.d("PHH-Presets", "Failed downloading presets...", e)
                }
            }
        }

        val parsePresets = object: Runnable {
            override fun run() {
                try {
                    val ctxt = ctxtWp?.get() ?: return
                    val file = File(ctxt.filesDir, "presets.json")
                    val strJson = file.inputStream().bufferedReader().use { it.readText() }
                    synchronized(jsonLock) {
                        json = JSONArray(strJson)
                    }
                    handler.post(findMatchingNodes)
                } catch(e: Exception) {
                    Log.d("PHH-Presets", "Failed parsing presets...", e)
                }
            }
        }

        val findMatchingNodes = object: Runnable {
            override fun run() {
                try {
                    Log.d("PHH-Presets", "Find matching nodes...")
                    val vendorFp = SystemProperties.get("ro.vendor.build.fingerprint", "nope")
                    val oppoPrjName = SystemProperties.get("ro.boot.prjname", "nope")
                    val json = synchronized(jsonLock) { json } ?: return

                    val mutablesNodes = mutableListOf<JSONObject>()

                    for (i in 0 until json.length()) {
                        val node = json.getJSONObject(i)
                        val prop = node.getString("match_property")
                        val re = Regex(node.getString("match_value"))
                        val propValue = when (prop) {
                            "vendor_fp" -> vendorFp
                            "oppo_prjname" -> oppoPrjName
                            else -> null
                        }
                        if (propValue == null) continue
                        if (!re.matches(propValue)) continue
                        mutablesNodes.add(node)
                    }
                    synchronized(jsonLock) {
                        matchedNodes = mutablesNodes.toList()
                    }
                    Log.d("PHH-Presets", "Found ${mutablesNodes.size}")
                    handler.post(applyPresets)
                } catch(e: Exception) {
                    Log.d("PHH-Presets", "Failed finding matching nodes", e)
                }
            }
        }

        var forcePresets = false
        val applyPresets = object: Runnable {
            override fun run() {
                try {
                    val ctxt = ctxtWp?.get() ?: return
                    val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
                    val spEdit = sp.edit()
                    val nodes = synchronized(jsonLock) { matchedNodes }
                    for (node in nodes) {
                        if (!node.has("preferences")) continue
                        val prefs = node.getJSONObject("preferences")
                        for (key in prefs.keys()) {
                            Log.d("PHH-Presets", "Setting preset $key?")
                            if (sp.contains(key) && !forcePresets) continue
                            val value = prefs.get(key)
                            Log.d("PHH-Presets", " => $value")
                            Log.d("PHH-Presets", "$key has type ${value.javaClass}")
                            when (value) {
                                is Int -> spEdit.putInt(key, value as Int)
                                is Boolean -> spEdit.putBoolean(key, value as Boolean)
                                is String -> {
                                    if(value == "true" || value == "false") {
                                        spEdit.putBoolean(key, value == "true")
                                    } else {
                                        spEdit.putString(key, value as String)
                                    }
                                }
                                else -> {
                                    Log.d("PHH-Presets", "Unknown key type. Ignoring...")
                                }
                            }
                        }
                    }
                    spEdit.apply()
                    forcePresets = false
                } catch(e: Exception) {
                    Log.d("PHH-Presets", "Failed applying presets", e)
                }
            }
        }

        private val networkListener = object: ConnectivityManager.NetworkCallback() {
            override fun onAvailable(network: Network) {
                handler.post(downloadPresets)
            }
        }

        fun startup(ctxt: Context) {
            ctxtWp = WeakReference(ctxt)
            val cm = ctxt.getSystemService(ConnectivityManager::class.java) ?: return
            cm.registerDefaultNetworkCallback(networkListener)

            handler.post(downloadPresets)
            handler.removeCallbacks(parsePresets)
            handler.post(parsePresets)
        }
    }
}
