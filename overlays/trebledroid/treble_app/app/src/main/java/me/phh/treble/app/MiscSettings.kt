package me.phh.treble.app

import android.app.AlertDialog
import android.app.Application
import android.hardware.display.DisplayManager
import android.os.Bundle
import android.util.Log
import androidx.preference.ListPreference
import androidx.preference.Preference

object MiscSettings : Settings {
    val mobileSignal = "key_misc_mobile_signal"
    val displayFps = "key_misc_display_fps"
    val dynamicFps = "key_misc_dynamic_fps"
    val maxAspectRatioPreO = "key_misc_max_aspect_ratio_pre_o"
    val multiCameras = "key_misc_multi_camera"
    val forceCamera2APIHAL3 = "key_misc_force_camera2api_hal3"
    val headsetFix = "key_huawei_headset_fix"
    val roundedCorners = "key_misc_rounded_corners"
    val roundedCornersOverlay = "key_misc_rounded_corners_overlay"
    val disableButtonsBacklight = "key_misc_disable_buttons_backlight"
    val forceNavbarOff = "key_misc_force_navbar_off"
    val bluetooth = "key_misc_bluetooth"
    val securize = "key_misc_securize"
    val secureAdb = "key_misc_secure_adb"
    val removeTelephony = "key_misc_removetelephony"
    val remotectl = "key_misc_remotectl"
    val disableAudioEffects = "key_misc_disable_audio_effects"
    val disableFastAudio = "key_misc_disable_fast_audio"
    val cameraTimestampOverride = "key_misc_camera_timestamp"
    val sysbta = "key_misc_dynamic_sysbta"
    val noHwcomposer = "key_misc_no_hwcomposer"
    val storageFUSE = "key_misc_storage_fuse"
    val backlightScale = "key_misc_backlight_scale"
    val headsetDevinput = "key_misc_headset_devinput"
    val restartRil = "key_misc_restart_ril"
    var patchSmsc = "key_misc_patch_smsc";
    val minimalBrightness = "key_misc_minimal_brightness"
    val aod = "key_misc_aod"
    val dt2w = "key_misc_dt2w"
    val restartSystemUI = "key_misc_restart_systemui"
    val fodColor = "key_misc_fod_color"
    val mtkTouchHintIsRotate = "key_misc_mediatek_touch_hint_rotate"
    val mtkGedKpi = "key_misc_mediatek_ged_kpi"
    val allowBinderThread = "key_misc_allow_binder_thread_on_incoming_calls"
    val lowGammaBrightness = "key_misc_low_gamma_brightness"
    val linearBrightness = "key_misc_linear_brightness"
    val forceDisplay5g = "key_misc_force_display_5g"
    val disableVoiceCallIn = "key_misc_disable_voice_call_in"
    val disableSfGlBackpressure = "key_misc_disable_sf_gl_backpressure"
    val disableSfHwcBackpressure = "key_misc_disable_sf_hwc_backpressure"
    val disableSaeUpgrade = "key_misc_disable_sae_upgrade"
    val escoTransportUnitSize = "key_misc_esco_transport_unit_size"
    val treatVirtualSensorsAsReal = "key_misc_treat_virtual_sensors_as_real"

    override fun enabled() = true
}

class MiscSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_misc
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)

        val securizePref = findPreference<Preference>(MiscSettings.securize)
        securizePref!!.setOnPreferenceClickListener {
                val builder = AlertDialog.Builder( this.getActivity() )
                builder.setTitle(getString(R.string.remove_root))
                builder.setMessage(getString(R.string.continue_question))

                builder.setPositiveButton(android.R.string.yes) { dialog, which ->

                var cmds = listOf(
                    arrayOf("/sbin/su", "-c", "/system/bin/phh-securize.sh"),
                    arrayOf("/system/xbin/su", "-c", "/system/bin/phh-securize.sh"),
                    arrayOf("/system/xbin/phh-su", "-c", "/system/bin/phh-securize.sh"),
                    arrayOf("/sbin/su", "0", "/system/bin/phh-securize.sh"),
                    arrayOf("/system/xbin/su", "0", "/system/bin/phh-securize.sh"),
                    arrayOf("/system/xbin/phh-su", "0", "/system/bin/phh-securize.sh")
                )
                for (cmd in cmds) {
                    try {
                        Runtime.getRuntime().exec(cmd).waitFor()
                        break
                    } catch (t: Throwable) {
                        Log.d("PHH", "Failed to exec \"" + cmd.joinToString(separator = " ") + "\", skipping")
                    }
                }
            }

            builder.setNegativeButton(android.R.string.no) { dialog, which ->
            }

            builder.show()
            return@setOnPreferenceClickListener true
        }

        val removeTelephonyPref = findPreference<Preference>(MiscSettings.removeTelephony)
        removeTelephonyPref!!.setOnPreferenceClickListener {

            val builder = AlertDialog.Builder( this.getActivity() )
            builder.setTitle(getString(R.string.remove_telephony_subsystem))
            builder.setMessage(getString(R.string.continue_question))

            builder.setPositiveButton(android.R.string.yes) { dialog, which ->

                var cmds = listOf(
                    arrayOf("/sbin/su", "-c", "/system/bin/remove-telephony.sh"),
                    arrayOf("/system/xbin/su", "-c", "/system/bin/remove-telephony.sh"),
                    arrayOf("/system/xbin/phh-su", "-c", "/system/bin/remove-telephony.sh"),
                    arrayOf("/sbin/su", "0", "/system/bin/remove-telephony.sh"),
                    arrayOf("/system/xbin/su", "0", "/system/bin/remove-telephony.sh"),
                    arrayOf("/system/xbin/phh-su", "0", "/system/bin/remove-telephony.sh")
                )
                for (cmd in cmds) {
                    try {
                        Runtime.getRuntime().exec(cmd).waitFor()
                        break
                    } catch (t: Throwable) {
                        Log.d("PHH", "Failed to exec \"" + cmd.joinToString(separator = " ") + "\", skipping")
                    }
                }
            }

            builder.setNegativeButton(android.R.string.no) { dialog, which ->
            }

            builder.show()
            return@setOnPreferenceClickListener true
        }

        val fpsPref = findPreference<ListPreference>(MiscSettings.displayFps)!!
        val displayManager = activity.getSystemService(DisplayManager::class.java)
        for(display in displayManager.displays) {
            Log.d("PHH", "Got display $display")
            for((index, mode) in display.supportedModes.withIndex()) {
                Log.d("PHH", "\tsupportedModes[$index] = $mode")
            }
        }

        val fpsEntries = listOf("Don't force") + displayManager.displays[0].supportedModes.map {
            val fps = it.refreshRate
            val w = it.physicalWidth
            val h = it.physicalHeight
            "${w}x${h}@${fps}"
        }
        val fpsValues = (-1.. displayManager.displays[0].supportedModes.size).toList().map { it.toString() }

        fpsPref.setEntries(fpsEntries.toTypedArray())
        fpsPref.setEntryValues(fpsValues.toTypedArray())

        val restartSystemUIPref = findPreference<Preference>(MiscSettings.restartSystemUI)
        restartSystemUIPref!!.setOnPreferenceClickListener {
            var cmds = listOf(
                arrayOf("/sbin/su", "-c", "/system/bin/killall com.android.systemui"),
                arrayOf("/system/xbin/su", "-c", "/system/bin/killall com.android.systemui"),
                arrayOf("/system/xbin/phh-su", "-c", "/system/bin/killall com.android.systemui"),
                arrayOf("/sbin/su", "0", "/system/bin/killall com.android.systemui"),
                arrayOf("/system/xbin/su", "0", "/system/bin/killall com.android.systemui"),
                arrayOf("/system/xbin/phh-su", "0", "/system/bin/killall com.android.systemui")
            )
            for (cmd in cmds) {
                try {
                    Runtime.getRuntime().exec(cmd).waitFor()
                    break
                } catch (t: Throwable) {
                    Log.d("PHH", "Failed to exec \"" + cmd.joinToString(separator = " ") + "\", skipping")
                }
            }
            return@setOnPreferenceClickListener true
        }
    }
}
