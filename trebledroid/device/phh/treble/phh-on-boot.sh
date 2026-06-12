#!/system/bin/sh

vndk="$(getprop persist.sys.vndk)"
[ -z "$vndk" ] && vndk="$(getprop ro.vndk.version |grep -oE '^[0-9]+')"

[ "$(getprop vold.decrypt)" = "trigger_restart_min_framework" ] && exit 0

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

# spoof post-boot props
# should be applied after boot complete to prevent breaking device features
if [ ! -f /metadata/securize_disable ]; then
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
  resetprop_phh ro.boot.veritymode enforcing
  resetprop_phh ro.boot.warranty_bit 0
  resetprop_phh ro.vendor.warranty_bit 0
  resetprop_phh ro.warranty_bit 0
  resetprop_phh --delete ro.build.selinux
  resetprop_phh -n sys.oem_unlock_allowed 0
  resetprop_phh -n init.svc.flash_recovery stopped
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
