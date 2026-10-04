package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import android.util.Log
import java.io.File
import java.lang.Exception

object Transsion: EntryStartup {
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            TranssionSettings.usbOtg -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.transsion.usbotg", value)
            }
            TranssionSettings.dt2w -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "2"
                Misc.safeSetprop("persist.sys.phh.transsion.dt2w", value)
            }
        }
    }

    override fun startup(ctxt: Context) {
        if(!TranssionSettings.enabled()) return
        Log.d("PHH", "Starting Transsion service")
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)
    }
}
