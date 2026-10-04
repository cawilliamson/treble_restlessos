package me.phh.treble.app

import android.os.Bundle
import android.os.SystemProperties

object QualcommSettings : Settings {
    val alternateMediaprofile = "key_qualcomm_alternate_mediaprofile"
    val alternateAudiopolicy = "key_qualcomm_alternate_audiopolicy"
    val disableSoundVolumeEffect = "key_qualcomm_disable_soundvolume_effect"
    val disableStereoVoip = "key_qualcomm_disable_stereo_voip"

    override fun enabled() = QtiAudio.isQualcommDevice || SystemProperties.get("ro.hardware", "N/A") == "qcom"
}

class QualcommSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_qualcomm

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        android.util.Log.d("PHH", "Loading qualcomm fragment ${OppoSettings.enabled()}")
    }
}
