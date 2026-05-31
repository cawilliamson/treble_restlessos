package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.media.AudioSystem
import android.os.Build
import android.os.Handler
import android.os.HandlerThread
import android.os.SystemProperties
import android.preference.PreferenceManager
import android.telephony.TelephonyCallback
import android.telephony.TelephonyManager
import android.util.Log
import androidx.annotation.RequiresApi
import java.io.File

class Samsung: EntryStartup {
    /*
    Here lies some documentations about stuff that cane be done on Samsung devices:
    - For touchscreen, check /sys/devices/virtual/sec/tsp/cmd_list. It can include stuff about pen too
        - aod_enable/set_aod_rect
        - brush_enable
    - For display, check /sys/class/mdnie/mdnie
        - accesibility: 0 no, 1 negative, 2 color_blind, 3 screen off, 4 grayscale, 5 grayscale_negative, 6 color blind hbm
        - night mode 1 <0-10> (enable and set level) or 0 0 (disable)
     */
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            SamsungSettings.highBrightess -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.samsung.full_brightness", value.toString())
            }
            SamsungSettings.gloveMode -> {
                val value = sp.getBoolean(key, false)
                val cmd = if(value) "glove_mode,1" else "glove_mode,0"
                val ret = tsCmd(cmd)
                Log.e("PHH", "Setting glove mode to $cmd got $ret")
            }
            SamsungSettings.audioStereoMode -> {
                val value = sp.getBoolean(key, false)
                if(value) {
                    AudioSystem.setParameters("Dualspk=1")
                    AudioSystem.setParameters("SpkAmpLPowerOn=1")
                    AudioSystem.setParameters("ProximitySensorClosed=0")
                } else {
                    AudioSystem.setParameters("Dualspk=0")
                    AudioSystem.setParameters("SpkAmpLPowerOn=0")
                }
            }
            SamsungSettings.wirelessChargingTransmit -> {
                val value = if(sp.getBoolean(key, false)) "1" else "0"
                try {
                    File("/sys/class/power_supply/battery/wc_tx_en").writeText(value + "\n")
                } catch(e: Exception) {
                    Log.e("PHH", "Failed setting wireless charging transmit", e)
                }
            }
            SamsungSettings.doubleTapToWake -> {
                var cmd = if(sp.getBoolean(key, false)) "aot_enable,1" else "aot_enable,0"
                tsCmd(cmd)
            }
            SamsungSettings.extraSensors -> {
                val value = if(sp.getBoolean(key, false)) "true" else " false"
                SystemProperties.set("persist.sys.phh.samsung_sensors", value)
            }
            SamsungSettings.colorspace -> {
                val value = if(sp.getBoolean(key, false)) "true" else " false"
                SystemProperties.set("persist.sys.phh.samsung_colorspace", value)
            }
            SamsungSettings.brokenFingerprint -> {
                val value = if(sp.getBoolean(key, false)) "1" else " 0"
                SystemProperties.set("persist.sys.phh.samsung_fingerprint", value)
            }
            SamsungSettings.backlightMultiplier -> {
                val value = sp.getString(key, "-1")
                SystemProperties.set("persist.sys.phh.samsung_backlight", value)
            }
            SamsungSettings.cameraIds -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.samsung.camera_ids", value.toString())
            }
            SamsungSettings.alternateAudioPolicy -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.caf.audio_policy", value)
            }
            SamsungSettings.fodSingleClick -> {
                val cmd = if(sp.getBoolean(key, false)) "fod_lp_mode,1" else "fod_lp_mode,0"
                tsCmd(cmd)
            }
            SamsungSettings.flashStrength -> {
                val value = sp.getString(key, "1")
                SystemProperties.set("persist.sys.phh.flash_strength", value)
            }
            SamsungSettings.disableBackMic -> {
                val value = if(sp.getBoolean(key, false)) "true" else " false"
                SystemProperties.set("persist.sys.phh.disable_back_mic", value)
            }
        }
    }

    val telephonyCallback: TelephonyCallback = @RequiresApi(Build.VERSION_CODES.S)
    object: TelephonyCallback(), TelephonyCallback.CallStateListener {
        override fun onCallStateChanged(p0: Int) {
            Log.d("PHH", "Call state changed $p0")
            if(p0 == TelephonyManager.CALL_STATE_OFFHOOK) {
                AudioSystem.setParameters("g_call_state=514") // CALL_STATUS_VOLTE_CP_VOICE_CALL_ON
            } else {
                AudioSystem.setParameters("g_call_state=1") // CALL_STATUS_CS_VOICE_CP_VIDEO_CALL_OFF
            }
        }
    }

    @RequiresApi(Build.VERSION_CODES.S)
    override fun startup(ctxt: Context) {
        if (!SamsungSettings.enabled()) return

        val handler = Handler(HandlerThread("SamsungThread").apply { start()}.looper)

        val tm = ctxt.getSystemService(TelephonyManager::class.java)
        tm.registerTelephonyCallback({ p0 -> handler.post(p0) }, telephonyCallback)

        Log.d("PHH", "Registered telecom listener for Samsung")
        //Reset wirelesss charging transmit at every boot
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)

        sp.edit().putBoolean(SamsungSettings.wirelessChargingTransmit, false).apply()

        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, SamsungSettings.highBrightess)
        spListener.onSharedPreferenceChanged(sp, SamsungSettings.gloveMode)
        spListener.onSharedPreferenceChanged(sp, SamsungSettings.audioStereoMode)
        spListener.onSharedPreferenceChanged(sp, SamsungSettings.doubleTapToWake)
        Log.e("PHH", "Samsung TS: ${tsCmd("get_chip_vendor")}:${tsCmd("get_chip_name")}")

        Log.e("PHH", "Samsung TS: Supports glove_mode ${tsCmdExists("glove_mode")}")
        Log.e("PHH", "Samsung TS: Supports aod_enable ${tsCmdExists("aod_enable")}")

        tsCmd("check_connection")
        tsCmd("fod_enable,1,1,0")

        for(malware in listOf("com.dti.globe", "com.singtel.mysingtel", "com.LogiaGroup.LogiaDeck", "com.mygalaxy")) {
            try {
                ctxt.packageManager
                        .setApplicationEnabledSetting(malware, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0)
            } catch (t: Throwable) { }
        }

    }

    companion object: EntryStartup {
        val tspBase = "/sys/devices/virtual/sec/tsp"
        fun tsCmd(cmd: String): String {
            File("${tspBase}/cmd").writeText(cmd+"\n")
            val status = File("${tspBase}/cmd_status").readText().trim()
            val ret = File("${tspBase}/cmd_result").readText().trim()
            if(status != "OK") Log.e("PHH", "Samsung TSP answered $status when doing $cmd (Got $ret)")
            return ret
        }

        fun tsCmdExists(cmd: String): Boolean {
            val supported = File("${tspBase}/cmd_list").readLines()
            return supported.contains(cmd)
        }

        var self: Samsung? = null
        override fun startup(ctxt: Context) {
            if (!SamsungSettings.enabled()) return
            self = Samsung()
            self!!.startup(ctxt)
        }
    }
}
