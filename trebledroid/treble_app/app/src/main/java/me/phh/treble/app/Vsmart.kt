package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import android.util.Log
import java.io.File
import java.lang.Exception

object Vsmart: EntryStartup {
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            VsmartSettings.dt2w -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.vsmart.dt2w", value)
            }
        }
    }

    override fun startup(ctxt: Context) {
        if(!VsmartSettings.enabled()) return
        Log.d("PHH", "Starting Vsmart service")
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)
    }
}
