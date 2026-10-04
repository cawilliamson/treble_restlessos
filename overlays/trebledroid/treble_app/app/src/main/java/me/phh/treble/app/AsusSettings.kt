package me.phh.treble.app

import android.os.Bundle

object AsusSettings : Settings {
    val dt2w = "key_asus_dt2w"
    val gloveMode = "key_asus_glove_mode"
    val fpWake = "key_asus_fp_wake"
    val usbPortPicker = "key_asus_usb_port_picker"

    override fun enabled() = Tools.vendorFp.startsWith("asus/") 
}

class AsusSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_asus

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        android.util.Log.d("PHH", "Loading asus fragment ${AsusSettings.enabled()}")
    }
}
