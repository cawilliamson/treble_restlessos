package me.phh.treble.app

import android.content.Context
import android.content.SharedPreferences
import android.media.AudioAttributes
import android.media.audiofx.AudioEffect
import android.os.Bundle
import androidx.preference.Preference
import androidx.preference.PreferenceFragment
import androidx.preference.SwitchPreference
import java.util.UUID

class AudioEffectsFragment : PreferenceFragment() {
    val effects = AudioEffect.queryEffects()

    override fun onCreatePreferences(savedInstanceState: Bundle?, rootKey: String?) {
        // First create preferencemanager instance
        preferenceScreen = preferenceManager.createPreferenceScreen(context)

        preferenceScreen.addPreference(Preference(context).apply {
            title = "Restart media app to apply change"
            summary = "Currently effects are applied exclusively on media output"
        })

        // List the available audio effects
        effects.forEach {
            // I think only INSERT effects are supported, but idk
            val isSupported =
                it.connectMode == AudioEffect.EFFECT_INSERT
            android.util.Log.d("PHH", "Effect ${it.name} is supported: $isSupported, connectMode ${it.connectMode}")

            if (!isSupported) return@forEach
            val pref = SwitchPreference(context)
            pref.title = it.name
            pref.key = "audio_effect_" + it.uuid.toString()
            pref.summary = "By ${it.implementor}"
            preferenceScreen.addPreference(pref)
        }
    }
}

object AudioEffects: SharedPreferences.OnSharedPreferenceChangeListener {
    // map of audio uuid => to-be-introspected android.media.audiofx.DefaultEffect
    val takenEffects = mutableMapOf<UUID, Object>()
    val effects = AudioEffect.queryEffects()
    val effectNull = AudioEffect::class.java.getField("EFFECT_TYPE_NULL").get(null) as UUID

    fun startup(context: Context) {
        val sp = android.preference.PreferenceManager.getDefaultSharedPreferences(context)
        val effects = AudioEffect.queryEffects()
        effects.forEach {
            //Refresh parameters on boot
            onSharedPreferenceChanged(sp, "audio_effect_" + it.uuid.toString())
        }
        sp.registerOnSharedPreferenceChangeListener(this)
    }

    override fun onSharedPreferenceChanged(sp: SharedPreferences, pref: String) {
        android.util.Log.e("PHH", "Clicked on preference ${pref}")
        val effect = effects.find { "audio_effect_" + it.uuid.toString() == pref }
        if (effect == null) {
            android.util.Log.d("PHH", "No effect found for key ${pref}")
            return
        }

        val enabled = sp.getBoolean(pref, false)
        if(enabled) {
            android.util.Log.e("PHH", "Creating effect ${effect.uuid} ${effect.name}")
            // NB: Stream vs Source effects, but that's for later
            // Prototype: UUID type, UUID uuid, int priority, int streamUsage
            val o = Class.forName("android.media.audiofx.StreamDefaultEffect")
                .getConstructor(UUID::class.java, UUID::class.java, Int::class.java, Int::class.java)
                .newInstance(effectNull, effect.uuid, 0, AudioAttributes.USAGE_MEDIA) as Object
            takenEffects[effect.uuid] = o
            android.util.Log.e("PHH", "Succeeded")
        } else {
            val o = takenEffects[effect.uuid]
            if (o == null) {
                android.util.Log.e("PHH", "No taken effect found for key ${effect.uuid}")
                return
            }
            o.javaClass.getMethod("release").invoke(o)
            takenEffects.remove(effect.uuid)
        }
    }
}