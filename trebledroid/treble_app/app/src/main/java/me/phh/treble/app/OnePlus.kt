package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioManager
import android.os.SystemProperties
import android.os.UEventObserver
import android.preference.PreferenceManager
import android.util.Log
import java.io.File

object OnePlus: EntryStartup {
    val dtPanel = "/proc/touchpanel/double_tap_enable" //OP3
    val gestureEnable = "/proc/touchpanel/gesture_enable" //OP6
    val triStateKey = "/devices/platform/soc/soc:tri_state_key"
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            OnePlusSettings.displayModeKey -> {
                val value = sp.getString(key, "default")
                when (value) {
                    "default" -> {
                        SystemProperties.set("sys.dci3p", "0")
                        SystemProperties.set("sys.dcip3", "0")
                        SystemProperties.set("sys.srgb", "0")
                        SystemProperties.set("sys.default_mode", "1")
                    }
                    "srgb" -> {
                        SystemProperties.set("sys.dci3p", "0")
                        SystemProperties.set("sys.dcip3", "0")
                        SystemProperties.set("sys.srgb", "1")
                        SystemProperties.set("sys.default_mode", "0")
                    }
                    "dcip3" -> {
                        SystemProperties.set("sys.dci3p", "1")
                        SystemProperties.set("sys.dcip3", "1")
                        SystemProperties.set("sys.srgb", "0")
                        SystemProperties.set("sys.default_mode", "0")
                    }
                }
            }
            OnePlusSettings.usbOtgKey -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.oem.otg_support", value.toString())
            }
            OnePlusSettings.highBrightnessModeKey -> {
                val value = sp.getString(key, "0")
                SystemProperties.set("sys.hbm", value)
            }
            OnePlusSettings.dt2w -> {
                //TODO: We need to check that the screen is on at this time
                //This won't have any effect if done with screen off
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                writeToFileNofail(dtPanel, value)
                try {
                    val v = if(b) arrayOf<Byte>(-128, 0) else arrayOf<Byte>(0, 0)
                    File(gestureEnable).outputStream().use { it.write(v.toByteArray()) }
                } catch(t: Throwable) { Log.d("PHH", "OP: Couldn't write to gesture enable")}
                Log.d("PHH", "Setting dtPanel to $value")
            }
        }
    }

    fun writeToFileNofail(path: String, content: String) {
        try {
            File(path).printWriter().use { it.println(content) }
        } catch(t: Throwable) {
            Log.d("PHH", "Failed writing to $path", t)
        }
    }

    override fun startup(ctxt: Context) {
        if(!OnePlusSettings.enabled()) return
        Log.d("PHH", "Starting OP service")

        if (File("/sys" + triStateKey).exists()) {
            //TODO: Oneplus framework calibrates tri_state_key because it is a hall sensor
            // It does so by copying /mnt/vendor/persist/engineermode/tri_state_hall_data to /sys/bus/platform/devices/soc:tri_state_key/hall_data_calib
            object : UEventObserver() {
                override fun onUEvent(event: UEventObserver.UEvent) {
                    try {
                        android.util.Log.v("PHH", "USB UEVENT: " + event.toString())
                        val state = event.get("STATE")
                        val ringing = state.contains("USB=0")
                        val silent = state.contains("(null)=0")
                        val vibrate = state.contains("USB_HOST=0")
                        android.util.Log.v("PHH", "Got ringing = $ringing, silent = $silent, vibrate = $vibrate")
                        if (ringing && !silent && !vibrate)
                            Tools.audioManager.setRingerMode(AudioManager.RINGER_MODE_NORMAL)
                        if (silent && !ringing && !vibrate)
                            Tools.audioManager.setRingerMode(AudioManager.RINGER_MODE_SILENT)
                        if (vibrate && !silent && !ringing)
                            Tools.audioManager.setRingerMode(AudioManager.RINGER_MODE_VIBRATE)
                    } catch (e: Exception) {
                        android.util.Log.d("PHH", "Failed parsing uevent", e)
                    }
                }
            }.startObserving("DEVPATH=" + triStateKey)
        }

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, OnePlusSettings.displayModeKey)
        spListener.onSharedPreferenceChanged(sp, OnePlusSettings.usbOtgKey)
        spListener.onSharedPreferenceChanged(sp, OnePlusSettings.highBrightnessModeKey)
        spListener.onSharedPreferenceChanged(sp, OnePlusSettings.dt2w)

    }
}
