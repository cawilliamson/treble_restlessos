package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioManager
import android.os.SystemProperties
import android.os.UEventObserver
import android.preference.PreferenceManager
import android.util.Log
import java.io.File

object Lenovo: EntryStartup {
    val dtPanel = "/sys/devices/virtual/touch/tp_dev/gesture_on" //K5P

    // DT2W for Y700 (2023)
    val dtPanel_Y700_2023 = "/proc/gesture_control"
    // stylus pen support for Y700 (2023)
    val supportPen_Y700_2023 = "/proc/support_pen"

    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            LenovoSettings.dt2w -> {
                if (File(Lenovo.dtPanel).exists()) {
                    //TODO: We need to check that the screen is on at this time
                    //This won't have any effect if done with screen off
                    val b = sp.getBoolean(key, false)
                    val value = if(b) "1" else "0"
                    writeToFileNofail(dtPanel, value)
                } else if (File(Lenovo.dtPanel_Y700_2023).exists()) {
                    val b = sp.getBoolean(key, false)
                    val value = if(b) "1" else "0"
                    writeToFileNofail(dtPanel_Y700_2023, value)
                }
            }
            LenovoSettings.support_pen -> {
                if (File(Lenovo.supportPen_Y700_2023).exists()) {
                    val b = sp.getBoolean(key, false)
                    val value = if(b) "1" else "0"

                    writeToFileNofail(supportPen_Y700_2023, value)
                }
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
        if(!LenovoSettings.enabled()) return

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, LenovoSettings.dt2w)
        spListener.onSharedPreferenceChanged(sp, LenovoSettings.support_pen)
    }
}