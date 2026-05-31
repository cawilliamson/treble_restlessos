package me.phh.treble.app

import android.content.ContentResolver
import android.content.Context
import android.content.SharedPreferences
import android.graphics.Point
import android.hardware.Sensor
import android.hardware.SensorEvent
import android.hardware.SensorEventListener
import android.hardware.SensorManager
import android.os.Binder
import android.os.Handler
import android.os.Looper
import android.os.PowerManager
import android.os.ServiceManager
import android.os.SystemClock
import android.os.SystemProperties
import android.preference.PreferenceManager
import android.provider.Settings
import android.util.Log
import android.view.Display
import android.view.IWindowManager
import vendor.mediatek.hardware.agolddaemon.IAgoldDaemon
import vendor.mediatek.hardware.agolddaemon.IAgoldDaemonCallback
import java.io.File
import kotlin.concurrent.thread
import kotlin.math.abs

/**
 * Unihertz 8849 (Tank) hardware controls.
 *
 * Controls the built-in projector, warning/camping lights, and provides
 * auto-keystone correction via accelerometer.
 *
 * The projector enable/disable sequence matches the stock Unihertz ROM's
 * ProjectorController.powerOnOffForPrejectorOn/Off() for the
 * PROJECT_NEW_FIRMWARE=true, ElephantProduct configuration.
 *
 * Auto-keystone is a faithful replica of stock calculateOrientation():
 * accumulate 10 accelerometer samples over ~2s, compute pitch correction,
 * write to projector_pitch_setting sysfs, repeat every 3s.
 *
 * Factory calibration data is read from system properties set at boot
 * by 8849-proinfo.sh (which reads proinfo at offsets 0x1000cc and 0x10014c).
 */
object Unihertz8849 : EntryStartup {
    private const val TAG = "PHH"
    private const val SYSFS_BASE = "/sys/bus/spi/drivers/fpga_spi"
    private const val DEFAULT_CALIBRATION = "550,720,711,696,685,672,663,649,638,626,614,599,587,561,548,538,538"

    // Warning light ioctl 310 mode values
    private const val WARNING_OFF   = 0
    private const val WARNING_RED   = 1
    private const val WARNING_BLUE  = 16
    private const val WARNING_GREEN = 256

    // Camping light ioctl 301 mode values
    private const val CAMPING_OFF    = 0
    private const val CAMPING_FULL   = 1   // time-limited: auto-steps down after duration
    private const val CAMPING_MEDIUM = 2
    private const val CAMPING_LOW    = 3
    private const val CAMPING_FLASH  = 4
    private const val CAMPING_SOS    = 5

    // Camping full-blast time limit (stock: BRIGHT_TYPE_MINUTES_5/10/15/30)
    // After this time, step down to medium to prevent overheating.
    private const val CAMPING_FULL_TIMEOUT_DEFAULT_MS = 10 * 60 * 1000L  // 10 minutes

    // Keystone constants from stock calculateOrientation()
    private const val KEYSTONE_SAMPLE_COUNT = 10
    private const val KEYSTONE_CYCLE_MS = 3000L  // BackupAgentTimeoutParameters.DEFAULT_QUOTA_EXCEEDED_TIMEOUT_MILLIS
    private const val KEYSTONE_CHANGE_THRESHOLD = 3.0f
    private const val PITCH_MIN = -40
    private const val PITCH_MAX = 25

    private var powerManager: PowerManager? = null
    private var contentResolver: ContentResolver? = null
    private var sensorManager: SensorManager? = null
    private var accelerometer: Sensor? = null
    private var keystoneListener: SensorEventListener? = null
    private var fanControlThread: Thread? = null
    @Volatile private var projectorActive = false

    // SOS flashing state (app-driven: alternates red/blue)
    private val sosHandler = Handler(Looper.getMainLooper())
    private var sosRunnable: Runnable? = null
    private var sosPhase = false

    // Camping light full-blast timer
    private val campingHandler = Handler(Looper.getMainLooper())
    private var campingTimeoutRunnable: Runnable? = null

    // Fan control state (mirrors stock ElephantProduct fields)
    private var mFanDuty = 20
    private var mPreTemperature = 0

    // Keystone accumulator state (mirrors stock ProjectorController fields)
    private var mX = 0f
    private var mY = 0f
    private var mZ = 0f

    private var mNewGrivatyVal = 0f
    private var mOldGrivatyVal = 0f
    private var mCount = KEYSTONE_SAMPLE_COUNT
    private var mCountStartTime = 0L
    private var mKeystoneChange = 1000  // sentinel: no value written yet

    // ─── Utility ────────────────────────────────────────────────────────

    fun writeToFileNofail(path: String, content: String) {
        // Direct file write. Requires SELinux policy allowing system_app
        // to write to the target sysfs type (sysfs_fpga_spi).
        // See sepolicy patch: 0004-Allow-system_app-to-write-FPGA-sysfs
        try {
            File(path).writeText(content + "\n")
        } catch (t: Throwable) {
            Log.d(TAG, "Failed writing to $path", t)
        }
    }

    private fun readFileNofail(path: String, default: String = ""): String {
        try {
            return File(path).readText().trim()
        } catch (_: Throwable) {}
        return default
    }

    private fun getService(): IAgoldDaemon? {
        try {
            val binder = Binder.allowBlocking(
                ServiceManager.waitForDeclaredService(IAgoldDaemon.DESCRIPTOR + "/default"))
            return IAgoldDaemon.Stub.asInterface(binder)
        } catch (t: Throwable) {
            Log.e(TAG, "Unihertz8849: failed to get agold daemon service", t)
            return null
        }
    }

    private fun sendIoctl(a: Int, b: Int, c: Int, d: Int): Int {
        try {
            return getService()?.SendMessageToIoctl(a, b, c, d) ?: -1
        } catch (t: Throwable) {
            Log.d(TAG, "SendMessageToIoctl($a, $b, $c, $d) failed", t)
            return -1
        }
    }

    // No-op callback for cmd() — the daemon dereferences the callback
    // binder without a null check, so we must pass a real object.
    private val noopCallback = object : IAgoldDaemonCallback.Stub() {
        override fun onResult(result: String?) {}
    }

    private fun sendCalibration() {
        // Read factory calibration from system property (set at boot by
        // 8849-proinfo.sh from the proinfo partition).
        val calibration = readProinfoFocusCalibration() ?: DEFAULT_CALIBRATION
        try {
            val svc = getService() ?: return
            svc.cmd(calibration, noopCallback)
            Log.d(TAG, "Unihertz8849 focus calibration sent via AIDL: $calibration")
        } catch (t: Throwable) {
            Log.e(TAG, "Unihertz8849: failed to send focus calibration", t)
        }
    }

    // ─── Warning light (ioctl 310) ──────────────────────────────────────

    /**
     * SOS mode: police-style alternating red/blue flash at ~3Hz.
     * Implemented in-app since the FPGA has no native SOS mode.
     * Stock app (com.agui.warninglights) implements this the same way.
     */
    private fun startSos() {
        stopSos()
        sosPhase = false
        val runnable = object : Runnable {
            override fun run() {
                sosPhase = !sosPhase
                sendIoctl(310, 0, if (sosPhase) WARNING_RED else WARNING_BLUE, 0)
                sosHandler.postDelayed(this, 167L)  // ~6Hz flash
            }
        }
        sosRunnable = runnable
        sosHandler.post(runnable)
        Log.d(TAG, "Unihertz8849: SOS warning light started")
    }

    private fun stopSos() {
        sosRunnable?.let { sosHandler.removeCallbacks(it) }
        sosRunnable = null
    }

    private fun setWarningLight(mode: Int) {
        stopSos()
        when (mode) {
            -1 -> startSos()  // SOS sentinel value
            else -> sendIoctl(310, 0, mode, 0)
        }
    }

    // ─── Camping light (ioctl 301) ──────────────────────────────────────

    /**
     * Full-brightness time limit.
     * Stock (AguiCampingLamp): BRIGHT_TYPE_MINUTES_5/10/15/30.
     * After the duration, automatically step down to medium to prevent
     * overheating. Default is 10 minutes.
     */
    private fun setCampingLight(mode: Int, sp: SharedPreferences) {
        // Cancel any existing full-blast timer
        campingTimeoutRunnable?.let { campingHandler.removeCallbacks(it) }
        campingTimeoutRunnable = null

        sendIoctl(301, 0, mode, 0)

        if (mode == CAMPING_FULL) {
            val timeoutMs = 10 * 60 * 1000L
            Log.d(TAG, "Unihertz8849: camping full blast, stepping down in 10min")
            val runnable = Runnable {
                Log.d(TAG, "Unihertz8849: camping full blast timeout, stepping down to medium")
                sendIoctl(301, 0, CAMPING_MEDIUM, 0)
                // Update the preference to reflect the new state
            }
            campingTimeoutRunnable = runnable
            campingHandler.postDelayed(runnable, timeoutMs)
        }
    }

    // ─── proinfo calibration (read from system properties) ────────────

    private fun readProinfoFocusCalibration(): String? {
        val raw = SystemProperties.get("sys.phh.8849.focus_cal", "")
        if (raw.startsWith("550,")) {
            Log.d(TAG, "Unihertz8849 proinfo focus calibration: $raw")
            return raw
        }
        return null
    }

    private fun readProinfoOtherCalibration(): String? {
        val raw = SystemProperties.get("sys.phh.8849.other_cal", "")
        val parts = raw.split(",")
        if (parts.size >= 3 && parts.all { it.matches(Regex("\\d+")) }) {
            Log.d(TAG, "Unihertz8849 proinfo other calibration: $raw")
            return raw
        }
        return null
    }

    private fun cleanupStalePreferences(sp: SharedPreferences) {
        // Remove stale preference keys from previous naming schemes
        val editor = sp.edit()
        sp.all.keys.filter { key ->
            (key.contains("8849") || key.contains("projector")) &&
            !key.startsWith("unihertz8849_")
        }.forEach { key ->
            Log.d(TAG, "Removing stale preference: $key")
            editor.remove(key)
        }
        // Remove focus calibration -- always read from proinfo now
        editor.remove("unihertz8849_focus_calibration")
        editor.apply()
    }

    // ─── Display management ─────────────────────────────────────────────

    private fun waitForSysStatus(target: Int, timeoutMs: Long = 8000): Boolean {
        val pm = powerManager ?: return false
        val start = SystemClock.elapsedRealtime()
        while (SystemClock.elapsedRealtime() - start < timeoutMs) {
            val status = readFileNofail("$SYSFS_BASE/projector_sys_status", "-1")
            if (status == target.toString()) return true
            if (target == 0) pm.goToSleep(SystemClock.uptimeMillis())
            else pm.wakeUp(SystemClock.uptimeMillis())
            Thread.sleep(200)
        }
        Log.w(TAG, "Timed out waiting for projector_sys_status == $target")
        return false
    }

    private fun getWindowManager(): IWindowManager? {
        return try {
            IWindowManager.Stub.asInterface(ServiceManager.getService("window"))
        } catch (t: Throwable) {
            Log.e(TAG, "Unihertz8849: failed to get IWindowManager", t)
            null
        }
    }

    private fun shrinkDisplayForProjector() {
        try {
            val wm = getWindowManager() ?: return
            val size = Point()
            wm.getInitialDisplaySize(Display.DEFAULT_DISPLAY, size)
            val width = size.x
            val height16x9 = width * 16 / 9
            Log.d(TAG, "Unihertz8849: shrinking display to ${width}x${height16x9}")
            wm.setForcedDisplaySize(Display.DEFAULT_DISPLAY, width, height16x9)
        } catch (t: Throwable) {
            Log.e(TAG, "Unihertz8849: failed to shrink display", t)
        }
    }

    private fun restoreDisplay() {
        try {
            val wm = getWindowManager() ?: return
            wm.clearForcedDisplaySize(Display.DEFAULT_DISPLAY)
        } catch (t: Throwable) {
            Log.e(TAG, "Unihertz8849: failed to restore display", t)
        }
    }

    private fun disableAoD(): Boolean {
        val cr = contentResolver ?: return false
        val wasEnabled = try {
            Settings.Secure.getInt(cr, Settings.Secure.DOZE_ALWAYS_ON, 0) != 0
        } catch (_: Throwable) { false }
        if (wasEnabled) {
            try {
                Settings.Secure.putInt(cr, Settings.Secure.DOZE_ALWAYS_ON, 0)
                Thread.sleep(200)
            } catch (t: Throwable) {
                Log.w(TAG, "Failed to disable AoD", t)
            }
        }
        return wasEnabled
    }

    private fun restoreAoD(wasEnabled: Boolean) {
        if (!wasEnabled) return
        val cr = contentResolver ?: return
        try {
            Settings.Secure.putInt(cr, Settings.Secure.DOZE_ALWAYS_ON, 1)
        } catch (t: Throwable) {
            Log.w(TAG, "Failed to restore AoD", t)
        }
    }

    // ─── Projector enable/disable ───────────────────────────────────────

    private fun enableProjector(sp: SharedPreferences) {
        thread {
            val mode = sp.getString(Unihertz8849Settings.projectorMode, "3") ?: "3"
            val pm = powerManager ?: return@thread

            // Step 1: Shrink display to 16:9 first
            shrinkDisplayForProjector()

            // Step 2: Disable AoD
            val aodWasEnabled = disableAoD()

            // Step 3: Pre-load focus calibration from proinfo
            sendCalibration()

            // Step 4: Sleep display and wait for sys_status == 0
            pm.goToSleep(SystemClock.uptimeMillis())
            if (!waitForSysStatus(0)) {
                Log.e(TAG, "Unihertz8849 enable: display failed to sleep, aborting")
                restoreAoD(aodWasEnabled)
                restoreDisplay()
                return@thread
            }

            // Step 5: Disable phone display
            sendIoctl(500, 0, 0, 4)

            // Step 6: Hardware init
            sendIoctl(500, 0, 0, 6)

            // Step 7: Wake display and wait for sys_status == 1
            pm.wakeUp(SystemClock.uptimeMillis())
            if (!waitForSysStatus(1)) {
                Log.e(TAG, "Unihertz8849 enable: display failed to wake, aborting")
                restoreAoD(aodWasEnabled)
                restoreDisplay()
                return@thread
            }

            // Step 8: Set projector mode
            sendIoctl(500, 0, mode.toInt(), 0)
            Thread.sleep(100)

            // Step 9: Re-enable phone display
            sendIoctl(500, 0, 0, 3)

            // Step 10: Restore AoD
            restoreAoD(aodWasEnabled)

            // Step 11: Apply other calibration (AWB + H3 scan)
            val otherCal = sp.getString(Unihertz8849Settings.otherCalibration, null)
            if (otherCal != null) {
                val parts = otherCal.split(",")
                if (parts.size >= 3) {
                    writeToFileNofail("$SYSFS_BASE/dlpc3421_awb_cali", parts[0])
                    writeToFileNofail("$SYSFS_BASE/fpga_h3_scan_hori", parts[1])
                    writeToFileNofail("$SYSFS_BASE/fpga_h3_scan_vert", parts[2])
                }
            }

            // Step 12: Re-send focus calibration
            sendCalibration()

            // Step 13: Set initial pitch to 0
            writeToFileNofail("$SYSFS_BASE/projector_pitch_setting", "0")

            // Step 14: Set brightness from preference
            val brightness = sp.getInt(Unihertz8849Settings.brightness, 128)
            writeToFileNofail("$SYSFS_BASE/projector_rgb_current", brightness.toString())

            // Step 15: Start temperature-reactive fan control
            startFanControl()

            // Step 16: Start auto-keystone (always on)
            startAutoKeystone()

            projectorActive = true
            Log.d(TAG, "Unihertz8849 enable: done")
        }
    }

    private fun disableProjector() {
        if (!projectorActive) {
            Log.d(TAG, "Unihertz8849 disable: projector not active, skipping")
            return
        }
        thread {
            stopAutoKeystone()
            stopFanControl()

            sendIoctl(500, 0, 0, 4)
            Thread.sleep(500)
            sendIoctl(500, 0, 0, 1)
            sendIoctl(500, 0, 0, 0)
            sendIoctl(500, 0, 0, 3)
            Thread.sleep(50)

            val aodWasEnabled = disableAoD()
            val pm = powerManager
            if (pm != null) {
                pm.goToSleep(SystemClock.uptimeMillis())
                waitForSysStatus(0)
                pm.wakeUp(SystemClock.uptimeMillis())
                waitForSysStatus(1)
            }
            restoreAoD(aodWasEnabled)
            Thread.sleep(100)

            restoreDisplay()
            projectorActive = false
            Log.d(TAG, "Unihertz8849 disable: done")
        }
    }

    // ─── Auto-keystone (always on when projector is active) ─────────────

    private fun startAutoKeystone() {
        if (keystoneListener != null) return
        val sm = sensorManager ?: return
        val accel = accelerometer ?: return

        mX = 0f; mY = 0f; mZ = 0f
        mNewGrivatyVal = 0f; mOldGrivatyVal = 0f
        mCount = KEYSTONE_SAMPLE_COUNT; mCountStartTime = 0L; mKeystoneChange = 1000

        val listener = object : SensorEventListener {
            override fun onSensorChanged(event: SensorEvent) { calculateOrientation(event) }
            override fun onAccuracyChanged(sensor: Sensor?, accuracy: Int) {}
        }
        keystoneListener = listener
        sm.registerListener(listener, accel, SensorManager.SENSOR_DELAY_NORMAL)
        Log.d(TAG, "Unihertz8849: auto-keystone started")
    }

    private fun stopAutoKeystone() {
        val listener = keystoneListener ?: return
        sensorManager?.unregisterListener(listener)
        keystoneListener = null
        writeToFileNofail("$SYSFS_BASE/projector_pitch_setting", "0")
        Log.d(TAG, "Unihertz8849: auto-keystone stopped")
    }

    /**
     * Stock calculateOrientation() algorithm, exact replica.
     * Accumulates 10 samples over ~2s, computes pitch correction,
     * writes to projector_pitch_setting, resets every 3s.
     *
     * GSI note: sign is inverted vs stock -- use (int)(mNewGrivatyVal)
     * instead of -(int)(mNewGrivatyVal).
     */
    private fun calculateOrientation(event: SensorEvent) {
        if (mCountStartTime == 0L) mCountStartTime = SystemClock.elapsedRealtime()

        val values = event.values

        if (mCount > 0) {
            mX += values[0]; mY += values[1]; mZ += values[2]
            mCount--
            return
        } else if (mCount == 0) {
            val n = KEYSTONE_SAMPLE_COUNT.toFloat()
            var raise = mY / n
            val avgZ = mZ / n
            val gval: Float
            if (raise >= 75.0f) {
                gval = avgZ
            } else if (raise < -75.0f) {
                gval = -avgZ
            } else {
                if (avgZ > 80.0f) raise = -raise
                gval = raise
            }

            var g = (gval * 1.68).toFloat()
            if (g > 100.0f) g = 100.0f
            if (g < -100.0f) g = -100.0f
            mNewGrivatyVal = (-g) * 0.4f

            if (abs(mNewGrivatyVal - mOldGrivatyVal) > KEYSTONE_CHANGE_THRESHOLD) {
                mOldGrivatyVal = mNewGrivatyVal
                var pitch = mNewGrivatyVal.toInt()  // GSI: no outer negation
                if (pitch > PITCH_MAX) pitch = PITCH_MAX
                if (pitch < PITCH_MIN) pitch = PITCH_MIN
                if (pitch != mKeystoneChange) {
                    mKeystoneChange = pitch
                    writeToFileNofail("$SYSFS_BASE/projector_pitch_setting", pitch.toString())
                    Log.d(TAG, "Unihertz8849 keystone: pitch=$pitch (raise=${mY / KEYSTONE_SAMPLE_COUNT})")
                }
            }
            mCount--
        }

        if (SystemClock.elapsedRealtime() - mCountStartTime > KEYSTONE_CYCLE_MS) {
            mCountStartTime = 0L; mCount = KEYSTONE_SAMPLE_COUNT
            mX = 0f; mY = 0f; mZ = 0f
            mNewGrivatyVal = 0f
        }
    }

    // ─── Fan control ────────────────────────────────────────────────────

    /**
     * Temperature-reactive fan control, exact replica of stock
     * ElephantProduct.getTemperature/setFanDuty logic polled every 1500ms.
     *
     * - Open:  start at 20%
     * - Close: set to 0%
     * - temp > 40°C and fan < 30% → ramp to 30%
     * - temp > 48°C               → full blast 100%
     * - |temp delta| >= 2°C       → gradual adjustment (delta/2 * 8 duty points)
     */
    private fun startFanControl() {
        if (fanControlThread?.isAlive == true) return
        mFanDuty = 20; mPreTemperature = 0
        writeToFileNofail("$SYSFS_BASE/projector_fan_en", "20")
        fanControlThread = thread {
            while (!Thread.currentThread().isInterrupted) {
                try {
                    Thread.sleep(1500)
                    updateFanFromTemperature()
                } catch (_: InterruptedException) { break }
                  catch (t: Throwable) { Log.w(TAG, "Fan control error", t) }
            }
        }
    }

    private fun stopFanControl() {
        fanControlThread?.interrupt()
        fanControlThread = null
        mPreTemperature = 0; mFanDuty = 20
        writeToFileNofail("$SYSFS_BASE/projector_fan_en", "0")
    }

    private fun updateFanFromTemperature() {
        val tempStr = readFileNofail("$SYSFS_BASE/projector_temp", "N/A")
        if (tempStr == "N/A" || tempStr.isEmpty()) return
        val temp = try {
            if (tempStr.contains(",")) tempStr.split(",")[0].trim().toInt()
            else tempStr.trim().toInt()
        } catch (_: NumberFormatException) { return }

        if (mPreTemperature <= 0) mPreTemperature = temp

        if (temp > 40 && mFanDuty < 30) {
            mFanDuty = 30; writeToFileNofail("$SYSFS_BASE/projector_fan_en", "30")
        } else if (temp > 48) {
            mFanDuty = 100; writeToFileNofail("$SYSFS_BASE/projector_fan_en", "100")
        }

        if (abs(temp - mPreTemperature) >= 2) {
            val delta = (temp - mPreTemperature) / 2
            mFanDuty = (mFanDuty + delta * 8).coerceIn(20, 100)
            writeToFileNofail("$SYSFS_BASE/projector_fan_en", mFanDuty.toString())
            mPreTemperature = temp
        }
    }

    // ─── SharedPreferences listener ─────────────────────────────────────

    val spListener = SharedPreferences.OnSharedPreferenceChangeListener { sp, key ->
        // NOTE: onSharedPreferenceChanged fires for ALL keys when the listener
        // is first registered. Guard every action to only run when meaningful.
        val projectorOn = sp.getBoolean(Unihertz8849Settings.projectorEnabled, false)

        when (key) {
            Unihertz8849Settings.warningLight -> {
                // Only send if value actually changed to something -- the listener
                // fires at startup for all keys including the default (0=off).
                val value = (sp.getString(key, "0") ?: "0").toIntOrNull() ?: 0
                setWarningLight(value)
            }
            Unihertz8849Settings.campingLight -> {
                val value = (sp.getString(key, "0") ?: "0").toIntOrNull() ?: 0
                setCampingLight(value, sp)
            }
            Unihertz8849Settings.projectorEnabled -> {
                val enabled = sp.getBoolean(key, false)
                if (enabled) enableProjector(sp) else disableProjector()
            }
            Unihertz8849Settings.projectorMode -> {
                // Only send mode change if projector is actually running
                if (projectorOn) {
                    val mode = sp.getString(key, "3") ?: "3"
                    sendIoctl(500, 0, mode.toInt(), 0)
                    sendCalibration()
                }
            }
            Unihertz8849Settings.displayEnabled -> {
                // Only send display toggle if projector is running --
                // ioctl 500 0 0 3/4 crashes the Agold daemon when projector is off
                if (projectorOn) {
                    val enabled = sp.getBoolean(key, true)
                    sendIoctl(500, 0, 0, if (enabled) 3 else 4)
                }
            }
            Unihertz8849Settings.brightness -> {
                // Only write brightness if projector is running
                if (projectorOn) {
                    val value = sp.getInt(key, 128)
                    writeToFileNofail("$SYSFS_BASE/projector_rgb_current", value.toString())
                }
            }
        }
    }

    // ─── Startup ────────────────────────────────────────────────────────

    override fun startup(ctxt: Context) {
        if (!Unihertz8849Settings.enabled()) return

        // Tell the Agold daemon we are not a GSI and set the expected
        // camera package name. These were previously done by agold-cmd
        // on every invocation; now we do it once at startup.
        try {
            getService()?.let { svc ->
                svc.setNotGsi("NotGsi")
                svc.setCameraClientPackageName("com.mediatek.camera")
            }
        } catch (t: Throwable) {
            Log.d(TAG, "Unihertz8849: setNotGsi/setCameraClientPackageName failed", t)
        }

        powerManager = ctxt.getSystemService(Context.POWER_SERVICE) as PowerManager
        contentResolver = ctxt.contentResolver
        sensorManager = ctxt.getSystemService(Context.SENSOR_SERVICE) as? SensorManager
        accelerometer = sensorManager?.getDefaultSensor(Sensor.TYPE_ACCELEROMETER)

        val sp = PreferenceManager.getDefaultSharedPreferences(ctxt)
        cleanupStalePreferences(sp)
        sp.registerOnSharedPreferenceChangeListener(spListener)
    }
}
