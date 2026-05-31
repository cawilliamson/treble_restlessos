package me.phh.treble.app

import android.content.Intent
import android.net.Uri
import android.os.Bundle
import androidx.preference.Preference

object MyDeviceSettings : Settings {
    val maintainer = "key_mydevice_maintainer"
    val tgGroup = "key_mydevice_telegram_group"
    val presets = "key_mydevice_apply_presets"
    override fun enabled() = synchronized(PresetDownloader.jsonLock) { PresetDownloader.matchedNodes.isNotEmpty() }
}

class MyDeviceSettingsFragment : SettingsFragment() {
    override val preferencesResId = R.xml.pref_mydevice

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        super.onCreatePreferences(savedInstanceState, rootKey)

        val maintainerPref = findPreference<Preference>(MyDeviceSettings.maintainer) ?: return
        val tgGroupPref = findPreference<Preference>(MyDeviceSettings.tgGroup) ?: return
        val applyPresetsPref = findPreference<Preference>(MyDeviceSettings.presets) ?: return

        val nodes = synchronized(PresetDownloader.jsonLock) { PresetDownloader.matchedNodes }
        val deviceNode = nodes.last { it.has("device_name") }

        val deviceName = deviceNode.getString("device_name")
        val maintainer = if(deviceNode.has("maintainer")) deviceNode.getJSONObject("maintainer") else return
        val maintainerNick = maintainer.getString("name")
        val community = if(deviceNode.has("community")) deviceNode.getJSONObject("community") else return
        maintainerPref.title = "The maintainer of your $deviceName is $maintainerNick"
        if(maintainer.has("telegram")) {
            maintainerPref.setOnPreferenceClickListener {
                val i = Intent(Intent.ACTION_VIEW)
                i.data = Uri.parse(maintainer.getString("telegram"))
                activity.startActivity(i)
                true
            }
        }
        if(community.has("telegram")) {
            val url = community.getString("telegram")
            tgGroupPref.title = "Your device has a community Telegram group at $url"
            tgGroupPref.setOnPreferenceClickListener {
                val i = Intent(Intent.ACTION_VIEW)
                i.data = Uri.parse(community.getString("telegram"))
                activity.startActivity(i)
                true
            }
        } else {
            tgGroupPref.isVisible = false
        }
        applyPresetsPref.setOnPreferenceClickListener {
            PresetDownloader.forcePresets = true
            PresetDownloader.handler.post(PresetDownloader.applyPresets)
        }
    }
}
