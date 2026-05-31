package me.phh.treble.app

object SamsungSettings : Settings {
    val highBrightess = "key_samsung_high_brightness"
    val gloveMode = "key_samsung_glove_mode"
    val audioStereoMode = "key_samsung_audio_stereo"
    val wirelessChargingTransmit = "key_samsung_wireless_charging_transmit"
    val doubleTapToWake = "key_samsung_double_tap_to_wake"
    val extraSensors = "key_samsung_extra_sensors"
    val colorspace = "key_samsung_colorspace"
    val brokenFingerprint = "key_samsung_broken_fingerprint"
    val backlightMultiplier = "key_samsung_backlight_multiplier"
    val cameraIds = "key_samsung_camera_ids"
    val alternateAudioPolicy = "key_samsung_alternate_audio_policy"
    val fodSingleClick = "key_samsung_fod_single_click"
    val flashStrength = "key_samsung_flash_strength"
    val disableBackMic = "key_samsung_disable_back_mic"

    override fun enabled() = Tools.vendorFpLow.startsWith("samsung/") || Tools.vendorFpLow.startsWith("kddi/scv41_")
}

class SamsungSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_samsung
}
