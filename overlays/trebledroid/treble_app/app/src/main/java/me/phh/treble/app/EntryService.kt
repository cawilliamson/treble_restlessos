package me.phh.treble.app

import android.app.Service
import android.content.BroadcastReceiver
import android.content.Context
import android.content.ComponentName
import android.content.Intent
import android.content.pm.PackageManager
import android.net.ConnectivityManager
import android.net.Network
import android.net.NetworkCapabilities
import android.net.NetworkRequest
import android.os.IBinder
import android.os.UserHandle
import android.os.SystemProperties
import android.util.Log
import dalvik.system.PathClassLoader
import kotlin.concurrent.thread

class EntryService: Service() {
    companion object {
        var service: EntryService? = null
    }
    override fun onBind(intent: Intent): IBinder? {
        return null
    }

    private fun tryC(fnc: () -> Unit) {
        try {
            fnc()
        } catch(e: Throwable) {
            Log.e("PHH", "Caught", e)
        }
    }

    override fun onCreate() {
        service = this

        thread {
            tryC { Tools.startup(this) }
            tryC { QtiAudio.startup(this) }
            tryC { Lenovo.startup(this) }
            tryC { OnePlus.startup(this) }
            tryC { Oppo.startup(this) }
            tryC { OverlayPicker.startup(this) }
            tryC { Doze.startup(this) }
            tryC { Huawei.startup(this) }
            tryC { Misc.startup(this) }
            tryC { Samsung.startup(this) }
            tryC { Transsion.startup(this) }
            tryC { Xiaomi.startup(this) }
            tryC { Asus.startup(this) }
            tryC { Qualcomm.startup(this) }
            tryC { Vsmart.startup(this) }
            tryC { Nubia.startup(this) }
            tryC { Ims.startup(this) }
            tryC { Custom.startup(this) }
            tryC { Hct.startup(this) }

            tryC { Desktop.startup(this) }
            tryC { Lid.startup(this) }
            tryC { AudioEffects.startup(this) }

            tryC { PresetDownloader.startup(this) }
            tryC {
                val p = SystemProperties.get("ro.system.ota.json_url", "")
                val c = ComponentName(this, UpdaterActivity::class.java)
                if(p.trim() == "") {
                    packageManager.setComponentEnabledSetting(c, PackageManager.COMPONENT_ENABLED_STATE_DISABLED, 0)
                } else {
                    packageManager.setComponentEnabledSetting(c, PackageManager.COMPONENT_ENABLED_STATE_DEFAULT, 0)
                }
            }
        }
    }
}

class Starter: BroadcastReceiver() {
    override fun onReceive(context: Context, intent: Intent) {
        val caller = UserHandle.getCallingUserId()
        if(caller != 0) {
            Log.d("PHH", "Service called from user none 0, ignore")
            return
        }
        Log.d("PHH", "Starting service")
        //TODO: Check current user == "admin" == 0
        when(intent.action) {
            Intent.ACTION_BOOT_COMPLETED,
            Intent.ACTION_MY_PACKAGE_REPLACED,
            Intent.ACTION_LOCKED_BOOT_COMPLETED -> {
                context.startServiceAsUser(Intent(context, EntryService::class.java), UserHandle.SYSTEM)
            }
        }
    }
}

interface EntryStartup {
    fun startup(ctxt: Context)
}
