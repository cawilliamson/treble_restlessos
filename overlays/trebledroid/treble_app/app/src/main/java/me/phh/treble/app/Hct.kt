package me.phh.treble.app

import android.content.Context
import android.os.PowerManager
import android.os.SystemClock
import android.util.Log
import vendor.hct.hardware.gesturewake.V1_0.*

class Hct: EntryStartup {
    companion object: EntryStartup {
        private var self: EntryStartup? = null
        override fun startup(ctxt: Context) {
            self = Hct()
            self!!.startup(ctxt)
        }

    }

    var svc: IGestureWake? = null
    override fun startup(ctxt: Context) {
        svc = try { IGestureWake.getService() } catch(t: Throwable) {
            Log.d("PHH", "Failed getting HCT gesture service")
            null
        }

        val powerManager = ctxt.getSystemService(PowerManager::class.java)
        if(svc != null) {
            Log.d("PHH", "Starting gesture")
            svc?.startGesture(true)
            Log.d("PHH", "Reading gesture")
            svc?.readGesture(object: IGestureWakeCallback.Stub() {
                override fun reportResults(p0: String) {
                    Log.d("PHH", "Received gesture $p0")
                    if(p0 == "DOUBCLICK") {
                        PowerManager::class.java.getMethod("wakeUp", Long::class.java).invoke(powerManager, SystemClock.uptimeMillis())
                    }
                }
            })
        }
    }
}