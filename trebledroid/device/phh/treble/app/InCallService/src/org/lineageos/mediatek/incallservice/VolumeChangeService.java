package org.lineageos.mediatek.incallservice;

import android.media.AudioManager;

import android.telephony.TelephonyManager;
import android.telephony.TelephonyCallback;

import android.content.Intent;
import android.content.IntentFilter;
import android.content.Context;
import android.app.Service;
import android.os.IBinder;

import android.util.Log;

public class VolumeChangeService extends Service {
    public static final String LOG_TAG = "MediatekInCallService";

    private Context mContext;
    private VolumeChangeReceiver mVolumeChangeReceiver;
    private CallStateListener mCallStateListener;

    @Override
    public IBinder onBind(Intent intent) {
        return null;
    }

    @Override
    public void onDestroy() {
        super.onDestroy();
    }

    @Override
    public int onStartCommand(Intent intent, int flags, int startid) {
        mContext = this;

        AudioManager audioManager = (AudioManager) mContext.getSystemService(Context.AUDIO_SERVICE);
        TelephonyManager telephonyManager = (TelephonyManager) mContext.getSystemService(Context.TELEPHONY_SERVICE);
        mVolumeChangeReceiver = new VolumeChangeReceiver(audioManager);
        mCallStateListener = new CallStateListener(audioManager);

        Log.i(LOG_TAG, "Service is starting...");

        this.registerReceiver(mVolumeChangeReceiver,
                               new IntentFilter(AudioManager.VOLUME_CHANGED_ACTION));

        telephonyManager.registerTelephonyCallback(getMainExecutor(), mCallStateListener);

        // Restore gain levels on service start.
        GainUtils.setGainLevel(audioManager.getStreamVolume(AudioManager.STREAM_VOICE_CALL));

        return START_STICKY;
    }
}
