package me.phh.treble.app

import android.app.Activity
import android.hardware.*
import android.os.Bundle
import android.util.Log
import android.view.View
import android.view.ViewGroup
import android.widget.*

class DebugSensors : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {
        super.onCreate(savedInstanceState)

        val sm = getSystemService(SensorManager::class.java)
        val sensors = sm.getSensorList(Sensor.TYPE_ALL)

        val layout = LinearLayout(this)
        val choser = Spinner(this)
        val textView = TextView(this)

        var requestedSensorListener = false
        val sensorListener = object: SensorEventListener {
            override fun onAccuracyChanged(sensor: Sensor, accuracy: Int) {

            }

            override fun onSensorChanged(event: SensorEvent) {
                Log.d("PHH", "Got event ${event.values.toList()}")
                textView.text = "${textView.text}\n${event.values.toList()}"
            }
        }

        val triggerListener = object: TriggerEventListener() {
            override fun onTrigger(event: TriggerEvent) {
                Log.d("PHH", "Received trigger event ${event.sensor} ${event.values.toList()}")
            }
        }

        choser.adapter = ArrayAdapter(this,  android.R.layout.simple_spinner_item, sensors.map { it.name })
        choser.onItemSelectedListener = object: AdapterView.OnItemSelectedListener {
            var sensor: Sensor? = null
            override fun onItemSelected(parent: AdapterView<*>?, view: View?, position: Int, id: Long) {
                Log.d("PHH", "Selected item ${id} ${sensors[id.toInt()].name}")
                if(requestedSensorListener) {
                    sm.unregisterListener(sensorListener)
                }
                sensor = sensors[id.toInt()]

                textView.text = ""
                textView.text = sensor!!.name + "\n"
                Log.d("PHH", "Sensor reporting mode is ${sensor?.reportingMode}")

                if(sensor?.reportingMode == Sensor.REPORTING_MODE_ONE_SHOT || sensor?.reportingMode == Sensor.REPORTING_MODE_SPECIAL_TRIGGER) {
                    sm.requestTriggerSensor(triggerListener, sensor)
                } else {
                    sm.registerListener(sensorListener, sensor, 1000 * 1000)
                    requestedSensorListener = true
                }

            }

            override fun onNothingSelected(parent: AdapterView<*>?) {
            }
        }
        layout.addView(choser, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.WRAP_CONTENT))
        layout.addView(textView, LinearLayout.LayoutParams(LinearLayout.LayoutParams.MATCH_PARENT, LinearLayout.LayoutParams.MATCH_PARENT))
        addContentView(layout,
                ViewGroup.LayoutParams(ViewGroup.LayoutParams.MATCH_PARENT, ViewGroup.LayoutParams.MATCH_PARENT)
        )
    }

    override fun onResume() {
        super.onResume()
    }
}