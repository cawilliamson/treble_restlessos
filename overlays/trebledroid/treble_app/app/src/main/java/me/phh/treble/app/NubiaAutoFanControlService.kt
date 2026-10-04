package me.phh.treble.app

import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.Intent
import android.content.IntentFilter
import android.content.SharedPreferences
import android.os.BatteryManager
import android.os.Handler
import android.os.IBinder
import android.os.Looper
import android.os.PowerManager
import android.os.PowerManager.WakeLock
import android.preference.PreferenceManager


class NubiaAutoFanControlService : Service() {

    private var powerConnectionChangedReceiver: BroadcastReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                when (intent.action) {
                    Intent.ACTION_POWER_CONNECTED,
                    Intent.ACTION_BOOT_COMPLETED,
                    Intent.ACTION_LOCKED_BOOT_COMPLETED ->
                        // Delay 1s for battery broadcast changed.
                        Handler(Looper.getMainLooper()).postDelayed({
                            if (canStartFan()){
                                startFan()
                            }
                        }, 1000)
                    Intent.ACTION_POWER_DISCONNECTED ->
                        stopFan()
                }
            }
        }

    // Register when fan startFan()
    // Unregister after stopFan()
    private var batteryChangedReceiver: BroadcastReceiver =
        object : BroadcastReceiver() {
            override fun onReceive(context: Context, intent: Intent) {
                when (intent.action) {
                    Intent.ACTION_BATTERY_CHANGED -> {
                        if (!canStartFan()) {
                            stopFan()
                        }
                    }
                }
            }
        }
    private lateinit var sp: SharedPreferences
    // Release after charger disconnected, or full charger
    private lateinit var wakeLock: WakeLock
    override fun onCreate() {
        this.sp = PreferenceManager.getDefaultSharedPreferences(this)
        val powerManager = this.getSystemService(POWER_SERVICE) as PowerManager
        wakeLock = powerManager.newWakeLock(PowerManager.PARTIAL_WAKE_LOCK, "nubiaFanControl:WakeLockStopFan")
        val connectionChangedIntent = IntentFilter()
        connectionChangedIntent.addAction(Intent.ACTION_POWER_CONNECTED)
        connectionChangedIntent.addAction(Intent.ACTION_POWER_DISCONNECTED)
        connectionChangedIntent.addAction(Intent.ACTION_BOOT_COMPLETED)
        connectionChangedIntent.addAction(Intent.ACTION_LOCKED_BOOT_COMPLETED)
        registerReceiver(powerConnectionChangedReceiver, connectionChangedIntent)
        // Case: Check if it need to start fan after reboot/power up
        if (canStartFan()){
            startFan()
        } else {
            // Case: Fan still start from last session and device rebooted.
            stopFan()
        }
    }

    override fun onStartCommand(
        resultIntent: Intent, resultCode: Int, startId: Int): Int {
        return START_REDELIVER_INTENT
    }

    override fun onDestroy() {
        super.onDestroy()
        try {
            unregisterReceiver(powerConnectionChangedReceiver)
        } catch (_: Exception) {
        }
    }

    override fun onBind(intent: Intent): IBinder? {
        return null
    }
    private fun startFan() {
        wakeLock.acquire(120*60*1000L /*120 minutes*/)
        with (sp.edit()) {
            putString(NubiaSettings.fanSpeed, "5")
            apply()
        }
        try {
            unregisterReceiver(batteryChangedReceiver)
        } catch (_: Exception) {
        }
        val batteryChangedIntent = IntentFilter()
        batteryChangedIntent.addAction(Intent.ACTION_BATTERY_CHANGED)
        registerReceiver(batteryChangedReceiver, batteryChangedIntent)
    }
    private fun stopFan() {
        val gameMode: Boolean = sp.getBoolean(NubiaSettings.tsGameMode, false)
        // Prevent stopping fan while in game mode
        if (!gameMode) {
            with (sp.edit()) {
                putString(NubiaSettings.fanSpeed, "0")
                apply()
            }
        }
        try {
            unregisterReceiver(batteryChangedReceiver)
        } catch (_: Exception) {
        }
        if(wakeLock.isHeld)
            wakeLock.release()
    }

    /**
     * True when connected to charger && battery < 100%
     */
    private fun canStartFan(): Boolean {
        val batteryChangedIntent: Intent? = registerReceiver(null,
                IntentFilter(Intent.ACTION_BATTERY_CHANGED)
        );
        val batStatus: Int = batteryChangedIntent?.getIntExtra(BatteryManager.EXTRA_STATUS, -1) ?: -1
        when (batStatus) {
            BatteryManager.BATTERY_STATUS_CHARGING -> {
                val batteryPct: Float? = batteryChangedIntent?.let { batIntent ->
                    val level: Int = batIntent.getIntExtra(BatteryManager.EXTRA_LEVEL, -1)
                    val scale: Int = batIntent.getIntExtra(BatteryManager.EXTRA_SCALE, -1)
                    level * 100 / scale.toFloat()
                }
                return batteryPct?.toInt() != 100
            }
            BatteryManager.BATTERY_STATUS_DISCHARGING,
            BatteryManager.BATTERY_STATUS_NOT_CHARGING,
            BatteryManager.BATTERY_STATUS_UNKNOWN,
            BatteryManager.BATTERY_STATUS_FULL -> {
                return false
            }
        }
        return true
    }
}