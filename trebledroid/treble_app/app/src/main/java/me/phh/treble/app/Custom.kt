package me.phh.treble.app

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.os.SystemProperties
import android.preference.PreferenceManager
import java.lang.ref.WeakReference

@SuppressLint("StaticFieldLeak")
object Custom: EntryStartup {
    lateinit var ctxt: WeakReference<Context>
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        val c = ctxt.get()
        if(c == null) return@OnSharedPreferenceChangeListener
        when(key) {
            CustomSettings.pointerType -> {
                val value = sp.getString(key, "mouse")
                when (value) {
                    "mouse" -> {
                        SystemProperties.set("persist.sys.overlay.spen_pointer", "false")
                        SystemProperties.set("persist.sys.overlay.trans_pointer", "false")
                    }
                    "pen" -> {
                        SystemProperties.set("persist.sys.overlay.spen_pointer", "true")
                        SystemProperties.set("persist.sys.overlay.trans_pointer", "false")
                    }
                    "transparent" -> {
                        SystemProperties.set("persist.sys.overlay.spen_pointer", "false")
                        SystemProperties.set("persist.sys.overlay.trans_pointer", "true")
                    }
                }
            }
            CustomSettings.accentColor -> {
                val value = sp.getString(key, "")
                val accentColorOverlays = OverlayPicker.getThemeOverlays(OverlayPicker.ThemeOverlay.AccentColor)
                accentColorOverlays
                        .filter { it.packageName != value }
                        .forEach { OverlayPicker.setOverlayEnabled(it.packageName, false) }
                if (!value.isNullOrEmpty()) {
                    OverlayPicker.setOverlayEnabled(value, true)
                }
            }
            CustomSettings.iconShape -> {
                val value = sp.getString(key, "")
                val iconShapeOverlays = OverlayPicker.getThemeOverlays(OverlayPicker.ThemeOverlay.IconShape)
                iconShapeOverlays
                        .filter { it.packageName != value }
                        .forEach { OverlayPicker.setOverlayEnabled(it.packageName, false) }
                if (!value.isNullOrEmpty()) {
                    OverlayPicker.setOverlayEnabled(value, true)
                }
            }
            CustomSettings.fontFamily -> {
                val value = sp.getString(key, "")
                val fontFamilyOverlays = OverlayPicker.getThemeOverlays(OverlayPicker.ThemeOverlay.FontFamily)
                fontFamilyOverlays
                        .filter { it.packageName != value }
                        .forEach { OverlayPicker.setOverlayEnabled(it.packageName, false) }
                if (!value.isNullOrEmpty()) {
                    OverlayPicker.setOverlayEnabled(value, true)
                }
            }
            CustomSettings.iconPack -> {
                val value = sp.getString(key, "")
                val iconPackOverlays = OverlayPicker.getThemeOverlays(OverlayPicker.ThemeOverlay.IconPack)                    
                val genericValue = value.toString().substringBeforeLast(".")
                for (o in iconPackOverlays) {
                    if (!value.isNullOrEmpty() && o.packageName.startsWith(genericValue)) {
                        OverlayPicker.setOverlayEnabled(o.packageName, true)
                    } else {
                        OverlayPicker.setOverlayEnabled(o.packageName, false)
                    }
                }
            }
        }
    }

    override fun startup(ctxt: Context) {
        if (!CustomSettings.enabled()) return

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        this.ctxt = WeakReference(ctxt.applicationContext)
    }
}
