package me.phh.treble.app

import android.os.SystemProperties

object HuaweiSettings : Settings {
    val fingerprintGestures = "key_huawei_fingerprint_gestures"
    val touchscreenGloveMode = "key_huawei_touchscreen_glove_mode"
    val fastCharge = "key_huawei_fast_charge"
    val noHwcomposer = "key_huawei_no_hwcomposer"

    override fun enabled(): Boolean =
            Tools.vendorFpLow.contains("huawei") ||
            Tools.vendorFpLow.contains("honor") ||
            SystemProperties.getBoolean("persist.sys.overlay.huawei", false)
}

class HuaweiSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_huawei
}
