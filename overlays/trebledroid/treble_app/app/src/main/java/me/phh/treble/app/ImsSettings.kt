package me.phh.treble.app

import android.app.AlertDialog
import android.app.Application
import android.app.DownloadManager
import android.app.PendingIntent
import android.content.*
import android.content.pm.PackageInstaller
import android.hardware.display.DisplayManager
import android.net.Uri
import android.os.Build
import android.os.Bundle
import android.os.Environment
import android.os.StrictMode
import android.os.UserHandle
import android.telephony.TelephonyManager
import android.util.Log
import android.widget.Toast
import androidx.core.content.FileProvider
import androidx.preference.ListPreference
import androidx.preference.Preference
import dalvik.system.PathClassLoader
import java.io.ByteArrayInputStream
import java.io.File
import java.io.FileInputStream

object ImsSettings : Settings {
    val requestNetwork = "key_ims_request_network"
    val createApn = "key_ims_create_apn"
    val forceEnableSettings = "key_ims_force_enable_setting"
    val installImsApk = "key_ims_install_apn"

    fun checkHasPhhSignature(): Boolean {
        try {
            val cl = PathClassLoader(
                "/system/framework/services.jar",
                ClassLoader.getSystemClassLoader()
            )
            val pmUtils = cl.loadClass("com.android.server.pm.PackageManagerServiceUtils")
            val field = pmUtils.getDeclaredField("PHH_SIGNATURE")
            Log.d("PHH", "checkHasPhhSignature Field $field")
            return true
        } catch(t: Throwable) {
            Log.d("PHH", "checkHasPhhSignature Field failed")
            return false
        }
    }


    override fun enabled() = true
}

class ImsSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_ims
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)

        val createApn = findPreference<Preference>(ImsSettings.createApn)
        createApn!!.setOnPreferenceClickListener {
            Log.d("PHH", "Adding \"ims\" APN")

            val tm = activity.getSystemService(TelephonyManager::class.java)
            val operator = tm.simOperator
            if(tm.simOperator == null || tm.simOperator == "") {
                Log.d("PHH","No current carrier bailing out")
                return@setOnPreferenceClickListener true
            }

            val mcc = operator.substring(0, 3)
            val mnc = operator.substring(3, operator.length)
            Log.d("PHH", "Got mcc = $mcc and mnc = $mnc")

            val cr = activity.contentResolver

            val cursor = cr.query(
                    Uri.parse("content://telephony/carriers/current"),
                    arrayOf("name", "type", "apn", "carrier_enabled", "edited"),
                    "name = ?", arrayOf("PHH IMS"), null
            )

            if(cursor != null && cursor.moveToFirst()) {
                Log.d("PHH", "PHH IMS APN for this provider is already here with data $cursor")
                return@setOnPreferenceClickListener true
            }

            Log.d("PHH", "No APN called PHH IMS, adding our own")

            val cv = ContentValues()
            cv.put("name", "PHH IMS")
            cv.put("apn", "ims")
            cv.put("type", "ims")
            cv.put("edited", "1")
            cv.put("user_editable", "1")
            cv.put("user_visible", "1")
            cv.put("protocol", "IPV4V6")
            cv.put("roaming_protocol", "IPV6")
            cv.put("modem_cognitive", "1")
            cv.put("numeric", operator)
            cv.put("mcc", mcc)
            cv.put("mnc", mnc)

            val res = cr.insert(Uri.parse("content://telephony/carriers"), cv)
            Log.d("PHH", "Insert APN returned $res")

            return@setOnPreferenceClickListener true
        }

        val installIms = findPreference<Preference>(ImsSettings.installImsApk)

        Log.d("PHH", "MTK P radio = ${Ims.gotMtkP}")
        Log.d("PHH", "MTK Q radio = ${Ims.gotMtkQ}")
        Log.d("PHH", "MTK R radio = ${Ims.gotMtkR}")
        Log.d("PHH", "MTK S radio = ${Ims.gotMtkS}")
        Log.d("PHH", "MTK AIDL radio = ${Ims.gotMtkAidl}")
        Log.d("PHH", "Qualcomm HIDL radio = ${Ims.gotQcomHidl}")
        Log.d("PHH", "Qualcomm AIDL radio = ${Ims.gotQcomAidl}")

        val signSuffix = if(ImsSettings.checkHasPhhSignature()) "-resigned" else ""

        val (url, message) =
                when {
                    (Ims.gotMtkR || Ims.gotMtkS || Ims.gotMtkAidl) && Build.VERSION.SDK_INT >= 34
                        -> Pair("https://treble.phh.me/ims-mtk-u$signSuffix.apk", "MediaTek R+ vendor")
                    Ims.gotMtkP -> Pair("https://treble.phh.me/stable/ims-mtk-p$signSuffix.apk", "MediaTek P vendor")
                    Ims.gotMtkQ -> Pair("https://treble.phh.me/stable/ims-mtk-q$signSuffix.apk", "MediaTek Q vendor")
                    Ims.gotMtkR -> Pair("https://treble.phh.me/stable/ims-mtk-r$signSuffix.apk", "MediaTek R vendor")
                    Ims.gotMtkS -> Pair("https://treble.phh.me/stable/ims-mtk-s$signSuffix.apk", "MediaTek S vendor")
                    (Ims.gotQcomHidl || Ims.gotQcomAidl) && Build.VERSION.SDK_INT >= 34
                        -> Pair("https://treble.phh.me/ims-caf-u$signSuffix.apk", "Qualcomm vendor")
                    Ims.gotQcomHidlMoto -> Pair("https://treble.phh.me/stable/ims-caf-moto$signSuffix.apk", "Qualcomm pre-S vendor (Motorola)")
                    Ims.gotQcomHidl -> Pair("https://treble.phh.me/stable/ims-q.64$signSuffix.apk", "Qualcomm pre-S vendor")
                    Ims.gotQcomAidl -> Pair("https://treble.phh.me/stable/ims-caf-s$signSuffix.apk", "Qualcomm S+ vendor")
                    else -> Pair("https://treble.phh.me/floss-ims-resigned.apk", "Floss IMS (EXPERIMENTAL)")
                }

        installIms!!.title = "Install IMS APK for $message"
        installIms!!.setOnPreferenceClickListener {
            val dm = activity.getSystemService(DownloadManager::class.java)

            val downloadRequest = DownloadManager.Request(Uri.parse(url))
            downloadRequest.setTitle("IMS APK")
            downloadRequest.setNotificationVisibility(DownloadManager.Request.VISIBILITY_VISIBLE_NOTIFY_COMPLETED)

            downloadRequest.setDestinationInExternalFilesDir(activity, Environment.DIRECTORY_DOWNLOADS, "ims.apk")

            val myId = dm!!.enqueue(downloadRequest)

            activity.registerReceiver(object: BroadcastReceiver() {
                override fun onReceive(context: Context, intent: Intent) {
                    Log.d("PHH", "Received download completed with intent $intent ${intent.data}")
                    if(intent.getLongExtra(DownloadManager.EXTRA_DOWNLOAD_ID, -1L) != myId) return

                    val query = DownloadManager.Query().setFilterById(myId)
                    val cursor = dm.query(query)
                    if(!cursor.moveToFirst()) {
                        Log.d("PHH", "DownloadManager gave us an empty cursor")
                        return
                    }

                    val localUri = Uri.parse(cursor.getString(cursor.getColumnIndex(DownloadManager.COLUMN_LOCAL_URI)))
                    Log.d("PHH", "Got localURI = $localUri")
                    val filename = localUri.lastPathSegment
                    val path = localUri.path!!
                    val pi = context.packageManager.packageInstaller
                    val sessionId = pi.createSession(PackageInstaller.SessionParams(PackageInstaller.SessionParams.MODE_FULL_INSTALL))
                    val session = pi.openSession(sessionId)

                    Misc.safeSetprop("persist.vendor.vilte_support", "0")

                    session.openWrite("hello", 0, -1).use { output ->
                        FileInputStream(path).use { input ->
                            val buf = ByteArray(512*1024)
                            while(input.available()>0) {
                                val l = input.read(buf)
                                output.write(buf, 0, l)
                            }
                            session.fsync(output)
                        }
                    }

                    activity.registerReceiver(
                            object: BroadcastReceiver() {
                                override fun onReceive(p0: Context?, intet: Intent?) {
                                    Log.e("PHH", "Apk install received $intent" )
                                    Toast.makeText(p0, "IMS apk installed! You may now reboot.", Toast.LENGTH_LONG).show()
                                }
                            },
                            IntentFilter("me.phh.treble.app.ImsInstalled")
                    )

                    session.commit(
                            PendingIntent.getBroadcast(
                                    this@ImsSettingsFragment.activity,
                                    1,
                                    Intent("me.phh.treble.app.ImsInstalled"),
                                    PendingIntent.FLAG_ONE_SHOT or PendingIntent.FLAG_IMMUTABLE).intentSender)
                    activity.unregisterReceiver(this)
                }

            }, IntentFilter(DownloadManager.ACTION_DOWNLOAD_COMPLETE))

            return@setOnPreferenceClickListener true
        }
    }
}
