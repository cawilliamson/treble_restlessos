#!/system/bin/sh

# Disable restricted networking mode on devices without working BPF.
# Android 14+ uses BPF firewall chains to enforce restricted_networking_mode.
# On older kernels where BPF maps failed to load, the restricted chain cannot
# maintain its allowlist, causing all apps to lose network connectivity.
# The uid_owner_map is the BPF map backing firewall chain rules -- if it does
# not exist, the restricted chain cannot function and must be disabled.
if [ ! -e /sys/fs/bpf/netd_shared/map_netd_uid_owner_map ]; then
    settings put global restricted_networking_mode 0
    log -t phh-on-boot "Disabled restricted_networking_mode (BPF uid_owner_map absent)"
fi

# Disable FUSE BPF on devices whose kernels lack fuse-bpf support.
# Android 16 enables FUSE BPF by default, which requires kernel support for
# the fuse-bpf program type. On older kernels (< 5.4 without backport),
# fuseMedia.bpf fails to load and the FuseDaemon falls back to legacy mode
# but the legacy FUSE path may not work correctly on QPR2+. Setting
# persist.sys.fuse.bpf.override=false tells the FuseDaemon and vold to
# use the legacy FUSE path from the start, avoiding the failed BPF load
# and ensuring bind-mounts for Android/data and Android/obb are set up
# correctly by vold.
if [ ! -f /sys/fs/fuse/features/fuse_bpf ]; then
    setprop persist.sys.fuse.bpf.override false
    log -t phh-on-boot "Disabled FUSE BPF (kernel fuse_bpf feature absent)"
fi

vndk="$(getprop persist.sys.vndk)"
[ -z "$vndk" ] && vndk="$(getprop ro.vndk.version |grep -oE '^[0-9]+')"

[ "$(getprop vold.decrypt)" = "trigger_restart_min_framework" ] && exit 0

# recover USB gadget if the UDC failed to bind during boot.
# on some exynos devices (e.g. samsung galaxy M33), the dwc3 OTG state
# machine starts the gadget before init has configured USB functions via
# configfs, leaving the UDC in a failed state with ENODEV.  cycling
# sys.usb.config forces the vendor USB init to tear down and rebuild
# the gadget cleanly.
#
# however, on devices with a fingerprint HAL, the USB config cycle can
# invalidate the TEE session that the fingerprint HAL depends on,
# breaking fingerprint after every boot without a USB cable.  skip the
# reset on these devices -- fingerprint is more important than
# ADB-over-USB on boot (ADB over WiFi still works).
udc_state="$(cat /config/usb_gadget/g1/UDC 2>/dev/null)"
if [ -z "$udc_state" ] || [ "$udc_state" = "none" ]; then
    fp_hal="$(getprop ro.hardware.fingerprint)"
    if [ -n "$fp_hal" ]; then
        log -t phh-on-boot "USB gadget not bound, but skipping reset: fingerprint HAL ($fp_hal) detected, reset would break TEE session"
    else
        usb_cfg="$(getprop persist.sys.usb.config)"
        if [ -n "$usb_cfg" ]; then
            log -t phh-on-boot "USB gadget not bound, retrying config: $usb_cfg"
            setprop sys.usb.config none
            sleep 1
            setprop sys.usb.config "$usb_cfg"
        fi
        # Restart fingerprint/biometrics HAL after USB gadget recovery.
        # The USB config cycle can disconnect the HAL's TEE session,
        # so restart both services to re-establish it.
        setprop ctl.restart vendor.fps_hal
        setprop ctl.restart vendor.biometrics-hal-1
    fi
fi

setprop ctl.start media.swcodec

for i in wpa p2p;do
	if [ ! -f /data/misc/wifi/${i}_supplicant.conf ];then
		cp /vendor/etc/wifi/wpa_supplicant.conf /data/misc/wifi/${i}_supplicant.conf
	fi
	chmod 0660 /data/misc/wifi/${i}_supplicant.conf
	chown wifi:system /data/misc/wifi/${i}_supplicant.conf
done

if [ -f /vendor/bin/mtkmal ];then
    if [ "$(getprop persist.mtk_ims_support)" = 1 ] || [ "$(getprop persist.mtk_epdg_support)" = 1 ];then
        setprop persist.mtk_ims_support 0
        setprop persist.mtk_epdg_support 0
        reboot
    fi
fi

# spoof post-boot props for securize (skipped on debug builds)
if [ ! -f /metadata/securize_disable ] && [ "$(getprop persist.sys.phh.debug_build)" != "true" ]; then
  resetprop_phh ro.build.user nobody
  resetprop_phh ro.build.host android-build
  resetprop_phh ro.build.tags release-keys
  resetprop_phh ro.product.build.tags release-keys
  resetprop_phh ro.system.build.tags release-keys
  resetprop_phh ro.system_ext.build.tags release-keys
  resetprop_phh ro.vendor.build.tags release-keys
  resetprop_phh ro.boot.vbmeta.device_state locked
  resetprop_phh vendor.boot.vbmeta.device_state locked
  resetprop_phh ro.boot.verifiedbootstate green
  resetprop_phh vendor.boot.verifiedbootstate green
  resetprop_phh ro.boot.flash.locked 1
  resetprop_phh vendor.boot.vbmeta.device_state locked
fi

if grep -qF android.hardware.boot /vendor/manifest.xml || grep -qF android.hardware.boot /vendor/etc/vintf/manifest.xml ;then
	bootctl mark-boot-successful
fi

setprop ctl.restart sec-light-hal-2-0
if find /sys/firmware -name support_fod |grep -qE .;then
	setprop ctl.restart vendor.fps_hal
fi

setprop ctl.stop storageproxyd

sleep 10

minijailSrc=/system/system_ext/apex/com.android.vndk.v28/lib/libminijail.so
minijailSrc64=/system/system_ext/apex/com.android.vndk.v28/lib64/libminijail.so
if [ "$vndk" = 27 ];then
    mount $minijailSrc64 /vendor/lib64/libminijail_vendor.so
    mount $minijailSrc /vendor/lib/libminijail_vendor.so
fi

if [ "$vndk" = 28 ];then
    mount $minijailSrc64 /vendor/lib64/libminijail_vendor.so
    mount $minijailSrc /vendor/lib/libminijail_vendor.so
    mount $minijailSrc64 /system/lib64/vndk-28/libminijail.so
    mount $minijailSrc /system/lib/vndk-28/libminijail.so
    mount $minijailSrc64 /vendor/lib64/libminijail.so
    mount $minijailSrc /vendor/lib/libminijail.so
fi

#Clear looping services
sleep 30
getprop | \
    grep restarting | \
    sed -nE -e 's/\[([^]]*).*/\1/g'  -e 's/init.svc.(.*)/\1/p' |
    while read -r svc ;do
        setprop ctl.stop "$svc"
    done
