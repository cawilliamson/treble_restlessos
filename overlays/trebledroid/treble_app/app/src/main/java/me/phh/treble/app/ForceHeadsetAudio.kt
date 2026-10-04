package me.phh.treble.app

import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.media.AudioManager
import android.media.AudioSystem
import android.util.Log

import java.util.concurrent.locks.ReentrantLock
import kotlin.concurrent.withLock

class ForceHeadsetAudio(val audioManager: AudioManager) : EntryStartup, BroadcastReceiver() {
    private val lock = ReentrantLock(true)
    private var mode: Int = AudioManager.MODE_INVALID
    fun speaker() {
        lock.withLock {
            mode = audioManager.mode
                audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
                audioManager.isSpeakerphoneOn = true
                // Again. Who knows why, but otherwise it just stays in in-call speakerphone
                audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
                audioManager.mode = AudioManager.MODE_NORMAL
                audioManager.mode = mode
        }
    }

    fun headset() {
        lock.withLock {
            mode = audioManager.mode
                audioManager.isSpeakerphoneOn = false
                audioManager.mode = AudioManager.MODE_IN_COMMUNICATION
                audioManager.isSpeakerphoneOn = false
                audioManager.mode = AudioManager.MODE_NORMAL
                audioManager.isSpeakerphoneOn = false
                audioManager.mode = mode
        }
    }

    override fun onReceive(context: Context, intent: Intent) {
        if (intent.action != Intent.ACTION_HEADSET_PLUG)
            return
                val state = intent.getIntExtra("state", -1)
                when(state) {
                    AudioSystem.DEVICE_STATE_UNAVAILABLE -> speaker()
                    AudioSystem.DEVICE_STATE_AVAILABLE -> headset()
                    else ->
                        Log.e("PlugReceiver", "Unrecognised headset plug state!", Throwable())
                }
        // Apply the changes by setting the volume to the current volume
        // Fails in DND but to exit that you must change the volume later so meh.
        val volume = audioManager.getStreamVolume(AudioManager.STREAM_MUSIC)
        audioManager.setStreamVolume(AudioManager.STREAM_MUSIC, volume, 0)
    }

    override fun startup(ctxt: Context) {
        val filter = IntentFilter(Intent.ACTION_HEADSET_PLUG)
            val plugReceiver = this
            ctxt.registerReceiver(plugReceiver, filter)
            Log.d(tag, "Registered for headset plug")
    }

    fun shutdown(ctxt: Context) {
        ctxt.unregisterReceiver(this)
        Log.d(tag, "Unregistered for headset plug")
    }

    companion object : EntryStartup {
        const val tag = "ForceHeadsetAudio"
        private var self: ForceHeadsetAudio? = null
        override fun startup(ctxt: Context) {
            val audioManager = ctxt.getSystemService(AudioManager::class.java)
            self = ForceHeadsetAudio(audioManager)
            self!!.startup(ctxt)
        }
        fun shutdown(ctxt: Context) {
            self?.shutdown(ctxt)
            // Return to normal state if we were initialized earlier
            self?.speaker()
        }
    }
}
