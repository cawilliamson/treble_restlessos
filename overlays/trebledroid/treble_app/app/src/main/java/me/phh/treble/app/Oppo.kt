package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import android.util.Log
import java.io.File
import java.lang.Exception

object Oppo: EntryStartup {
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            OppoSettings.dt2w -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.oppo.dt2w", value)
            }
            OppoSettings.gamingMode -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.oppo.gaming_mode", value)
            }
            OppoSettings.usbOtg -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.oppo.usbotg", value)
            }
            OppoSettings.dcDiming -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                try {
                    File("/sys/kernel/oppo_display/dimlayer_bl_en ").writeText(value)
                } catch(e: Exception) {
                    Log.d("PHH", "Failed setting dc diming", e)
                }
            }
        }
    }

    override fun startup(ctxt: Context) {
        if(!OppoSettings.enabled()) return
        Log.d("PHH", "Starting OPPO service")
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, OppoSettings.gamingMode)
        spListener.onSharedPreferenceChanged(sp, OppoSettings.dt2w)
        spListener.onSharedPreferenceChanged(sp, OppoSettings.usbOtg)
        spListener.onSharedPreferenceChanged(sp, OppoSettings.dcDiming)
    }
}
