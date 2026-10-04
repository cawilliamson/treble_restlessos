package me.phh.treble.app


object XiaomiSettings : Settings {
    val dt2w = "xiaomi_double_tap_to_wake"

    override fun enabled() = Tools.vendorFp.toLowerCase().startsWith("xiaomi") ||
                             Tools.vendorFp.toLowerCase().startsWith("redmi/") ||
                             Tools.vendorFp.toLowerCase().startsWith("poco/")
}

class XiaomiSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_xiaomi
}
