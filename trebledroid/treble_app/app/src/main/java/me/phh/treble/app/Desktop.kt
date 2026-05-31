package me.phh.treble.app

import android.app.Activity
import android.app.AlertDialog
import android.content.ComponentName
import android.content.Context
import android.content.Intent
import android.hardware.display.DisplayManager
import android.hardware.input.InputManager
import android.os.*
import android.util.Log
import android.view.InputDevice
import java.io.ByteArrayOutputStream
import java.io.File
import java.io.FileInputStream
import kotlin.concurrent.thread

object Desktop: EntryStartup {
    val devices = HashMap<Int, InputDevice>()
    val managedDevices = HashMap<Int, String>()

    override fun startup(ctxt: Context) {
        val im = ctxt.getSystemService(InputManager::class.java)!!
        val dm = ctxt.getSystemService(DisplayManager::class.java)!!

        im.registerInputDeviceListener(object: InputManager.InputDeviceListener {
            override fun onInputDeviceRemoved(p0: Int) {
                if(p0 !in devices) return
                val device = devices.remove(p0)!!
                Log.d("PHH", "Removing device $device")
                val location = managedDevices.remove(p0) ?: return
                val m = InputManager::class.java.getMethod("removePortAssociation", String::class.java)
                Log.d("PHH", "Removing association for $location")
                m.invoke(im, location)
            }

            override fun onInputDeviceAdded(p0: Int) {
                val device = im.getInputDevice(p0)
                devices[p0] = device

                Log.d("PHH", "Received new device! $device")
                if(Build.VERSION.SDK_INT < 29 || !device.isExternal) return

                val nDisplays = dm.displays.size
                if(nDisplays<=1) return


                val inputflinger = ServiceManager.getService("input")
                val outStream = ParcelFileDescriptor.createPipe()

                var result: String? = null
                val resultLock = Object()
                thread {
                    try {
                        Log.d("PHH", "Starting thread to receive dump!")
                        val res = FileInputStream(outStream[0].fileDescriptor).bufferedReader().readText()
                        synchronized(resultLock) {
                            result = res
                            resultLock.notifyAll()
                        }
                        Log.d("PHH", "Closing thread to receive dump!")
                        outStream[0].close()
                    } catch(e: java.lang.Exception) {
                        Log.d("PHH", "Failed getting dump result because of ", e)
                        result = "fail"
                    }
                }
                Log.d("PHH", "Asking for dump!")
                inputflinger.dump(outStream[1].fileDescriptor, emptyArray())
                outStream[1].close()
                Log.d("PHH", "Closed dump!")
                while(result == null) {
                    synchronized(resultLock) {
                        resultLock.wait()
                    }
                }

                var currentLocation = ""
                var currentUnique = ""
                var found = false

                val locationMatcher = Regex("^\\s*Location:(.*)")
                val uniqueMatcher = Regex("^\\s*UniqueId:(.*)")
                val idMatcher = Regex("\\s*Identifier: .*vendor=0x([0-9a-fA-F]{4}).*product=0x([0-9a-fA-F]{4}).*")
                for(line in result!!.split("\n")) {
                    val locationMatch = locationMatcher.matchEntire(line)
                    val uniqueMatch = uniqueMatcher.matchEntire(line)
                    val idMatch = idMatcher.matchEntire(line)

                    if(locationMatch != null) {
                        currentLocation = locationMatch.groupValues[1].trim()
                    }
                    if(uniqueMatch != null) {
                        currentUnique = uniqueMatch.groupValues[1].trim()
                    }
                    if(idMatch != null) {
                        val vendor = Integer.parseInt(idMatch.groupValues[1], 16)
                        val product = Integer.parseInt(idMatch.groupValues[2], 16)
                        if(vendor == device.vendorId && product == device.productId) {
                            found = true
                            break
                        }
                    }
                }

                if(!found) return

                val portLocation = if(currentLocation.isNotEmpty()) currentLocation else currentUnique
                managedDevices[p0] = portLocation

                val m = Context::class.java.getMethod("startActivityAsUser", Intent::class.java, UserHandle::class.java)
                m.invoke(ctxt,
                        Intent()
                                .setComponent(ComponentName(ctxt, DesktopInput::class.java))
                                .putExtra("deviceName", device.name)
                                .putExtra("portLocation", portLocation)
                                .addFlags(Intent.FLAG_ACTIVITY_NEW_TASK),
                        UserHandle.getUserHandleForUid(10105)
                )
            }

            override fun onInputDeviceChanged(p0: Int) {
            }
        }, Handler(Looper.getMainLooper()))
    }
}

class DesktopInput : Activity() {
    override fun onCreate(savedInstanceState: Bundle?) {

        Log.d("PHH", "Creating desktop input activity!")
        super.onCreate(savedInstanceState)

        setFinishOnTouchOutside(true)
    }

    override fun onResume() {
        super.onResume()

        val portLocation = intent.getStringExtra("portLocation")
        if(portLocation == null || portLocation.isEmpty()) throw Exception("Meeeeeeeeeeeeeeeeeeh")

        val deviceName = intent.getStringExtra("deviceName") ?: "Unknown"

        val im = getSystemService(InputManager::class.java)!!
        val m = InputManager::class.java.getMethod("addPortAssociation", String::class.java, Int::class.java)

        val dm = getSystemService(DisplayManager::class.java)!!
        val displays = dm.getDisplays()
        val primaryDisplay = if(displays.size == 2) displays[0] else displays[2]
        val secondaryDisplay = dm.getDisplays()[1]

        val getAddressMethod = secondaryDisplay.javaClass.getMethod("getAddress")
        val primaryAddress = getAddressMethod.invoke(primaryDisplay)
        val secondaryAddress = getAddressMethod.invoke(secondaryDisplay)
        if (secondaryAddress == null) {
            Log.d("PHH", "Secondary screen with no address, can't do input mapping")
            return
        }
        val getPortMethod = secondaryAddress.javaClass.getMethod("getPort")
        val primaryPort = ((getPortMethod.invoke(primaryAddress) as Byte).toInt() + 256) % 256
        val secondaryPort = getPortMethod.invoke(secondaryAddress)

        Log.d("PHH", "Associating device $portLocation to display port $primaryAddress $primaryPort or $secondaryAddress $secondaryPort")

        AlertDialog.Builder(this)
                .setMessage("You connected a new input device ($deviceName) while having a secondary screen.\nDo you want to assign that device to the screen called ${secondaryDisplay.name}?")
                .setPositiveButton("Yes") { p0, p1 ->
                    m.invoke(im, portLocation, secondaryPort)
                    finish()
                }
                .setNeutralButton("Set to ${primaryDisplay.getName()} screen") { p0, p1 ->
                    m.invoke(im, portLocation, primaryPort)
                    finish()
                }
                .setNegativeButton("No") { p0, p1 ->
                    finish()
                }
                .show()
    }
}

