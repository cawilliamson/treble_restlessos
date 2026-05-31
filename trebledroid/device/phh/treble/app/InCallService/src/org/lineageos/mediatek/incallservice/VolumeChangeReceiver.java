package org.lineageos.mediatek.incallservice;

import android.content.Intent;
import android.content.Context;
import android.content.BroadcastReceiver;

import android.media.AudioManager;
import android.media.AudioSystem;
import android.media.AudioDeviceInfo;

import android.util.Log;

public class VolumeChangeReceiver extends BroadcastReceiver {
    public static final String LOG_TAG = "MediatekInCallService";

    private AudioManager mAudioManager;

    public VolumeChangeReceiver(AudioManager audioManager) {
        mAudioManager = audioManager;
    }

    private void handleVolumeStateChange(Intent intent) {
        if (intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_TYPE, -1) == AudioManager.STREAM_VOICE_CALL) {
            AudioDeviceInfo callDevice = mAudioManager.getCommunicationDevice();

            // Try to get volumeIndex
            int volumeIndex = intent.getIntExtra(AudioManager.EXTRA_VOLUME_STREAM_VALUE, -1);
            if (volumeIndex < 0) {
                Log.w(LOG_TAG, "Could not get volumeIndex!");
                return;
            }

            GainUtils.setGainLevel(callDevice.getPort().type(), volumeIndex, AudioSystem.STREAM_VOICE_CALL);
        }
    }

    @Override
    public void onReceive(Context context, Intent intent) {
            handleVolumeStateChange(intent);
    }
}
