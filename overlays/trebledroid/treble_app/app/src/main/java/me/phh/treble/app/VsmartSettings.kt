package me.phh.treble.app

import android.os.Bundle

object VsmartSettings : Settings {
    val dt2w = "key_vsmart_dt2w"

    override fun enabled() = Tools.vendorFp.startsWith("vsmart/") 
}

class VsmartSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_vsmart

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        android.util.Log.d("PHH", "Loading vsmart fragment ${VsmartSettings.enabled()}")
    }
}
