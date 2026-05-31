package me.phh.treble.app

import android.content.SharedPreferences
import android.os.Bundle
import android.os.Handler
import android.os.Looper
import android.os.ServiceManager
import android.preference.PreferenceManager
import android.util.Log
import vendor.mediatek.hardware.agolddaemon.IAgoldDaemon
import java.io.File

object Unihertz8849Settings : Settings {
    val warningLight = "unihertz8849_warning_light"
    val campingLight = "unihertz8849_camping_light"
    val projectorEnabled = "unihertz8849_projector_enabled"
    val projectorMode = "unihertz8849_projector_mode"
    val displayEnabled = "unihertz8849_display_enabled"
    val brightness = "unihertz8849_brightness"
    val otherCalibration = "unihertz8849_other_calibration"
    val laserEnabled = "unihertz8849_laser_enabled"

    override fun enabled(): Boolean {
        try {
            val binder = ServiceManager.checkService(IAgoldDaemon.DESCRIPTOR + "/default")
            if (binder == null) return false
            return File("/sys/bus/spi/drivers/fpga_spi/projector_sys_status").exists()
        } catch (t: Throwable) {
            Log.d("PHH", "Unihertz8849 detection failed", t)
            return false
        }
    }
}

class Unihertz8849SettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_unihertz8849

    private val handler = Handler(Looper.getMainLooper())
    private var laserRunnable: Runnable? = null

    private lateinit var sp: SharedPreferences
    private val spListener = SharedPreferences.OnSharedPreferenceChangeListener { _, key ->
        if (key == Unihertz8849Settings.laserEnabled) updateLaserState()
    }

    private fun readSysFile(path: String): String {
        try {
            return File(path).readText().trim()
        } catch (t: Throwable) {
            Log.d("PHH", "readSysFile failed for $path", t)
        }
        return ""
    }

    private fun isLaserActive(): Boolean {
        val projectorOn = sp.getBoolean(Unihertz8849Settings.projectorEnabled, false)
        val laserEnabled = sp.getBoolean(Unihertz8849Settings.laserEnabled, false)
        return projectorOn || laserEnabled
    }

    private fun updateLaserState() {
        if (isLaserActive()) startLaserUpdates() else stopLaserUpdates()
    }

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        sp = PreferenceManager.getDefaultSharedPreferences(activity)
        sp.registerOnSharedPreferenceChangeListener(spListener)
    }

    override fun onResume() {
        super.onResume()
        updateLaserState()
    }

    override fun onPause() {
        super.onPause()
        // Turn off the laser sight when leaving the page — the hardware has
        // no auto-off and the laser should not stay active in the background.
        if (sp.getBoolean(Unihertz8849Settings.laserEnabled, false)) {
            sp.edit().putBoolean(Unihertz8849Settings.laserEnabled, false).apply()
        }
        stopLaserUpdates()
    }

    override fun onDestroyView() {
        super.onDestroyView()
        sp.unregisterOnSharedPreferenceChangeListener(spListener)
        stopLaserUpdates()
    }

    private fun startLaserUpdates() {
        if (laserRunnable != null) return  // already running
        val runnable = object : Runnable {
            override fun run() {
                try {
                    val distance = readSysFile("/sys/nds03_ctrl/nds03_read")
                    val pref = findPreference<androidx.preference.Preference>("unihertz8849_laser_display")
                    pref?.summary = if (distance.isNotEmpty() && distance != "65300") {
                        val mm = distance.toIntOrNull()
                        when {
                            mm == null -> "$distance mm"
                            mm >= 1000 -> "${"%.2f".format(mm / 1000.0)} m"
                            mm >= 10   -> "${"%.1f".format(mm / 10.0)} cm"
                            else       -> "$mm mm"
                        }
                    } else
                        "Out of range"
                } catch (t: Throwable) {
                    // ignore
                }
                handler.postDelayed(this, 1000)
            }
        }
        laserRunnable = runnable
        handler.post(runnable)
    }

    private fun stopLaserUpdates() {
        laserRunnable?.let { handler.removeCallbacks(it) }
        laserRunnable = null
        findPreference<androidx.preference.Preference>("unihertz8849_laser_display")
            ?.summary = "---"
    }
}
