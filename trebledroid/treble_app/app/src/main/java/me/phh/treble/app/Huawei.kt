package me.phh.treble.app

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.net.Uri
import android.os.SystemProperties
import android.os.Parcel
import android.os.ServiceManager
import android.os.UserHandle
import android.preference.PreferenceManager
import android.util.Log
import vendor.huawei.hardware.biometrics.fingerprint.V2_1.IExtBiometricsFingerprint
import vendor.huawei.hardware.tp.V1_0.ITouchscreen
import java.io.File

class Huawei: EntryStartup {
    val fastChargeCharger = "/sys/class/hw_power/charger/direct_charger/iin_thermal"
    val fastChargeData = "/sys/class/hw_power/charger/charge_data/iin_thermal"
    val fpService = try { IExtBiometricsFingerprint.getService() } catch(t: Throwable) { Log.d("PHH", "Failed getting fp svc" , t);null }
    val tsService = try { ITouchscreen.getService() } catch(t: Throwable) { Log.d("PHH", "Failed getting tp svc" , t); null }
    val surfaceFlinger = ServiceManager.getService("SurfaceFlinger")
    var ctxt: Context? = null

    fun enableHwcOverlay(v: Boolean) {
        val data = Parcel.obtain()
        data.writeInterfaceToken("android.ui.ISurfaceComposer")
        data.writeInt(if(v) 0 else 1)
        surfaceFlinger.transact(1008, data, null, 0)
        data.recycle()
        Log.d("PHH", "Set surface flinger hwc overlay to $v")
    }

    fun writeToFileNofail(path: String, content: String) {
        try {
            File(path).printWriter().use { it.println(content) }
        } catch(t: Throwable) {
            Log.d("PHH", "Failed writing to $path", t)
        }
    }

    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            HuaweiSettings.fingerprintGestures -> {
                val value = sp.getBoolean(key, false)
                Log.d("PHH", "Setting Huawei fingerprint gestures to $value")
                if(value)
                    fpService?.sendCmdToHal(41)
                else
                    fpService?.sendCmdToHal(42)
            }
            HuaweiSettings.touchscreenGloveMode -> {
                val value = sp.getBoolean(key, false)
                Log.d("PHH", "Setting Huawei glove mode to $value")
                tsService?.hwTsSetGloveMode(value)
            }
            HuaweiSettings.fastCharge -> {
                val value = sp.getString(key, "-1")
                when(value) {
                    "-1" -> {
                        writeToFileNofail(fastChargeCharger, "1500")
                        writeToFileNofail(fastChargeData, "900")
                    }
                    "2750" -> {
                        writeToFileNofail(fastChargeCharger, "2750")
                        writeToFileNofail(fastChargeData, "1000")
                    }
                    "4000" -> {
                        writeToFileNofail(fastChargeCharger, "4000")
                        writeToFileNofail(fastChargeData, "2000")
                    }
                }
            }
            HuaweiSettings.noHwcomposer -> {
                val value = sp.getBoolean(key, false)
                enableHwcOverlay(!value)
            }
        }
    }

    override fun startup(ctxt: Context) {
        if (!HuaweiSettings.enabled()) return

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        this.ctxt = ctxt.applicationContext

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, HuaweiSettings.fingerprintGestures)
        spListener.onSharedPreferenceChanged(sp, HuaweiSettings.touchscreenGloveMode)
        spListener.onSharedPreferenceChanged(sp, HuaweiSettings.fastCharge)
        spListener.onSharedPreferenceChanged(sp, HuaweiSettings.noHwcomposer)
        tsService?.hwTsGetChipInfo { _, chip_info ->
            Log.d("PHH", "HW Touchscreen chip $chip_info")
        }

        Log.d("PHH", "Checking HwIms status")
        val installed = ctxt.packageManager.getInstalledPackages(0).find { it.packageName == "com.huawei.ims" } != null
        val imsRroProperty = "persist.sys.phh.ims.hw"
        Log.d("PHH", "HwIms $installed installed")
        if(installed != SystemProperties.getBoolean(imsRroProperty, false)) {
            SystemProperties.set(imsRroProperty, if (installed) "true" else "false")
            val replaceIntent =
                    Intent(Intent.ACTION_PACKAGE_CHANGED)
                            .setData(Uri.parse("package:com.huawei.ims"))
                            .putExtra(Intent.EXTRA_UID, 0)
                            .putExtra(Intent.EXTRA_DONT_KILL_APP, false)
                            .putExtra(Intent.EXTRA_CHANGED_COMPONENT_NAME_LIST, emptyArray<String>())
            ctxt.sendBroadcastAsUser(replaceIntent, UserHandle.SYSTEM)
        }
    }

    companion object: EntryStartup {
        private var self: Huawei? = null
        override fun startup(ctxt: Context) {
            if (!HuaweiSettings.enabled()) return
            self = Huawei()
            self!!.startup(ctxt)
        }
    }
}
