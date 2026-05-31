package me.phh.treble.app

import android.os.Bundle

object TranssionSettings : Settings {
    val usbOtg = "key_transsion_usb_otg"
    val dt2w = "key_transsion_dt2w"

    override fun enabled() = Tools.vendorFp.startsWith("Infinix/") || Tools.vendorFp.startsWith("TECNO/")
        || Tools.vendorFp.startsWith("Itel/")
}

class TranssionSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_transsion

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        android.util.Log.d("PHH", "Loading Transsion fragment ${TranssionSettings.enabled()}")
    }
}
