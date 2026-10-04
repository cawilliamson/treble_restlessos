/*
 * Copyright (C) 2018-2020 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/*
 * Rewritten in Kotlin by Pavel Dubrova <pashadubrova@gmail.com>
 * AIDL support added by Andy Yan <geforce8800ultra@gmail.com>
 */

package me.phh.qcrilam

import android.app.Service
import android.content.Intent
import android.media.AudioManager
import android.os.IBinder
import android.os.RemoteException
import android.os.ServiceManager
import android.telephony.SubscriptionManager
import android.util.Log

private const val TAG = "QcRilAm-Service"
private var qcRilAudioResponse: vendor.qti.hardware.radio.am.IQcRilAudioResponse? = null

class QcRilAmService : Service() {
    private fun addCallbackForSimSlot(simSlotNo: Int, audioManager: AudioManager) {
        try {
            val qcRilAudioHidl1 = vendor.qti.hardware.radio.am.V1_0.IQcRilAudio.getService("slot$simSlotNo", true /*retry*/)
            if (qcRilAudioHidl1 == null) {
                Log.e(TAG, "Could not get HIDL service instance for slot$simSlotNo, failing")
            } else {
                qcRilAudioHidl1.setCallback(object : vendor.qti.hardware.radio.am.V1_0.IQcRilAudioCallback.Stub() {
                    override fun getParameters(keys: String?): String {
                        return audioManager.getParameters(keys)
                    }

                    override fun setParameters(keyValuePairs: String?): Int {
                        // AudioManager.setParameters does not check nor return
                        // the value coming from AudioSystem.setParameters.
                        // Assume there was no error:
                        audioManager.setParameters(keyValuePairs)
                        return 0
                    }
                })
            }
        } catch (e: Exception) {
            Log.e(TAG, "RemoteException while trying to add HIDL callback for slot$simSlotNo")
        }
        try {
            val qcRilAudioHidl2 = vendor.qti.qcril.am.V1_0.IQcRilAudio.getService("slot$simSlotNo", true /*retry*/)
            if (qcRilAudioHidl2 == null) {
                Log.e(TAG, "Could not get HIDL service instance for slot$simSlotNo, failing")
            } else {
                qcRilAudioHidl2.setCallback(object : vendor.qti.qcril.am.V1_0.IQcRilAudioCallback.Stub() {
                    override fun getParameters(keys: String?): String {
                        return audioManager.getParameters(keys)
                    }

                    override fun setParameters(keyValuePairs: String?): Int {
                        // AudioManager.setParameters does not check nor return
                        // the value coming from AudioSystem.setParameters.
                        // Assume there was no error:
                        audioManager.setParameters(keyValuePairs)
                        return 0
                    }
                })
            }
        } catch (e: Exception) {
            Log.e(TAG, "RemoteException while trying to add HIDL callback for slot$simSlotNo")
        }
        try {
            val serviceName = "vendor.qti.hardware.radio.am.IQcRilAudio/slot$simSlotNo"
            val qcRilAudioAidl = vendor.qti.hardware.radio.am.IQcRilAudio.Stub.asInterface(ServiceManager.getService(serviceName))
            if (qcRilAudioAidl == null) {
                Log.e(TAG, "Could not get AIDL service instance for slot$simSlotNo, failing")
            } else {
                qcRilAudioResponse = qcRilAudioAidl.setRequestInterface(object : vendor.qti.hardware.radio.am.IQcRilAudioRequest.Stub() {
                    override fun getInterfaceVersion() = vendor.qti.hardware.radio.am.IQcRilAudioRequest.VERSION

                    override fun getInterfaceHash() = vendor.qti.hardware.radio.am.IQcRilAudioRequest.HASH

                    override fun queryParameters(token: Int, params: String) {
                        qcRilAudioResponse?.queryParametersResponse(token, audioManager.getParameters(params))
                    }

                    override fun setParameters(token: Int, params: String) {
                        // AudioManager.setParameters does not check nor return
                        // the value coming from AudioSystem.setParameters.
                        // Assume there was no error:
                        audioManager.setParameters(params)
                        qcRilAudioResponse?.setParametersResponse(token, 0)
                    }
                })
            }
        } catch (e: Exception) {
            Log.e(TAG, "RemoteException while trying to add AIDL callback for slot$simSlotNo")
        }
    }

    override fun onBind(intent: Intent?) = null

    override fun onCreate() {
        val simCount = getSystemService(SubscriptionManager::class.java)?.getActiveSubscriptionInfoCountMax() ?:2
        Log.i(TAG, "Device has $simCount sim slots")
        val audioManager = getSystemService(AudioManager::class.java)
        if (audioManager == null) {
            throw RuntimeException("Can't get audiomanager!")
        }
        for (simSlotNo in 1..simCount) {
            addCallbackForSimSlot(simSlotNo, audioManager)
        }
    }
}
