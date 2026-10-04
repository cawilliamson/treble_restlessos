package me.phh.treble.app

import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.content.pm.PackageManager
import android.os.Parcel
import android.os.RemoteException
import android.os.SystemProperties
import android.os.UserHandle
import android.preference.PreferenceManager
import android.util.Log
import java.io.File


object Nubia : EntryStartup {
    fun writeToFileNofail(path: String, content: String) {
        try {
            File(path).printWriter().use { it.println(content) }
        } catch(t: Throwable) {
            Log.d("PHH", "Failed writing to $path", t)
        }
    }

    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when (key) {
            NubiaSettings.dt2w -> {
                val b = sp.getBoolean(key, false)
                writeToFileNofail("/data/vendor/tp/easy_wakeup_gesture", if(b) "1" else "0")
                writeToFileNofail("/sys/devices/platform/nubia_goodix_ts.0/gesture/enable", if(b) "1" else "0")
            }
            NubiaSettings.bypassCharger -> {
                val b = sp.getBoolean(key, false)
                writeToFileNofail("/sys/kernel/nubia_charge/charger_bypass", if(b) "on" else "off")
            }
            NubiaSettings.highTouchScreenSampleRate -> {
                val b = sp.getBoolean(key, false)
                writeToFileNofail("/sys/devices/platform/nubia_goodix_ts.0/gesture/highrate", if(b) "1" else "0")
            }
            NubiaSettings.highTouchScreenSensitivity -> {
                val b = sp.getBoolean(key, false)
                writeToFileNofail("/sys/devices/platform/nubia_goodix_ts.0/gesture/sensitivity", if(b) "1" else "0")
            }
            NubiaSettings.tsGameMode -> {
                val b = sp.getBoolean(key, false)
                SystemProperties.set("sys.nubia.touch.game", if(b) "1" else "0")
            }
            NubiaSettings.fanSpeed -> {
                val i = sp.getString(key, "0")
                writeToFileNofail("/sys/kernel/fan/fan_speed_level", i)
            }
            NubiaSettings.logoBreath -> {
                val b = sp.getBoolean(key, false)
                if(b) {
                    writeToFileNofail("/sys/class/leds/blue/breath_feature", "3 1000 0 700 0 255 3")
                } else {
                    writeToFileNofail("/sys/class/leds/blue/breath_feature", "0")
                    writeToFileNofail("/sys/class/leds/red/breath_feature", "0")
                    writeToFileNofail("/sys/class/leds/green/breath_feature", "0")
                }
            }
            NubiaSettings.redmagicLed -> {
                val i = sp.getString(key, "0")
                writeToFileNofail("/sys/class/leds/aw22xxx_led/imax", "8")
                writeToFileNofail("/sys/class/leds/aw22xxx_led/effect", i)
                writeToFileNofail("/sys/class/leds/aw22xxx_led/cfg", "1")
            }
            NubiaSettings.boostCpu -> {
                val b = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.cpu.boost", if(b) "1" else "0")
                SystemProperties.set("nubia.perf.cpu.boost", if(b) "1" else "0")
            }
            NubiaSettings.boostGpu -> {
                val b = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.gpu.boost", if(b) "1" else "0")
                // 0 - normal, 1 - medium, 2 - maximum
                SystemProperties.set("nubia.perf.gpu.boost", if(b) "2" else "0")
            }

            NubiaSettings.boostCache -> {
                val b = sp.getBoolean(key, false)
                SystemProperties.set("nubia.perf.cache.boost", if(b) "1" else "0")
                SystemProperties.set("persist.sys.cache.boost", if(b) "1" else "0")
            }
            NubiaSettings.boostUfs -> {
                val b = sp.getBoolean(key, false)
                SystemProperties.set("nubia.perf.ufs", if(b) "1" else "0")
            }
            NubiaSettings.shoulderBtn -> {
                val i = if(sp.getBoolean(key, false)) "1" else "0"
                if (NubiaSettings.is6Series()){
                    // Right
                    writeToFileNofail("/sys/devices/platform/soc/a88000.i2c/i2c-3/3-0010/mode", i)
                    // Left
                    writeToFileNofail("/sys/devices/platform/soc/998000.i2c/i2c-1/1-0010/mode", i)
                } else if (NubiaSettings.is5GLite()){
                    // both right and left
                    writeToFileNofail("/sys/devices/platform/soc/880000.i2c/i2c-0/0-0010/mode", i)
                } else if (NubiaSettings.is5G5S()){
                    // Right
                    writeToFileNofail("/sys/devices/platform/soc/988000.i2c/i2c-1/1-0010/mode", i)
                    // Left
                    writeToFileNofail("/sys/devices/platform/soc/990000.i2c/i2c-2/2-0010/mode", i)
                }
            }
        }
    }

    override fun startup(ctxt: Context) {
        if (!NubiaSettings.enabled()) return

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.dt2w)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.tsGameMode)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.shoulderBtn)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.bypassCharger)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.highTouchScreenSampleRate)

        spListener.onSharedPreferenceChanged(sp, NubiaSettings.logoBreath)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.redmagicLed)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.boostCpu)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.boostGpu)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.boostCache)
        spListener.onSharedPreferenceChanged(sp, NubiaSettings.boostUfs)
        // For 6, 6s, 6s pro only
        if (SystemProperties.get("persist.sys.support.fan", "false") == "true") {
            // Auto enable fan after connected to power supplier
            ctxt.startServiceAsUser(Intent(ctxt, NubiaAutoFanControlService::class.java), UserHandle.SYSTEM)
            // Enable fan quick setting tile
            ctxt.packageManager.setComponentEnabledSetting(
                ComponentName(
                    ctxt,
                    NubiaFanControlTilesService::class.java
                ), PackageManager.COMPONENT_ENABLED_STATE_ENABLED, 0)
        }
        // Enable game mode quick setting tile
        ctxt.packageManager.setComponentEnabledSetting(
            ComponentName(
                ctxt,
                NubiaGameModeTilesService::class.java
            ), PackageManager.COMPONENT_ENABLED_STATE_ENABLED, 0)
        if (NubiaSettings.is5G5S() || NubiaSettings.is6Series() || NubiaSettings.is5GLite()) {
            // Enable shoulder triggers quick setting tile
            ctxt.packageManager.setComponentEnabledSetting(
                ComponentName(
                    ctxt,
                    NubiaShoulderBtnTilesService::class.java
                ), PackageManager.COMPONENT_ENABLED_STATE_ENABLED, 0)

        }
    }
}
