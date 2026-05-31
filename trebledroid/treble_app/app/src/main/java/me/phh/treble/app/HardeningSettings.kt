package me.phh.treble.app

object HardeningSettings : Settings {
    val mteVendor = "key_hardening_mte_vendor"
    val stackHardening = "key_hardening_stack_hardening"

    override fun enabled() = true
}

class HardeningSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_hardening
}
