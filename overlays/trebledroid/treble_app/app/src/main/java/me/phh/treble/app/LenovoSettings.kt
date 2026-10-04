package me.phh.treble.app

import java.io.File

object LenovoSettings : Settings {
    val dt2w = "lenovo_double_tap_to_wake"
    val support_pen = "lenovo_support_pen"

    override fun enabled(): Boolean {
        return Tools.vendorFp.contains("Lenovo") &&
            (File(Lenovo.dtPanel).exists() || File(Lenovo.dtPanel_Y700_2023).exists())
    }
}

class LenovoSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_lenovo
}
