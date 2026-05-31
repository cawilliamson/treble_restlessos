package me.phh.treble.app

import android.content.SharedPreferences
import android.content.res.TypedArray
import android.os.Build
import android.preference.PreferenceManager
import android.service.quicksettings.Tile
import android.service.quicksettings.TileService
import android.util.TypedValue
import android.widget.Toast
import androidx.annotation.RequiresApi

class NubiaGameModeTilesService: TileService() {
    private lateinit var sp: SharedPreferences

    override fun onCreate() {
        this.sp = PreferenceManager.getDefaultSharedPreferences(this)
    }

    // Called when the user adds your tile.
    override fun onTileAdded() {
        super.onTileAdded()
    }
    // Called when your app can update your tile.
    override fun onStartListening() {
        super.onStartListening()
        val gameMode: Boolean = sp.getBoolean(NubiaSettings.tsGameMode, false)
        qsTile.contentDescription = if (gameMode) "On" else "Off"
        qsTile.state = if (gameMode) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        qsTile.updateTile()

    }

    // Called when your app can no longer update your tile.
    override fun onStopListening() {
        super.onStopListening()
    }

    // Called when the user taps on your tile in an active or inactive state.
    override fun onClick() {
        super.onClick()
        val gameMode: Boolean = sp.getBoolean(NubiaSettings.tsGameMode, false)
        with (sp.edit()) {
            putBoolean(NubiaSettings.tsGameMode, !gameMode)
            apply()
        }
        qsTile.state = if (gameMode) Tile.STATE_INACTIVE else Tile.STATE_ACTIVE
        qsTile.contentDescription = if (gameMode) "Off" else "On"
        qsTile.updateTile()
    }
    // Called when the user removes your tile.
    override fun onTileRemoved() {
        super.onTileRemoved()
    }

}

class NubiaFanControlTilesService: TileService() {
    private lateinit var sp: SharedPreferences
    private lateinit var fanSpeedValues: Array<String>
    private lateinit var fanSpeedDescs: Array<String>
    override fun onCreate() {
        this.sp = PreferenceManager.getDefaultSharedPreferences(this)
        // 0 3 4 5
        fanSpeedValues = resources.getStringArray(R.array.pref_nubia_fan_values)
        // Off Low Middle High
        fanSpeedDescs = resources.getStringArray(R.array.pref_nubia_fan)
    }

    private fun getSpeedDesc(fanSpeedValue: String): String {
        val index = fanSpeedValues.indexOf(fanSpeedValue)
        return fanSpeedDescs[index]
    }

    // Called when the user adds your tile.
    override fun onTileAdded() {
        super.onTileAdded()
    }
    // Called when your app can update your tile.
    override fun onStartListening() {
        super.onStartListening()
        val currentFanSpeedValue: String = sp.getString(NubiaSettings.fanSpeed, "0")!!
        val fanStopped = fanSpeedValues.indexOf(currentFanSpeedValue) == 0;

        qsTile.contentDescription = getSpeedDesc(currentFanSpeedValue)
        qsTile.state = if (fanStopped) Tile.STATE_INACTIVE else Tile.STATE_ACTIVE
        qsTile.updateTile()

    }

    // Called when your app can no longer update your tile.
    override fun onStopListening() {
        super.onStopListening()
    }

    // Called when the user taps on your tile in an active or inactive state.
    override fun onClick() {
        super.onClick()

        val beforeClickFanSpeedValue: String = sp.getString(NubiaSettings.fanSpeed, "0")!!
        val beforeClickFanSpeedIndex: Int = fanSpeedValues.indexOf(beforeClickFanSpeedValue)

        var afterClickFanSpeedIndex: Int = beforeClickFanSpeedIndex + 1
        if (beforeClickFanSpeedIndex == (fanSpeedValues.size - 1)) {
            afterClickFanSpeedIndex = 0
        }
        val afterClickFanSpeedValue = fanSpeedValues[afterClickFanSpeedIndex]
        val afterClickFanSpeedDesc = getSpeedDesc(afterClickFanSpeedValue)

        with (sp.edit()) {
            putString(NubiaSettings.fanSpeed, afterClickFanSpeedValue)
            apply()
        }

        val fanStopped = afterClickFanSpeedIndex == 0;

        qsTile.contentDescription = afterClickFanSpeedDesc
        qsTile.state = if (fanStopped) Tile.STATE_INACTIVE else Tile.STATE_ACTIVE
        qsTile.updateTile()
        Toast.makeText(this, afterClickFanSpeedDesc, Toast.LENGTH_SHORT).show()
    }
    // Called when the user removes your tile.
    override fun onTileRemoved() {
        super.onTileRemoved()
    }
}


class NubiaShoulderBtnTilesService: TileService() {
    private lateinit var sp: SharedPreferences

    override fun onCreate() {
        this.sp = PreferenceManager.getDefaultSharedPreferences(this)
    }

    // Called when the user adds your tile.
    override fun onTileAdded() {
        super.onTileAdded()
    }
    // Called when your app can update your tile.
    override fun onStartListening() {
        super.onStartListening()
        val shouldBtnEnabled: Boolean = sp.getBoolean(NubiaSettings.shoulderBtn, false)
        qsTile.contentDescription = if (shouldBtnEnabled) "On" else "Off"
        qsTile.state = if (shouldBtnEnabled) Tile.STATE_ACTIVE else Tile.STATE_INACTIVE
        qsTile.updateTile()

    }

    // Called when your app can no longer update your tile.
    override fun onStopListening() {
        super.onStopListening()
    }

    // Called when the user taps on your tile in an active or inactive state.
    override fun onClick() {
        super.onClick()
        val shouldBtnEnabled: Boolean = sp.getBoolean(NubiaSettings.shoulderBtn, false)
        with (sp.edit()) {
            putBoolean(NubiaSettings.shoulderBtn, !shouldBtnEnabled)
            apply()
        }
        qsTile.state = if (shouldBtnEnabled) Tile.STATE_INACTIVE else Tile.STATE_ACTIVE
        qsTile.contentDescription = if (shouldBtnEnabled) "Off" else "On"
        qsTile.updateTile()
    }
    // Called when the user removes your tile.
    override fun onTileRemoved() {
        super.onTileRemoved()
    }

}
