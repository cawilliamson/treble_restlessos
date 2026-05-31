package me.phh.treble.app

import android.content.Context
import android.hardware.*
import android.os.Build
import android.os.IBinder
import android.os.PowerManager
import android.os.SystemClock
import android.os.UEventObserver
import android.util.Log
import android.view.SurfaceControl
import androidx.annotation.RequiresApi


object Lid: EntryStartup {
    val globalRef = mutableListOf<Any>()
    fun waky(ctxt: Context) {
        Log.d("PHH", "Lid Waking up")

        val powerManager = ctxt.getSystemService(PowerManager::class.java)
        PowerManager::class.java.getMethod("wakeUp", Long::class.java).invoke(powerManager, SystemClock.uptimeMillis())
    }

    fun sleepy(ctxt: Context) {
        Log.d("PHH", "Lid Sleeping")
        val powerManager = ctxt.getSystemService(PowerManager::class.java)
        PowerManager::class.java.getMethod("goToSleep", Long::class.java).invoke(powerManager, SystemClock.uptimeMillis())
    }


    fun huawei(ctxt: Context) {
        val sensorManager = ctxt.getSystemService(SensorManager::class.java)
        // Huawei Hall sensor for magnetic cover : type = 65538 - name = "HALL sensor"
        // find the first item or return null
        // type=65538 is the Huawei type for magnetic sensor (found in the EMUI9 frameworks)
        val lidSensor = sensorManager.getSensorList(Sensor.TYPE_ALL).find { it.stringType == "android.sensor.hall" }
        if(lidSensor == null) {
            Log.d("PHH", "Failed finding sensor for lid wakeup")
            for(s in sensorManager.getSensorList(Sensor.TYPE_ALL)) {
                Log.d("PHH", "Sensor name : '${s.name}' - type : '${s.type}'")
            }
            return
        }
        Log.d("PHH", "Found lid sensor $lidSensor")

        sensorManager.registerListener(object: SensorEventListener {
            override fun onSensorChanged(p0: SensorEvent) {
                Log.d("PHH", "Received LID event $p0")
                Log.d("PHH", "Lid value is ${p0.values[0]}, ${p0.values[0] == 0.0f}, ${p0.values[0] == 1.0f}")

                if(p0.values[0] == 0.0f) { // Lid is opening, wakeup
                    waky(ctxt)
                } else if(p0.values[0] == 1.0f) { // Lid is closing, sleeping
                    sleepy(ctxt)
                }
            }

            override fun onAccuracyChanged(p0: Sensor?, p1: Int) {
            }
        }, lidSensor, 1000*1000)
    }
    fun lenovo(ctxt: Context) {
        val sensorManager = ctxt.getSystemService(SensorManager::class.java)
        val sensors = listOf(
		"ah1902 Hall Effect Sensor Wakeup",
		"bu52053nvx Hall Effect Sensor Wakeup",
		"mmc56x3x Hall Effect Sensor Wakeup",
		"qmc6308 Hall Effect Sensor Wakeup",
		"qmc630x Hall Effect Sensor Wakeup",
	)
        val lidSensor = sensorManager.getSensorList(Sensor.TYPE_ALL).firstOrNull() { sensor -> sensors.any { name -> sensor.name.contains(name)}}
        if(lidSensor == null) {
            Log.d("PHH", "Failed finding sensor for lid wakeup")
            for(s in sensorManager.getSensorList(Sensor.TYPE_ALL)) {
                Log.d("PHH", " - '${s.name}'")
            }
            return
        }
        Log.d("PHH", "Found lid sensor $lidSensor")

        sensorManager.registerListener(object: SensorEventListener {
            override fun onSensorChanged(p0: SensorEvent) {
                Log.d("PHH", "Received LID event $p0")
                Log.d("PHH", "Lid value is ${p0.values[0]}, ${p0.values[0] == 0.0f}, ${p0.values[0] == 1.0f}")

                if(p0.values[0] == 1.0f) { // Lid is opening, wakeup
                    waky(ctxt)
                } else if(p0.values[0] == 0.0f) { // Lid is closing, sleeping
                    sleepy(ctxt)
                }
            }

            override fun onAccuracyChanged(p0: Sensor?, p1: Int) {
            }
        }, lidSensor, 1000*1000)
    }

    fun cat(ctxt: Context) {
        Log.d("PHH", "Got a cat S22, observing uevent")
        val observer = object : UEventObserver() {
            @RequiresApi(Build.VERSION_CODES.Q)
            override fun onUEvent(event: UEventObserver.UEvent) {
                try {
                    Log.v("PHH", "Cat S22 Flip event: $event")
                    val state = event.get("SWITCH_STATE")
                    val closed = state == "0"
                    Log.v("PHH", "Closed = $closed")

                    if(closed) {
                        sleepy(ctxt)
                    } else {
                        waky(ctxt)
                        val displayToken =
                            SurfaceControl::class.java.getMethod("getPhysicalDisplayToken", Long::class.java)
                                .invoke(null, 1)
                        SurfaceControl::class.java.getMethod("setDisplayPowerMode", IBinder::class.java, Int::class.java).invoke(null, displayToken, 0)
                    }
                } catch (e: Exception) {
                    Log.d("PHH", "Failed parsing uevent", e)
                }
            }
        }
        globalRef.add(observer)
        observer.startObserving("/devices/virtual/switch_hall/hall_switch")
    }

    override fun startup(ctxt: Context) {
        Log.d("PHH", "LID vendorFpLow = " + Tools.vendorFpLow)
        if(Tools.vendorFpLow.startsWith("Lenovo/TB-9707F_PRC/TB-9707F".lowercase()) ||
            Tools.vendorFpLow.startsWith("Lenovo/LenovoTB-J716F_PRC/J716F".lowercase()) ||
            Tools.vendorFpLow.startsWith("Lenovo/TB320FC".lowercase()) ||
            Tools.vendorFpLow.startsWith("Lenovo/TB321FU".lowercase()) ||
            Tools.vendorFpLow.startsWith("Lenovo/TB322FC".lowercase())
        ) {
            lenovo(ctxt)
        }
        if(Tools.vendorFpLow.startsWith("Cat/S22FLIP/S22FLIP".lowercase())) {
            cat(ctxt)
        }
        if(Tools.vendorFpLow.startsWith("hi6250".lowercase()) ||
            Tools.vendorFpLow.startsWith("kirin710".lowercase()) ||
            Tools.vendorFpLow.startsWith("kirin970".lowercase()) ||
            Tools.vendorFpLow.startsWith("hi3660".lowercase())
            ) {
            huawei(ctxt)
        }
    }
}
