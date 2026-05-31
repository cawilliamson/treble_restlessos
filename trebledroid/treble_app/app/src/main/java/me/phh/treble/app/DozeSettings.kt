package me.phh.treble.app

import android.hardware.Sensor
import android.os.Bundle
import android.preference.PreferenceManager
import androidx.preference.SwitchPreference
import me.phh.treble.app.DozeSettings.chopchopkey

object DozeSettings : Settings {
    val handwaveKey = "key_doze_handwave"
    val pocketKey = "key_doze_pocket"
    val chopchopkey = "key_doze_chopchop"

    override fun enabled(): Boolean {
        //TODO: Check if sensors are available and respond to interrupts
        return true
    }
}

class DozeSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_doze

    //Checking for ChopChop Sensor
    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)
        val chopchopPref = findPreference<SwitchPreference>(DozeSettings.chopchopkey)
        var chopchopSensor: Sensor? = null
        try {
            chopchopSensor = Doze.sensorManager.getSensorList(Sensor.TYPE_ALL).first { it.stringType == "com.motorola.sensor.chopchop" }

        } catch (e: Exception){
            // Disabeling ChopChop Preference
            chopchopPref!!.isVisible = false
            chopchopPref!!.isEnabled = false
            chopchopPref!!.isChecked = false
            val sp = PreferenceManager.getDefaultSharedPreferences(activity)
            val editor = sp.edit()
            editor.putBoolean(chopchopkey, false)
            editor.apply()
        }
    }
}
