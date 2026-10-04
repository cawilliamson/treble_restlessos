package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.preference.PreferenceManager
import android.util.Log
import java.io.File
import java.lang.Exception

object Qualcomm: EntryStartup {
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            QualcommSettings.alternateAudiopolicy -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.caf.audio_policy", value)
            }
            QualcommSettings.alternateMediaprofile -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "true" else "false"
                Misc.safeSetprop("persist.sys.phh.caf.media_profile", value)
            }
            QualcommSettings.disableSoundVolumeEffect -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "1" else "0"
                Misc.safeSetprop("persist.sys.phh.disable_soundvolume_effect", value)
            }
            QualcommSettings.disableStereoVoip -> {
                val b = sp.getBoolean(key, false)
                val value = if(b) "true" else "false"
                Misc.safeSetprop("persist.sys.phh.disable_stereo_voip", value)
            }
        }
    }

    override fun startup(ctxt: Context) {
        if(!QualcommSettings.enabled()) return
        Log.d("PHH", "Starting Qualcomm service")
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)
    }
}
