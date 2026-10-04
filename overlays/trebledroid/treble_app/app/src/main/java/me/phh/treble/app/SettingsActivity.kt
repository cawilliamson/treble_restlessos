package me.phh.treble.app

import android.content.Context
import android.content.Intent
import android.content.res.Configuration
import android.os.Bundle
import android.os.SystemProperties
import android.os.UserHandle
import android.preference.PreferenceActivity
import androidx.preference.ListPreference
import androidx.preference.Preference
import androidx.preference.PreferenceFragment
import androidx.preference.PreferenceManager

/**
 * A [PreferenceActivity] that presents a set of application settings. On
 * handset devices, settings are presented as a single list. On tablets,
 * settings are split by category, with category headers shown to the left of
 * the list of settings.
 *
 * See [Android Design: Settings](http://developer.android.com/design/patterns/settings.html)
 * for design guidelines and the [Settings API Guide](http://developer.android.com/guide/topics/ui/settings.html)
 * for more information on developing a Settings UI.
 */
class SettingsActivity : PreferenceActivity() {

    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)
        getApplicationContext().startServiceAsUser(Intent(getApplicationContext(), EntryService::class.java), UserHandle.SYSTEM)
        setupActionBar()
    }

    /**
     * Set up the [android.app.ActionBar], if the API is available.
     */
    private fun setupActionBar() {
        actionBar?.setDisplayHomeAsUpEnabled(true)
    }

    /**
     * {@inheritDoc}
     */
    override fun onIsMultiPane(): Boolean {
        return isXLargeTablet(this)
    }

    /**
     * {@inheritDoc}
     */
    override fun onBuildHeaders(target: MutableList<Header>) {
        loadHeadersFromResource(R.xml.pref_headers, target)
        if (!LenovoSettings.enabled())
            target.removeIf { it.fragment == LenovoSettingsFragment::class.java.name }
        if (!OnePlusSettings.enabled())
            target.removeIf { it.fragment == OnePlusSettingsFragment::class.java.name }
        if (!HuaweiSettings.enabled())
            target.removeIf { it.fragment == HuaweiSettingsFragment::class.java.name }
        if (!SamsungSettings.enabled())
            target.removeIf { it.fragment == SamsungSettingsFragment::class.java.name }
        if (!TranssionSettings.enabled())
            target.removeIf { it.fragment == TranssionSettingsFragment::class.java.name }
        if (!XiaomiSettings.enabled())
            target.removeIf { it.fragment == XiaomiSettingsFragment::class.java.name }
        if (!OppoSettings.enabled())
            target.removeIf { it.fragment == OppoSettingsFragment::class.java.name }
        if (!AsusSettings.enabled())
            target.removeIf { it.fragment == AsusSettingsFragment::class.java.name }
        if (!QualcommSettings.enabled())
            target.removeIf { it.fragment == QualcommSettingsFragment::class.java.name }
        if (!VsmartSettings.enabled())
            target.removeIf { it.fragment == VsmartSettingsFragment::class.java.name }
        if (!MyDeviceSettings.enabled())
            target.removeIf { it.fragment == MyDeviceSettingsFragment::class.java.name }
        if (!NubiaSettings.enabled())
            target.removeIf { it.fragment == NubiaSettingsFragment::class.java.name }
        if (!ImsSettings.enabled())
            target.removeIf { it.fragment == ImsSettingsFragment::class.java.name }
        if (!CustomSettings.enabled())
            target.removeIf { it.fragment == CustomSettingsFragment::class.java.name }
        val p = SystemProperties.get("ro.system.ota.json_url", "")
        if (p.trim() == "")
            target.removeIf { it.id.compareTo(R.id.updater) == 0 }
    }

    /**
     * This method stops fragment injection in malicious applications.
     * Make sure to deny any unknown fragments here.
     */
    override fun isValidFragment(fragmentName: String): Boolean {
        return PreferenceFragment::class.java.name == fragmentName
                || LenovoSettingsFragment::class.java.name == fragmentName
                || OnePlusSettingsFragment::class.java.name == fragmentName
                || DozeSettingsFragment::class.java.name == fragmentName
                || HuaweiSettingsFragment::class.java.name == fragmentName
                || MiscSettingsFragment::class.java.name == fragmentName
                || SamsungSettingsFragment::class.java.name == fragmentName
                || TranssionSettingsFragment::class.java.name == fragmentName
                || XiaomiSettingsFragment::class.java.name == fragmentName
                || OppoSettingsFragment::class.java.name == fragmentName
                || AsusSettingsFragment::class.java.name == fragmentName
                || QualcommSettingsFragment::class.java.name == fragmentName
                || VsmartSettingsFragment::class.java.name == fragmentName
                || MyDeviceSettingsFragment::class.java.name == fragmentName
                || NubiaSettingsFragment::class.java.name == fragmentName
                || ImsSettingsFragment::class.java.name == fragmentName
                || CustomSettingsFragment::class.java.name == fragmentName
                || AudioEffectsFragment::class.java.name == fragmentName
    }

    companion object {
        /**
         * A preference value change listener that updates the preference's summary
         * to reflect its new value.
         */
        private val sBindPreferenceSummaryToValueListener =
            Preference.OnPreferenceChangeListener { preference, value ->
                val stringValue = value.toString()

                preference.summary = if (preference is ListPreference) {
                    // For list preferences, look up the correct display value in the preference's
                    // 'entries' list.
                    val index = preference.findIndexOfValue(stringValue)
                    // Set the summary to reflect the new value.
                    if (index >= 0) preference.entries[index] else null
                } else {
                    // For all other preferences, set the summary to the value's simple string
                    // representation.
                    stringValue
                }
                true
            }

        /**
         * Helper method to determine if the device has an extra-large screen. For
         * example, 10" tablets are extra-large.
         */
        private fun isXLargeTablet(context: Context): Boolean {
            return context.resources.configuration.screenLayout and
                    Configuration.SCREENLAYOUT_SIZE_MASK >= Configuration.SCREENLAYOUT_SIZE_XLARGE
        }

        /**
         * Binds a preference's summary to its value. More specifically, when the
         * preference's value is changed, its summary (line of text below the
         * preference title) is updated to reflect the value. The summary is also
         * immediately updated upon calling this method. The exact display format is
         * dependent on the type of preference.

         * @see .sBindPreferenceSummaryToValueListener
         */
        fun bindPreferenceSummaryToValue(preference: Preference) {
            // Set the listener to watch for value changes.
            preference.onPreferenceChangeListener = sBindPreferenceSummaryToValueListener

            // Trigger the listener immediately with the preference's current value.
            sBindPreferenceSummaryToValueListener.onPreferenceChange(
                preference,
                PreferenceManager
                    .getDefaultSharedPreferences(preference.context)
                    .getString(preference.key, "")
            )
        }
    }
}
