package me.phh.treble.app

import android.content.Context
import android.os.ServiceManager
import android.content.om.IOverlayManager
import android.content.om.OverlayInfo
import android.os.RemoteException
import android.os.SystemProperties
import android.preference.PreferenceManager
import android.util.Log

object OverlayPicker: EntryStartup {
    private var om: IOverlayManager? = null
    private var overlays = listOf<OverlayInfo>()

    private val platform = SystemProperties.get("ro.board.platform")
    private val vendorFp = SystemProperties.get("ro.vendor.build.fingerprint")

    enum class ThemeOverlay {
        AccentColor,
        IconShape,
        FontFamily,
        IconPack
    }

    fun setOverlayEnabled(o: String, enabled: Boolean) {
        try {
            om!!.setEnabled(o, enabled, 0)
        } catch (e: RemoteException) {
            Log.d("PHH", "Failed to set overlay", e)
        }
    }

    fun getThemeOverlays(to: ThemeOverlay): List<OverlayInfo> {
        when(to) {
            ThemeOverlay.AccentColor ->
                return overlays.filter {
                    it.targetPackageName == "android" &&
                    it.packageName.startsWith("com.android.theme.color.")
                }
            ThemeOverlay.IconShape ->
                return overlays.filter {
                    it.targetPackageName == "android" &&
                    it.packageName.startsWith("com.android.theme.icon.")
                }
            ThemeOverlay.FontFamily ->
                return overlays.filter {
                    it.targetPackageName == "android" &&
                    it.packageName.startsWith("com.android.theme.font.")
                }
            ThemeOverlay.IconPack -> {
                return overlays.filter {
                    it.packageName.startsWith("com.android.theme.icon_pack.")
                }
            }
            else ->
                return listOf<OverlayInfo>()
        }
    }

    private fun enableLte(ctxt: Context) {
        //TODO: List here all non-LTE platforms
        if ("mt6580" != platform)
            setOverlayEnabled("me.phh.treble.overlay.telephony.lte", true)
    }

    private fun handleNokia(ctxt: Context) {
        if(vendorFp == null) return
		
        //Nokia 8.1/X7 [PNX]
        if(vendorFp.matches(Regex("Nokia/Phoenix.*"))) {
            setOverlayEnabled("me.phh.treble.overlay.nokia.pnx_8_1_x7.systemui", true)
        }
    }

    private fun handleSamsung(ctxt: Context) {
        if(vendorFp == null) return

        if(vendorFp.matches(Regex(".*(crown|star)[q2]*lte.*")) ||
                vendorFp.matches(Regex(".*(SC-0[23]K|SCV3[89]).*"))) {
            setOverlayEnabled("me.phh.treble.overlay.samsung.s9.systemui", true)
        }
    }

    private fun handleXiaomi(ctxt: Context) {
        if(vendorFp == null) return

        if(vendorFp.matches(Regex(".*iaomi/perseus.*"))) {
            setOverlayEnabled("me.phh.treble.overlay.xiaomi.mimix3.systemui", true)
        }
        if(vendorFp.matches(Regex(".*iaomi/cepheus.*"))) {
            setOverlayEnabled("me.phh.treble.overlay.xiaomi.mi9.systemui", true)
        }
    }

    private fun getOverlays(ctxt: Context) {
        try {
            val values = mutableListOf<OverlayInfo>()
            @Suppress("UNCHECKED_CAST")
            (om!!.getAllOverlays(0) as Map<String, List<OverlayInfo>>).forEach { values.addAll(it.value) }
            overlays = values.toList()
        } catch (e: Exception) {
            Log.d("PHH", "Failed to get overlays", e)
        }
    }

    override fun startup(ctxt: Context) {
        om = IOverlayManager.Stub.asInterface(
                ServiceManager.getService("overlay"))

        enableLte(ctxt)
        handleNokia(ctxt)
        handleSamsung(ctxt)
        handleXiaomi(ctxt)
        getOverlays(ctxt)

        setOverlayEnabled("me.phh.treble.overlay.systemui.falselocks", true)
    }
}
