package me.phh.treble.app

import android.os.Bundle

object OnePlusSettings : Settings {
    val displayModeKey = "key_oneplus_display_mode"
    val highBrightnessModeKey = "key_oneplus_display_high_brightness"
    val usbOtgKey = "key_oneplus_usb_otg"
    val dt2w = "key_oneplus_double_tap_to_wake"

    override fun enabled() = Tools.vendorFp.contains("OnePlus")
}

class OnePlusSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_oneplus

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        SettingsActivity.bindPreferenceSummaryToValue(findPreference(OnePlusSettings.displayModeKey)!!)
        SettingsActivity.bindPreferenceSummaryToValue(findPreference(OnePlusSettings.highBrightnessModeKey)!!)
    }
}
