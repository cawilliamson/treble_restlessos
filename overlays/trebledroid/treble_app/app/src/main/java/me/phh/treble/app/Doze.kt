package me.phh.treble.app

import android.content.Context
import android.content.Intent
import android.content.SharedPreferences
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.hardware.camera2.CameraAccessException
import android.hardware.camera2.CameraManager
import android.hardware.camera2.CameraManager.TorchCallback
import android.os.Handler
import android.os.HandlerThread
import android.os.Looper
import android.os.UserHandle
import android.os.VibrationEffect
import android.os.Vibrator
import android.preference.PreferenceManager
import android.util.Log
import java.util.*
import java.util.concurrent.LinkedBlockingQueue
import java.util.concurrent.TimeUnit

object Doze: EntryStartup {
    val dozeHandlerThread = HandlerThread("Doze Handler").also { it.start() }
    val dozeHandler = Handler(dozeHandlerThread.looper)

    class AccelerometerListener {
        val queue = LinkedBlockingQueue<FloatArray>()
        val cb = object: SensorEventListener {
            override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {
            }

            override fun onSensorChanged(event: SensorEvent) {
                queue.add(event.values)
                sensorManager.unregisterListener(this)
            }
        }

        fun read(): FloatArray {
            if(Looper.getMainLooper() == Looper.myLooper())
                throw Exception("This is a blocking function. Don't call me from main Looper.")
            sensorManager.registerListener(cb, accelerometerSensor, 1000)
            val result = queue.poll(1, TimeUnit.SECONDS)
            return result?: FloatArray(3, {0.0f})
        }

        fun isFaceUp(): Boolean {
            val v = read()
            val isUp = v[2] > 5.0f
            Log.d("PHH", "Got ${v[2]} $isUp")
            return isUp
        }
    }

    class Pocket {
        val NEAR = true
        val FAR = false

        //If events are closer than this, then guess this is a hand gesture
        val HANDWAVE_MAX_NS = 1000*1000*1000L // 1s
        //If events are more separated than this, then guess this is a pocket gesture
        val POCKET_MIN_NS = 60*1000*1000*1000L // 60s

        var state = FAR
        var lastEvent = -1L

        val maxRange = proximitySensor.maximumRange
        val threshold = if(maxRange >= 5.0f) 5.0f else maxRange

        fun update(event: SensorEvent) {
            Log.d("PHH", "Pocket got updated proximity to ${event.values[0]}")
            if(event.sensor != proximitySensor) return
            val newState = if(event.values[0] >= threshold) FAR else NEAR
            if(newState == state) return
            state = newState
            if(lastEvent == -1L) {
                lastEvent = event.timestamp
                return
            }

            //If we're far for the first time in a long time
            //Consider it a "pocket" event
            if(event.timestamp > (lastEvent + POCKET_MIN_NS)) {
                if(pocketEnabled && state == FAR) {
                    Log.d("PHH", "Got pocket event")
                    pulseDoze()
                }
            }

            //If we're far but we've been close not long enough
            //Consider it an "handwave" event
            if(event.timestamp < (lastEvent + HANDWAVE_MAX_NS)) {
                if(handwaveEnabled && state == FAR) {
                    Log.d("PHH", "Got handwave event")
                    dozeHandler.post {
                        if (accelerometer?.isFaceUp() == true)
                            pulseDoze()
                    }
                }
            }
            lastEvent = event.timestamp
        }
    }

    class ChopChop {
        fun trigger(){
            Log.d("PHH", "ChopChop got Triggered")

            try {
                cameraManager.setTorchMode(torchCameraId, !flashLightStatus)
                flashLightStatus = !flashLightStatus
                vibrator.vibrate(VibrationEffect.createOneShot(250, VibrationEffect.EFFECT_DOUBLE_CLICK))

                //save current Timestamp
                val timestamp = System.currentTimeMillis()
                flashLightChangedSinceWait = timestamp


                //wait a minute and turn of flashlight if its still on
                if(!flashLightStatus) return
                dozeHandler.postDelayed(fun(){
                    if(flashLightChangedSinceWait != timestamp) return

                    cameraManager.setTorchMode(torchCameraId, false)
                    flashLightStatus = false
                    vibrator.vibrate(VibrationEffect.createOneShot(1000, VibrationEffect.EFFECT_HEAVY_CLICK))

                },1000 * 60 * 10)
            } catch (e: CameraAccessException) {
                Log.d("PHH", "ChopChop couldn't toggle FlashLight", e)
            }
        }
    }

    var enabled: Boolean = false
    var handwaveEnabled = false
    var pocketEnabled = false
    var chopchopEnabled = false
    var flashLightStatus = false
    var flashLightChangedSinceWait = System.currentTimeMillis()
    var torchCameraId = ""
    lateinit var cameraManager: CameraManager
    lateinit var vibrator: Vibrator
    lateinit var sensorManager: SensorManager
    lateinit var proximitySensor: Sensor
    lateinit var accelerometerSensor: Sensor
    lateinit var chopchopSensor: Sensor
    var pocket: Pocket? = null
    var chopchop: ChopChop? = null
    var accelerometer: AccelerometerListener? = null

    fun updateState(handwave: Boolean, pocket: Boolean, chopchop: Boolean) {
        handwaveEnabled = handwave
        pocketEnabled = pocket
        chopchopEnabled = chopchop

        unregisterListeners()
        this.pocket = null
        this.chopchop = null

        if(!(handwave || pocket || chopchop)) return

        Log.d("PHH", "Starting Doze service")
        Log.d("PHH", "handwave: $handwave pocket: $pocket chopchop $chopchop")

        if(pocket || handwave){
            this.pocket = Pocket()
            registerPocketListeners()
        }

        if(chopchop){
            this.chopchop = ChopChop()
            registerChopchopListeners()
        }

    }

    val pocketSensorListener = object: SensorEventListener {
        override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
        }

        override fun onSensorChanged(event: SensorEvent) {
            pocket?.update(event)
        }
    }

    val chopchopSensorListener = object : SensorEventListener {

        override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {
        }

        override fun onSensorChanged(event: SensorEvent) {
            chopchop?.trigger()
        }
    }

    val torchStateListener = object : TorchCallback() {
        override fun onTorchModeChanged(cameraId: String, enabled: Boolean) {
            super.onTorchModeChanged(cameraId, enabled)
            flashLightStatus = if(cameraId == torchCameraId) enabled else flashLightStatus
        }
    }

    fun registerPocketListeners() {
        //Request for once every 1000s, we only want updates anyway
        sensorManager.registerListener(pocketSensorListener, proximitySensor, 1000*1000*1000)
    }
    fun registerChopchopListeners() {
        //Request update every 100ms should work just fine
        sensorManager.registerListener(chopchopSensorListener, chopchopSensor, 1000*100)
        cameraManager.registerTorchCallback(torchStateListener, dozeHandler)
    }

    fun unregisterListeners() {
        //Pocket
        sensorManager.unregisterListener(pocketSensorListener)
        //ChopChop
        sensorManager.unregisterListener(chopchopSensorListener)
        cameraManager.unregisterTorchCallback(torchStateListener)
    }

    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        when(key) {
            DozeSettings.handwaveKey, DozeSettings.pocketKey, DozeSettings.chopchopkey -> {
                updateState(
                        sp.getBoolean(DozeSettings.handwaveKey, false),
                        sp.getBoolean(DozeSettings.pocketKey, false),
                        sp.getBoolean(DozeSettings.chopchopkey, false)
                )
            }
        }
    }

    override fun startup(ctxt: Context) {
        Log.d("PHH", "Starting Doze service")
        sensorManager = ctxt.getSystemService(SensorManager::class.java)
        proximitySensor = sensorManager.getDefaultSensor(Sensor.TYPE_PROXIMITY, true)
        accelerometerSensor = sensorManager.getDefaultSensor(Sensor.TYPE_ACCELEROMETER, false)
        cameraManager = ctxt.getSystemService(Context.CAMERA_SERVICE) as CameraManager

        try {
            chopchopSensor = Doze.sensorManager.getSensorList(Sensor.TYPE_ALL).first { it.stringType == "com.motorola.sensor.chopchop" }
            Log.d("PHH", "Found ChopChop Sensor, Initalizing needed Services")
            torchCameraId = cameraManager.cameraIdList[0]
            vibrator = ctxt.getSystemService(Context.VIBRATOR_SERVICE) as Vibrator

        } catch (e: Exception){}



        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        sp.registerOnSharedPreferenceChangeListener(spListener)

        //Refresh parameters on boot
        spListener.onSharedPreferenceChanged(sp, DozeSettings.handwaveKey)
        spListener.onSharedPreferenceChanged(sp, DozeSettings.pocketKey)
        spListener.onSharedPreferenceChanged(sp, DozeSettings.chopchopkey)
        accelerometer = AccelerometerListener()
    }

    fun pulseDoze() {
        EntryService.service?.sendBroadcastAsUser(Intent("com.android.systemui.doze.pulse"), UserHandle.ALL)
    }
}
