package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.os.SystemProperties
import android.util.Log
import androidx.preference.PreferenceManager

object Hardening : EntryStartup {
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when (key) {
            HardeningSettings.mteVendor -> {
                val value = sp.getBoolean(key, false)
                Misc.safeSetprop("persist.sys.phh.hardening.mte_vendor", if (value) "true" else "false")
            }
            HardeningSettings.stackHardening -> {
                val value = sp.getBoolean(key, false)
                Misc.safeSetprop("persist.sys.phh.hardening.stack_hardening", if (value) "true" else "false")
            }
        }
    }

    override fun startup(ctxt: Context) {
        if (!HardeningSettings.enabled()) return
        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        // Re-apply on boot
        spListener.onSharedPreferenceChanged(sp, HardeningSettings.mteVendor)
        spListener.onSharedPreferenceChanged(sp, HardeningSettings.stackHardening)
    }
}
