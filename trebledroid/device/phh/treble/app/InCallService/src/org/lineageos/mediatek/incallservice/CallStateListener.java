/*
 * Copyright (C) 2023 The LineageOS Project
 *
 * SPDX-License-Identifier: Apache-2.0
 */

package org.lineageos.mediatek.incallservice;

import android.media.AudioManager;

import android.telephony.TelephonyManager;
import android.telephony.TelephonyCallback;

import android.util.Log;

public class CallStateListener extends TelephonyCallback implements TelephonyCallback.CallStateListener {
    private static final String LOG_TAG = "MtkInCallService";
    private AudioManager mAudioManager;

    public CallStateListener(AudioManager audioManager) {
        mAudioManager = audioManager;
    }

    @Override
    public void onCallStateChanged(int state) {
        if (state == TelephonyManager.CALL_STATE_OFFHOOK || state == TelephonyManager.CALL_STATE_RINGING) {
            Log.d(LOG_TAG, "CallStateListener: CALL_STATE_OFFHOOK, setting gain.");
            GainUtils.setGainLevel(mAudioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL));
        }
    }
}
