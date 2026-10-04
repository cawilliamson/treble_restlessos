package me.phh.treble.app

import android.annotation.SuppressLint
import android.content.Context
import android.content.SharedPreferences
import android.hardware.display.DisplayManager
import android.os.Parcel
import android.os.ServiceManager
import android.os.SystemProperties
import android.preference.PreferenceManager
import android.provider.Settings
import android.util.Log
import java.lang.ref.WeakReference

import vendor.ims.zenmotion.V1_0.IZenMotion;
import vendor.xiaomi.hw.touchfeature.ITouchFeature
import vendor.mediatek.hardware.agolddaemon.IAgoldDaemon

@SuppressLint("StaticFieldLeak")
object Misc: EntryStartup {
    fun safeSetprop(key: String, value: String?) {
        try {
            Log.d("PHH", "Setting property $key to $value")
            SystemProperties.set(key, value)
        } catch (e: Exception) {
            Log.d("PHH", "Failed setting prop $key", e)
        }
    }

    val surfaceFlinger = ServiceManager.getService("SurfaceFlinger")
    fun forceFps(v: Int, c: Boolean) {
        val data = Parcel.obtain()
        try {
            data.writeInterfaceToken("android.ui.ISurfaceComposer")
            data.writeInt(v)
            surfaceFlinger.transact(1035, data, null, 0)
            Log.d("PHH", "Set surface flinger forced fps/mode to supportedModes[$v]")
            if (c) {
                Log.d("PHH", "Resolution changed, attempting to restart SystemUI")
                var cmds = listOf(
                    arrayOf("/sbin/su", "-c", "/system/bin/killall com.android.systemui"),
                    arrayOf("/system/xbin/su", "-c", "/system/bin/killall com.android.systemui"),
                    arrayOf("/system/xbin/phh-su", "-c", "/system/bin/killall com.android.systemui"),
                    arrayOf("/sbin/su", "0", "/system/bin/killall com.android.systemui"),
                    arrayOf("/system/xbin/su", "0", "/system/bin/killall com.android.systemui"),
                    arrayOf("/system/xbin/phh-su", "0", "/system/bin/killall com.android.systemui")
                )
                for (cmd in cmds) {
                    try {
                        Runtime.getRuntime().exec(cmd).waitFor()
                        break
                    } catch (t: Throwable) {
                        Log.d("PHH", "Failed to exec \"" + cmd.joinToString(separator = " ") + "\", skipping")
                    }
                }
            }
        } catch (r: Exception) {
            Log.d("PHH", "Failed setting surface flinger forced fps/mode to supportedModes[$v]")
        } finally {
            data.recycle()
        }
    }

    fun enableHwcOverlay(v: Boolean) {
        val data = Parcel.obtain()
        try {
            data.writeInterfaceToken("android.ui.ISurfaceComposer")
            data.writeInt(if(v) 0 else 1)
            surfaceFlinger.transact(1008, data, null, 0)
            Log.d("PHH", "Set surface flinger hwc overlay to $v")
        } catch (r: Exception) {
            Log.d("PHH", "Failed setting surface flinger hwc overlay to $v")
        } finally {
            data.recycle()
        }
    }

    lateinit var ctxt: WeakReference<Context>
    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        val c = ctxt.get()
        if(c == null) return@OnSharedPreferenceChangeListener
        val displayManager = c.getSystemService(DisplayManager::class.java)
        when(key) {
            MiscSettings.mobileSignal -> {
                val value = sp.getString(key, "default")
                SystemProperties.set("persist.sys.signal.level", value)
                Log.d("PHH", "Setting signal level method to $value")
            }
            MiscSettings.cameraTimestampOverride -> {
                val value = sp.getString(key, "-1")
                Log.d("PHH", "Setting cameraTimestampOverride to $value")
                SystemProperties.set("persist.sys.phh.camera.force_timestampsource", value)
            }
            MiscSettings.maxAspectRatioPreO -> {
                val value = sp.getString(key, "1.86")
                SystemProperties.set("persist.sys.max_aspect_ratio.pre_o", value)
                Log.d("PHH", "Setting max aspect ratio for pre-o app $value")
            }
            MiscSettings.multiCameras -> {
                val value = sp.getBoolean(key, false)

                safeSetprop("persist.sys.phh.include_all_cameras", if(value) "true" else "false")
                if (value ||
                        SystemProperties.get("vendor.camera.aux.packagelist", null) == null ||
                        SystemProperties.get("camera.aux.packagelist", null) == null) {
                    safeSetprop("vendor.camera.aux.packagelist", if (value) "nothing" else null)
                    safeSetprop("camera.aux.packagelist", if (value) "nothing" else null)
                    safeSetprop("ctl.restart", "vendor.camera-provider-2-4")
                    safeSetprop("ctl.restart", "camera-provider-2-4")
                    safeSetprop("ctl.restart", "cameraserver")
                }
            }
            MiscSettings.forceCamera2APIHAL3 -> {
                val value = sp.getBoolean(key, false)
                val defValue = "0"
                val newValue = if (value) "1" else defValue

                if (value ||
                        SystemProperties.get("persist.vendor.camera.HAL3.enabled", defValue) != newValue ||
                        SystemProperties.get("persist.vendor.camera.eis.enable", defValue) != newValue) {
                    safeSetprop("persist.vendor.camera.HAL3.enabled", newValue)
                    safeSetprop("persist.vendor.camera.eis.enable", newValue)
                    Log.d("PHH", "forced Camera2API HAL3 to $value")
                    // Restart services
                    safeSetprop("ctl.restart", "vendor.camera-provider-2-4")
                    safeSetprop("ctl.restart", "camera-provider-2-4")
                }
            }
            MiscSettings.headsetFix -> {
                val value = sp.getBoolean(key, HuaweiSettings.enabled())
                if (!sp.contains(key))
                    Log.d("PHH", "Setting Huawei headset fix to $value")
                if (value) {
                    Log.d("PHH", "starting huaweiaudio")
                    ForceHeadsetAudio.startup(c)
                } else {
                    Log.d("PHH", "stopping huaweiaudio")
                    ForceHeadsetAudio.shutdown(c)
                }
            }
            MiscSettings.roundedCorners -> {
                val value = sp.getString(key, "-1").toInt()
                if (value >= 0) {
                    Settings.Secure.putInt(c.contentResolver, "sysui_rounded_content_padding", value)
                    SystemProperties.set("persist.sys.phh.rounded_corners_padding", value.toString())
                }
            }
            MiscSettings.roundedCornersOverlay -> {
                val value = sp.getString(key, "-1").toFloat()
                if (value >= 0) {
                    Settings.Secure.putFloat(c.contentResolver, "sysui_rounded_size", value)
                }
            }
            MiscSettings.disableButtonsBacklight -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.disable_buttons_light", if (value) "true" else "false")
            }
            MiscSettings.forceNavbarOff -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.mainkeys", if (value) "1" else "0")
            }
            MiscSettings.bluetooth -> {
                val value = sp.getString(key, "none")
                android.util.Log.d("PHH", "Setting bluetooth workaround to $value")
                when (value) {
                    "none" -> {
                        SystemProperties.set("persist.sys.bt.unsupported.commands", "")
                        SystemProperties.set("persist.sys.bt.unsupported.ogfeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.lefeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.states", "")
                    }
                    "mediatek" -> {
                        // 182 - READ_DEFAULT_ERRONEOUS_DATA_REPORTING
                        SystemProperties.set("persist.sys.bt.unsupported.commands", "182")
                        SystemProperties.set("persist.sys.bt.unsupported.ogfeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.lefeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.states", "")
                    }
                    "huawei" -> {
                        // 182 - READ_DEFAULT_ERRONEOUS_DATA_REPORTING
                        SystemProperties.set("persist.sys.bt.unsupported.commands", "182")
                        SystemProperties.set("persist.sys.bt.unsupported.ogfeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.lefeatures", "")
                        SystemProperties.set("persist.sys.bt.unsupported.states", "")
                    }
                }
            }
            MiscSettings.displayFps -> {
                val thisModeIndex = sp.getString(key, "-1").toInt()
                val displayInfo = displayManager.displays[0]
                if (thisModeIndex < 0 || thisModeIndex >= displayInfo.supportedModes.size) {
                    Log.d("PHH", "Trying to set impossible supportedModes[$thisModeIndex]")
                } else {
                    Log.d("PHH", "Trying to set supportedModes[$thisModeIndex]")
                    val lastMode = displayInfo.getMode()
                    var lastModeIndex = displayInfo.supportedModes.indexOf(lastMode)
                    val thisMode = displayInfo.supportedModes[thisModeIndex]
                    Log.d("PHH", "\tlastMode = supportedModes[$lastModeIndex] = $lastMode")
                    Log.d("PHH", "\tthisMode = supportedModes[$thisModeIndex] = $thisMode")
                    forceFps(thisModeIndex, (thisMode.getPhysicalWidth() != lastMode.getPhysicalWidth())
                        || (thisMode.getPhysicalHeight() != lastMode.getPhysicalHeight()))
                }
            }
            MiscSettings.dynamicFps -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.dynamic_fps", if (value) "1" else "0")
            }
            MiscSettings.remotectl -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.remote", if (value) "true" else "false")
            }
            MiscSettings.disableAudioEffects -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.disable_audio_effects", if (value) "1" else "0")
            }
            MiscSettings.disableFastAudio -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.disable_fast_audio", if (value) "1" else "0")
            }
            MiscSettings.sysbta -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.bluetooth.system_audio_hal.enabled", if (value) "true" else "false")
            }
            MiscSettings.noHwcomposer -> {
                val value = sp.getBoolean(key, false)
                enableHwcOverlay(!value)
            }
            MiscSettings.storageFUSE -> {
                val value = sp.getBoolean(key, false)
                Log.d("PHH", "Setting storageFUSE to $value")
                SystemProperties.set("persist.sys.fflag.override.settings_fuse", if (!value) "true" else "false")
            }
            MiscSettings.backlightScale -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.backlight.scale", if (value) "1" else "0")
            }
            MiscSettings.headsetDevinput -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.overlay.devinputjack", if (value) "true" else "false")
            }
            MiscSettings.restartRil -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.restart_ril", if (value) "true" else "false")
            }
            MiscSettings.patchSmsc -> {
                val value = sp.getBoolean(key, true)
                SystemProperties.set("persist.sys.phh.patch_smsc", if (value) "true" else "false")
            }
            MiscSettings.minimalBrightness -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.overlay.minimal_brightness", if (value) "true" else "false")
            }
            MiscSettings.aod -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.overlay.aod", if (value) "true" else "false")
                OverlayPicker.setOverlayEnabled("me.phh.treble.overlay.misc.aod_systemui", value)
            }
            MiscSettings.dt2w -> {
                // Asus dt2w
                val value = sp.getBoolean(key, false)
                val asusSvc = try { IZenMotion.getService() } catch(e: Exception) { null }
                if(asusSvc != null) {
                    asusSvc.setDclickEnable(if(value) 1 else 0)
                }

                // Xiaomi dt2w
                Misc.safeSetprop("persist.sys.phh.xiaomi.dt2w", if(value) "1" else "0")
                try {
                    val binder = android.os.Binder.allowBlocking(
                        ServiceManager.waitForDeclaredService(ITouchFeature.DESCRIPTOR + "/default"));
                    val instance = ITouchFeature.Stub.asInterface(binder);
                    val ret = instance.set_mode_value(0 /*touchid*/, 14 /* TOUCH_DOUBLETAP_MODE */, if(value) 1 else 0)
                    Log.d("PHH", "Setting xiaomi touch mode returned $ret")
                } catch(t: Throwable) {
                    Log.d("PHH", "Setting xiaomi touch mode failed", t)
                }

                // Agold//unihertz dt2w
                try {
                    val binder = android.os.Binder.allowBlocking(
                        ServiceManager.waitForDeclaredService(IAgoldDaemon.DESCRIPTOR + "/default"));
                    val instance = IAgoldDaemon.Stub.asInterface(binder);
                    val ret = instance.SendMessageToIoctl(100, 0, if(value) 1 else 0, if(value) 1 else 0)
                    Log.d("PHH", "Setting agold touch mode returned $ret")
                } catch(t: Throwable) {
                    Log.d("PHH", "Setting agold touch mode failed", t)
                }
            }
            MiscSettings.fodColor -> {
                val value = sp.getString(key, "00ff00")
                SystemProperties.set("persist.sys.phh.fod_color", value)
            }
            MiscSettings.mtkTouchHintIsRotate -> {
                val value = sp.getBoolean(key, false)
                if(value) {
                    SystemProperties.set("persist.sys.phh.touch_hint", "rotate")
                } else {
                    SystemProperties.set("persist.sys.phh.touch_hint", "touch")
                }
            }
            MiscSettings.allowBinderThread -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.allow_binder_thread_on_incoming_calls", if(value) "1" else "0")
            }
            MiscSettings.lowGammaBrightness -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.low_gamma_brightness", if (value) "true" else "false")
            }
            MiscSettings.forceDisplay5g -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.force_display_5g", if(value) "1" else "0")
            }
            MiscSettings.linearBrightness -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.linear_brightness", if(value) "1" else "0")
            }
            MiscSettings.disableVoiceCallIn -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.disable_voice_call_in", if (value) "true" else "false")
            }
            MiscSettings.mtkGedKpi -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.mtk_ged_kpi", if (value) "1" else "0")
            }
            MiscSettings.disableSfGlBackpressure -> {
                val value = sp.getBoolean(key, false)
                // Note: Reversed value because the prop is enabling
                SystemProperties.set("persist.sys.phh.enable_sf_gl_backpressure", if (value) "0" else "1")
            }
            MiscSettings.disableSfHwcBackpressure -> {
                val value = sp.getBoolean(key, false)
                // Note: Reversed value because the prop is enabling
                SystemProperties.set("persist.sys.phh.enable_sf_hwc_backpressure", if (value) "0" else "1")
            }
            MiscSettings.disableSaeUpgrade -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.wifi_disable_sae", if (value) "true" else "false")
            }
            MiscSettings.escoTransportUnitSize -> {
                val value = sp.getString(key, "0")
                SystemProperties.set("persist.sys.bt.esco_transport_unit_size", value)
            }
            MiscSettings.secureAdb -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.adb_secure", if (value) "1" else "0")
            }
            MiscSettings.treatVirtualSensorsAsReal -> {
                val value = sp.getBoolean(key, false)
                SystemProperties.set("persist.sys.phh.virtual_sensors_are_real", if (value) "1" else "0")
            }
        }
    }

    override fun startup(ctxt: Context) {
        if (!MiscSettings.enabled()) return

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        this.ctxt = WeakReference(ctxt.applicationContext)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, MiscSettings.cameraTimestampOverride)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.mobileSignal)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.maxAspectRatioPreO)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.multiCameras)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.forceCamera2APIHAL3)
        if (! sp.contains(MiscSettings.headsetFix))
            sp.edit().putBoolean(MiscSettings.headsetFix, HuaweiSettings.enabled()).commit()
        sp.edit().putBoolean(MiscSettings.sysbta, SystemProperties.getBoolean("persist.bluetooth.system_audio_hal.enabled", false)).apply()
        spListener.onSharedPreferenceChanged(sp, MiscSettings.headsetFix)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.bluetooth)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.displayFps)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.noHwcomposer)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.storageFUSE)
        spListener.onSharedPreferenceChanged(sp, MiscSettings.dt2w)
    }
}
