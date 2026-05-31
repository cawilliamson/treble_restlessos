package me.phh.treble.app

import android.content.Context
import android.media.AudioManager
import android.os.SystemProperties

object Tools {
    lateinit var audioManager: AudioManager
    val vendorFp = SystemProperties.get("ro.vendor.build.fingerprint")
    val vendorFpLow = vendorFp.toLowerCase()
    val deviceId = SystemProperties.get("ro.build.overlay.deviceid")

    fun startup(ctxt: Context) {
        audioManager = ctxt.getSystemService(AudioManager::class.java)
    }
}