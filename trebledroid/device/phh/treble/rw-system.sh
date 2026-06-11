#!/system/bin/sh

if [ -z "$debug" ] && [ -f /cache/phh-log ];then
	mkdir -p /cache/phh
	debug=1 exec sh -x "$(readlink -f -- "$0")" > /cache/phh/logs 2>&1
else
    # Allow accessing logs from system app
    # Protected via SELinux for other apps
    chmod 0755 /cache/phh
    chmod 0644 /cache/phh/logs
fi

vndk="$(getprop persist.sys.vndk)"
[ -z "$vndk" ] && vndk="$(getprop ro.vndk.version |grep -oE '^[0-9]+')"

if [ "$vndk" = 26 ];then
	resetprop_phh ro.vndk.version 26
fi

setprop sys.usb.ffs.aio_compat true

if getprop ro.vendor.build.fingerprint | grep -q -i -e Blackview/BV9500Plus;then
    setprop persist.adb.nonblocking_ffs true
else
    setprop persist.adb.nonblocking_ffs false
fi

fixSPL() {
    if [ "$(getprop ro.product.cpu.abi)" = "armeabi-v7a" ]; then
        setprop ro.keymaster.mod 'AOSP on ARM32'
    else
        setprop ro.keymaster.mod 'AOSP on ARM64'
    fi
    img="$(find /dev/block -type l -iname kernel"$(getprop ro.boot.slot_suffix)" | grep by-name | head -n 1)"
    [ -z "$img" ] && img="$(find /dev/block -type l -iname boot"$(getprop ro.boot.slot_suffix)" | grep by-name | head -n 1)"
    if [ -n "$img" ]; then
    #Rewrite SPL/Android version if needed
    # Read Android version from vbmeta (often trustkernel's root of trust)
    Arelease="$(strings -n 2 /dev/block/by-name/vbmeta* | grep -A1 com.android.build.system.os_version | grep -E '^[0-9]+$' | sort -n | head -n1)"
    # Otherwise read it from boot.img
    [ -z "$Arelease" ] && Arelease="$(getSPL "$img" android)"
    spl="$(getSPL "$img" spl)"
    setprop ro.keymaster.xxx.release "${Arelease}"
    setprop ro.keymaster.xxx.security_patch "$spl"
    if [ -z "$Arelease" ] || [ -z "$spl" ];then
        return 0
    fi
    # Some devices will want true vbmeta_state and verifiedbootstate
    # Setup those properties redirect for "keymaster" prop redirects
    setprop ro.keymaster.xxx.vbmeta_state unlocked
    setprop ro.keymaster.xxx.verifiedbootstate orange

    if getprop ro.vendor.build.fingerprint |grep -q -i samsung/j6lte;then
        setprop debug.phh.props.ter@3.0-service keymaster
        setprop debug.phh.props.mcDriverDaemon keymaster
    fi

    # Found on Cubot Pocket 3: trustkernel work only on stock model name or AOSP GSI model name
    if [ -f /vendor/bin/hw/android.hardware.keymaster@4.1-service.trustkernel ] && [ -f /proc/tkcore/tkcore_log ];then
        setprop debug.phh.props.teed keymaster
        # Process name is android.hardware.keymaster@4.1-service.trustkernel
        setprop debug.phh.props.ice.trustkernel keymaster
    fi

        setprop ro.keymaster.brn Android

        if getprop ro.vendor.build.fingerprint |grep -qiE 'samsung.*star.*lte';then
            additional="/apex/com.android.vndk.v28/lib64/libsoftkeymasterdevice.so /apex/com.android.vndk.v29/lib64/libsoftkeymasterdevice.so"
        else
            getprop ro.vendor.build.fingerprint | grep -qiE '^samsung/' && return 0
        fi
        for f in \
            /vendor/lib64/hw/android.hardware.keymaster@3.0-impl-qti.so /vendor/lib/hw/android.hardware.keymaster@3.0-impl-qti.so \
            /system/lib64/vndk-26/libsoftkeymasterdevice.so /vendor/bin/teed \
            /apex/com.android.vndk.v26/lib/libsoftkeymasterdevice.so  \
            /apex/com.android.vndk.v26/lib64/libsoftkeymasterdevice.so  \
            /system/lib64/vndk/libsoftkeymasterdevice.so /system/lib/vndk/libsoftkeymasterdevice.so \
            /system/lib/vndk-26/libsoftkeymasterdevice.so \
            /system/lib/vndk-27/libsoftkeymasterdevice.so /system/lib64/vndk-27/libsoftkeymasterdevice.so \
	    /vendor/lib/libkeymaster3device.so /vendor/lib64/libkeymaster3device.so \
        /vendor/lib/libMcTeeKeymaster.so /vendor/lib64/libMcTeeKeymaster.so \
        /vendor/lib/hw/libMcTeeKeymaster.so /vendor/lib64/hw/libMcTeeKeymaster.so $additional; do
            [ ! -f "$f" ] && continue
            # shellcheck disable=SC2010
            ctxt="$(ls -lZ "$f" | grep -oE 'u:object_r:[^:]*:s0')"
            b="$(echo "$f" | tr / _)"

            cp -a "$f" "/mnt/phh/$b"
            sed -i \
                -e 's/ro.build.version.release/ro.keymaster.xxx.release/g' \
                -e 's/ro.build.version.security_patch/ro.keymaster.xxx.security_patch/g' \
                -e 's/ro.product.model/ro.keymaster.mod/g' \
                -e 's/ro.product.brand/ro.keymaster.brn/g' \
                "/mnt/phh/$b"
            chcon "$ctxt" "/mnt/phh/$b"
            mount -o bind "/mnt/phh/$b" "$f"
        done
        if [ "$(getprop init.svc.keymaster-3-0)" = "running" ]; then
            setprop ctl.restart keymaster-3-0
        fi
        if [ "$(getprop init.svc.teed)" = "running" ]; then
            setprop ctl.restart teed
        fi
    fi
}

changeKeylayout() {
    mpk="/mnt/phh/keylayout"
    cp -a /system/usr/keylayout /mnt/phh/keylayout
    changed=false
    if grep -q vendor.huawei.hardware.biometrics.fingerprint /vendor/etc/vintf/manifest.xml; then
        changed=true
        cp /system/phh/huawei/fingerprint.kl /mnt/phh/keylayout/fingerprint.kl
        chmod 0644 /mnt/phh/keylayout/fingerprint.kl
    fi

    if getprop ro.vendor.build.fingerprint |
        grep -qE -e "^samsung"; then
        changed=true

        cp /system/phh/samsung-gpio_keys.kl /mnt/phh/keylayout/gpio_keys.kl
        cp /system/phh/samsung-sec_touchscreen.kl /mnt/phh/keylayout/sec_touchscreen.kl
        cp /system/phh/samsung-sec_touchkey.kl /mnt/phh/keylayout/sec_touchkey.kl
        chmod 0644 /mnt/phh/keylayout/gpio_keys.kl /mnt/phh/keylayout/sec_touchscreen.kl
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq \
        -e poco/ -e POCO/ -e redmi/ -e xiaomi/ ; then
        setprop persist.sys.phh.evgrab 'uinput-egis;uinput-goodix;uinput-fpc'
        if [ ! -f /mnt/phh/keylayout/uinput-goodix.kl ]; then
          cp /mnt/phh/empty /mnt/phh/keylayout/uinput-goodix.kl
          chmod 0644 /mnt/phh/keylayout/uinput-goodix.kl
          changed=true
        fi
        if [ ! -f /mnt/phh/keylayout/uinput-fpc.kl ]; then
          cp /mnt/phh/empty /mnt/phh/keylayout/uinput-fpc.kl
          chmod 0644 /mnt/phh/keylayout/uinput-fpc.kl
          changed=true
        fi
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq -e xiaomi/daisy; then
        cp /system/phh/daisy-buttonJack.kl ${mpk}/msm8953-snd-card-mtp_Button_Jack.kl
        changed=true
        if [ ! -f /mnt/phh/keylayout/uinput-goodix.kl ]; then
           cp /system/phh/daisy-uinput-goodix.kl ${mpk}/uinput-goodix.kl
           changed=true
        fi
        if [ ! -f /mnt/phh/keylayout/uinput-fpc.kl ]; then
           cp /system/phh/daisy-uinput-fpc.kl ${mpk}/uinput-fpc.kl
           changed=true
        fi
        chmod 0644 ${mpk}/uinput* ${mpk}/msm8953*
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq -e xiaomi/renoir; then
        mpk="/mnt/phh/keylayout"
        cp /system/phh/daisy-buttonJack.kl ${mpk}/lahaina-shimaidp-snd-card_Button_Jack.kl
        chmod 0644 ${mpk}/lahaina-shimaidp-snd-card_Button_Jack.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -iq -e samsung/a10sxx;then
        mkdir -p /data/vendor/mcRegistry
        chown system /data/vendor/mcRegistry
        chmod 755 /data/vendor/mcRegistry
    fi

    if getprop ro.vendor.build.fingerprint | grep -qi oneplus/oneplus6/oneplus6; then
        cp /system/phh/oneplus6-synaptics_s3320.kl /mnt/phh/keylayout/synaptics_s3320.kl
        chmod 0644 /mnt/phh/keylayout/synaptics_s3320.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq -e iaomi/perseus -e iaomi/cepheus; then
        cp /system/phh/mimix3-gpio-keys.kl /mnt/phh/keylayout/gpio-keys.kl
        chmod 0644 /mnt/phh/keylayout/gpio-keys.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq -E -e '^Sony/'; then
        cp /system/phh/sony-gpio-keys.kl /mnt/phh/keylayout/gpio-keys.kl
        chmod 0644 /mnt/phh/keylayout/gpio-keys.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -iq -E -e '^Nokia/Panther';then
        cp /system/phh/nokia-soc_gpio_keys.kl /mnt/phh/keylayout/soc_gpio_keys.kl
        chmod 0644 /mnt/phh/keylayout/soc_gpio_keys.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -iq -E -e '^Lenovo/' && [ -f /sys/devices/virtual/touch/tp_dev/gesture_on ];then
        cp /system/phh/lenovo-synaptics_dsx.kl /mnt/phh/keylayout/synaptics_dsx.kl
        chmod 0644 /mnt/phh/keylayout/synaptics_dsx.kl
        cp /system/phh/lenovo-synaptics_dsx.kl /mnt/phh/keylayout/fts_ts.kl
        chmod 0644 /mnt/phh/keylayout/fts_ts.kl
        changed=true
    fi

    if ( getprop ro.build.overlay.deviceid |grep -q -e RMX1931 -e RMX1941 -e CPH1859 -e CPH1861 -e RMX2185) ||
	    ( grep -q OnePlus /odm/etc/$(getprop ro.boot.prjname)/*.prop);then
	echo 1 > /proc/touchpanel/double_tap_enable
        cp /system/phh/oppo-touchpanel.kl /mnt/phh/keylayout/touchpanel.kl
	cp /system/phh/oppo-touchpanel.kl /mnt/phh/keylayout/mtk-tpd.kl
        chmod 0644 /mnt/phh/keylayout/touchpanel.kl
	chmod 0644 /mnt/phh/keylayout/mtk-tpd.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -q -e google/;then
        cp /system/phh/google-uinput-fpc.kl /mnt/phh/keylayout/uinput-fpc.kl
        chmod 0644 /mnt/phh/keylayout/uinput-fpc.kl
        changed=true
    fi

    if getprop ro.product.vendor.manufacturer |grep -q -e motorola;then
        cp /system/phh/moto-uinput-egis.kl /mnt/phh/keylayout/uinput-egis.kl
        cp /system/phh/moto-uinput-egis.kl /mnt/phh/keylayout/uinput-fpc.kl
        chmod 0644 /mnt/phh/keylayout/uinput-egis.kl
        chmod 0644 /mnt/phh/keylayout/uinput-fpc.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -q -e nubia/NX659;then
        cp /system/phh/nubia-nubia_synaptics_dsx.kl /mnt/phh/keylayout/nubia_synaptics_dsx.kl
        chmod 0644 /mnt/phh/keylayout/nubia_synaptics_dsx.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint |grep -q -i -e Teracube/Teracube_2e;then
        cp /system/phh/teracube2e-mtk-kpd.kl /mnt/phh/keylayout/mtk-kpd.kl
        chmod 0644 /mnt/phh/keylayout/mtk-kpd.kl
        changed=true
    fi

    if getprop ro.vendor.asus.build.fp |grep -q ASUS_I01WD;then
        cp /system/phh/zf6-goodixfp.kl /mnt/phh/keylayout/goodixfp.kl
        cp /system/phh/zf6-googlekey_input.kl /mnt/phh/keylayout/googlekey_input.kl
        chmod 0644 /mnt/phh/keylayout/goodixfp.kl
        chmod 0644 /mnt/phh/keylayout/googlekey_input.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -q -e Unihertz/;then
        cp /system/phh/unihertz-mtk-kpd.kl /mnt/phh/keylayout/mtk-kpd.kl
        cp /system/phh/unihertz-mtk-tpd.kl /mnt/phh/keylayout/mtk-tpd.kl
        cp /system/phh/unihertz-mtk-tpd-kpd.kl /mnt/phh/keylayout/mtk-tpd-kpd.kl
        cp /system/phh/unihertz-fingerprint_key.kl /mnt/phh/keylayout/fingerprint_key.kl
        chmod 0644 /mnt/phh/keylayout/mtk-kpd.kl
        chmod 0644 /mnt/phh/keylayout/mtk-tpd.kl
        chmod 0644 /mnt/phh/keylayout/mtk-tpd-kpd.kl
        chmod 0644 /mnt/phh/keylayout/fingerprint_key.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -q -i -e Blackview/BV9500Plus -e Ulefone/Armor_9 -e Blackview/BV9800;then
        cp /system/phh/bv9500plus-mtk-kpd.kl /mnt/phh/keylayout/mtk-kpd.kl
        chmod 0644 /mnt/phh/keylayout/mtk-kpd.kl
        changed=true
    fi

    if getprop ro.product.vendor.device |grep -qi -e mfh505glm -e fh50lm; then
        cp /mnt/phh/empty /mnt/phh/keylayout/uinput-fpc.kl
        chmod 0644 /mnt/phh/keylayout/uinput-fpc.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq -e motorola/liber; then
        cp /system/phh/moto-liber-gpio-keys.kl /mnt/phh/keylayout/gpio-keys.kl
        chmod 0644 /mnt/phh/keylayout/gpio-keys.kl

        cp /mnt/phh/empty /mnt/phh/keylayout/uinput_nav.kl
        chmod 0644 /mnt/phh/keylayout/uinput_nav.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -iq DOOGEE/S88Pro;then
          cp /mnt/phh/empty /mnt/phh/keylayout/sf-keys.kl
          chmod 0644 /mnt/phh/keylayout/sf-keys.kl
          changed=true
    fi

    if grep -qE ^cc: /proc/gesture_state;then
        cp /system/phh/tecno-touchpanel.kl /mnt/phh/keylayout/mtk-tpd.kl
        chmod 0644 /mnt/phh/keylayout/mtk-tpd.kl
        changed=true
    fi

    if getprop ro.vendor.build.fingerprint | grep -q -e nubia/NX669; then
        cp /system/phh/nubia-nubia_goodix_ts.kl /mnt/phh/keylayout/nubia_goodix_ts.kl
        chmod 0644 /mnt/phh/keylayout/nubia_goodix_ts.kl
        changed=true
    fi

    if [ "$changed" = true ]; then
        mount -o bind /mnt/phh/keylayout /system/usr/keylayout
        restorecon -R /system/usr/keylayout
    fi
}

if [ "$(getprop ro.product.vendor.manufacturer)" = motorola ] && getprop ro.vendor.product.name |grep -qE '^lima';then
    for l in lib lib64;do
        for f in mt6771 lima;do
            mount /mnt/phh/empty /vendor/$l/hw/keystore.$f.so
        done
    done
    setprop persist.sys.overlay.devinputjack true
fi

if ! getprop ro.vendor.build.fingerprint | grep -q -e samsung/;then
    if mount -o remount,rw /system; then
        resize2fs "$(grep ' /system ' /proc/mounts | cut -d ' ' -f 1)" || true
    else
        mount -o remount,rw /
        major="$(stat -c '%D' /.|sed -E 's/^([0-9a-f]+)([0-9a-f]{2})$/\1/g')"
        minor="$(stat -c '%D' /.|sed -E 's/^([0-9a-f]+)([0-9a-f]{2})$/\2/g')"
        mknod /dev/tmp-phh b $((0x$major)) $((0x$minor))
        blockdev --setrw /dev/tmp-phh
        resize2fs /dev/root || true
        resize2fs /dev/tmp-phh || true
    fi
    mount -o remount,ro /system || true
    mount -o remount,ro / || true
fi

for part in /dev/block/bootdevice/by-name/oppodycnvbk  /dev/block/platform/bootdevice/by-name/nvdata;do
    if [ -b "$part" ];then
        oppoName="$(grep -aohE '(RMX|CPH)[0-9]{4}' "$part" |head -n 1)"
        if [ -n "$oppoName" ];then
            setprop ro.build.overlay.deviceid "$oppoName"
        fi
    fi
done

# Disable touch firmware update on OPPO/Realme MTK devices to prevent
# kernel panic from td4320_nf driver during boot.  The driver triggers
# a kernel panic during firmware change on some devices (notably OPPO F11
# CPH1911 MT6771).  Writing 0 to the firmware update flag prevents the
# driver from attempting the update.
for node in /proc/touchpanel/fw_update /sys/devices/platform/tpd/fw_update; do
    if [ -f "$node" ]; then
        echo 0 > "$node" 2>/dev/null
        log -t rw-system "Disabled touch fw update at $node"
    fi
done

mkdir -p /mnt/phh/
mount -t tmpfs -o rw,nodev,relatime,mode=755,gid=0 none /mnt/phh || true
mkdir /mnt/phh/empty_dir
touch /mnt/phh/empty
touch /mnt/phh/unreadable
chmod 0 /mnt/phh/unreadable
if [ -n "$vndk" ] && [ "$vndk" -le 29 ]; then
    mount /mnt/phh/unreadable /vendor/etc/seccomp_policy/configstore@1.1.policy
fi
if ! getprop ro.product.vendor.model | grep -q -e oppo6771; then
    fixSPL
fi

changeKeylayout

mount /mnt/phh/empty /vendor/bin/vendor.samsung.security.proca@1.0-service || true

if grep vendor.huawei.hardware.biometrics.fingerprint /vendor/manifest.xml; then
    mount -o bind system/phh/huawei/fingerprint.kl /vendor/usr/keylayout/fingerprint.kl
fi

foundFingerprint=false
for manifest in /vendor/manifest.xml /vendor/etc/vintf /odm/etc/vintf;do
	if grep -q \
		-e android.hardware.biometrics.fingerprint \
		-e vendor.oppo.hardware.biometrics.fingerprint \
		-e vendor.oplus.hardware.biometrics.fingerprint \
		-r $manifest;then
			foundFingerprint=true
	fi
done

if [ "$foundFingerprint" = false ];then
    mount -o bind /mnt/phh/empty /system/etc/permissions/android.hardware.fingerprint.xml
fi

if ! grep android.hardware.bluetooth /vendor/manifest.xml && ! grep android.hardware.bluetooth /vendor/etc/vintf/manifest.xml; then
    mount -o bind /mnt/phh/empty /system/etc/permissions/android.hardware.bluetooth.xml
    mount -o bind /mnt/phh/empty /system/etc/permissions/android.hardware.bluetooth_le.xml
fi

if getprop ro.hardware | grep -qF qcom && [ -f /sys/class/backlight/panel0-backlight/max_brightness ] &&
    grep -qvE '^255$' /sys/class/backlight/panel0-backlight/max_brightness; then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/backlight/panel0-backlight/max_brightness)"
fi

#Sony don't use Qualcomm HAL, so they don't have their mess
if getprop ro.vendor.build.fingerprint | grep -qE 'Sony/'; then
    setprop persist.sys.qcom-brightness -1
fi

# Xiaomi MiA3 uses OLED display which works best with this setting
if getprop ro.vendor.build.fingerprint | grep -iq \
    -e iaomi/laurel_sprout;then
    setprop persist.sys.qcom-brightness -1
fi

# Lenovo Z5s & Xiaomi Mi10TLite brightness flickers without this setting
if getprop ro.vendor.build.fingerprint | grep -iq \
    -e Lenovo/jd2019 -e Xiaomi/gauguin -e Redmi/gauguin; then
    setprop persist.sys.qcom-brightness -1
fi

if getprop ro.vendor.build.fingerprint | grep -qi oneplus/oneplus6/oneplus6; then
    resize2fs /dev/block/platform/soc/1d84000.ufshc/by-name/userdata
fi

if getprop ro.vendor.build.fingerprint | grep -q full_k50v1_64 || getprop ro.hardware | grep -q mt6580; then
    setprop persist.sys.overlay.nightmode false
fi

if getprop ro.wlan.mtk.wifi.5g | grep -q 1; then
    setprop persist.sys.overlay.wifi5g true
fi

if grep -qF 'mkdir /data/.fps 0770 system fingerp' vendor/etc/init/hw/init.mmi.rc; then
    mkdir -p /data/.fps
    chmod 0770 /data/.fps
    chown system:9015 /data/.fps

    chown system:9015 /sys/devices/soc/soc:fpc_fpc1020/irq
    chown system:9015 /sys/devices/soc/soc:fpc_fpc1020/irq_cnt
fi

if getprop ro.vendor.build.fingerprint | grep -q -i \
    -e xiaomi/clover -e xiaomi/wayne -e xiaomi/sakura \
    -e xiaomi/nitrogen -e xiaomi/whyred -e xiaomi/platina \
    -e xiaomi/ysl -e nubia/nx60 -e nubia/nx61 -e xiaomi/tulip \
    -e xiaomi/lavender -e xiaomi/olive -e xiaomi/olivelite -e xiaomi/pine \
    -e POCO/evergreen; then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
fi

# Samsung Galaxy A20s (SM-A207F/M)
# Sets vendor.gsi.test to 0 because the stock vendor blocks camera HAL
# access to auxiliary cameras when this property is not set to 0 or is absent
# because then it would know that it isn't running the stock firmware.
if getprop ro.vendor.build.fingerprint | grep -qi "samsung/a20s"; then {
    setprop vendor.gsi.test 0
}; fi

#Realme 6
if getprop ro.vendor.product.device |grep -iq -e RMX2001 -e RMX2151 -e RMX2111 -e RMX2111L1;then
    setprop persist.sys.phh.fingerprint.nocleanup true
    setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
fi

if getprop ro.vendor.product.device |grep -iq -e RMX1801 -e RMX1803 -e RMX1807;then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
fi

if getprop ro.build.overlay.deviceid |grep -q -e CPH1859 -e CPH1861 -e RMX1811 -e RMX2185;then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
fi

if getprop ro.build.overlay.deviceid |grep -iq -e RMX2020 -e RMX2027 -e RMX2040 -e RMX2193 \
    -e RMX2193 -e RMX2191 -e RMX2195;then
    setprop persist.sys.qcom-brightness 2047
    setprop persist.sys.overlay.devinputjack true
    setprop persist.sys.phh.fingerprint.nocleanup true
fi

if getprop ro.vendor.build.fingerprint | grep -iq \
    -e xiaomi/beryllium/beryllium -e xiaomi/sirius/sirius \
    -e xiaomi/dipper/dipper -e xiaomi/ursa/ursa -e xiaomi/polaris/polaris \
    -e motorola/ali/ali -e xiaomi/perseus/perseus -e xiaomi/platina/platina \
    -e xiaomi/equuleus/equuleus -e motorola/nora -e xiaomi/nitrogen \
    -e motorola/hannah -e motorola/james -e motorola/pettyl -e xiaomi/cepheus \
    -e xiaomi/grus -e xiaomi/cereus -e xiaomi/cactus -e xiaomi/raphael -e xiaomi/davinci \
    -e xiaomi/ginkgo -e xiaomi/willow -e xiaomi/laurel_sprout -e xiaomi/andromeda \
    -e xiaomi/gauguin -e redmi/gauguin -e redmi/curtana -e redmi/picasso \
    -e bq/Aquaris_M10 ; then
    mount -o bind /mnt/phh/empty_dir /vendor/lib64/soundfx
    mount -o bind /mnt/phh/empty_dir /vendor/lib/soundfx
    setprop  ro.audio.ignore_effects true
fi

if getprop ro.vendor.build.fingerprint | grep -iq \
	-e bq/Aquaris_M10 ; then
	setprop ro.surface_flinger.primary_display_orientation ORIENTATION_90
fi

if getprop ro.build.fingerprint | grep -iq \
    -e motorola/channel; then
    mount -o bind /mnt/phh/empty_dir /vendor/lib64/soundfx
    mount -o bind /mnt/phh/empty_dir /vendor/lib/soundfx
    setprop ro.audio.ignore_effects true
fi

if [ "$(getprop ro.vendor.product.manufacturer)" = "motorola" ] || [ "$(getprop ro.product.vendor.manufacturer)" = "motorola" ]; then
    if getprop ro.vendor.product.device | grep -q -e nora -e ali -e hannah -e evert -e jeter -e deen -e james -e pettyl -e jater; then
        setprop  ro.audio.ignore_effects true
        if [ -n "$vndk" ] && [ "$vndk" -ge 28 ]; then
            f="/vendor/lib/libeffects.so"
            # shellcheck disable=SC2010
            ctxt="$(ls -lZ $f | grep -oE 'u:object_r:[^:]*:s0')"
            b="$(echo "$f" | tr / _)"

            cp -a $f "/mnt/phh/$b"
            sed -i \
                's/%zu errors during loading of configuration: %s/%zu errors during loading of configuration: ss/g' \
                "/mnt/phh/$b"
            chcon "$ctxt" "/mnt/phh/$b"
            mount -o bind "/mnt/phh/$b" $f
        else
            mount -o bind /mnt/phh/empty_dir /vendor/lib64/soundfx
            mount -o bind /mnt/phh/empty_dir /vendor/lib/soundfx
        fi
    fi
fi

if getprop ro.vendor.build.fingerprint | grep -q -i -e xiaomi/wayne -e xiaomi/jasmine; then
    setprop persist.imx376_sunny.low.lux 310
    setprop persist.imx376_sunny.light.lux 280
    setprop persist.imx376_ofilm.low.lux 310
    setprop persist.imx376_ofilm.light.lux 280
    echo "none" > /sys/class/leds/led:torch_2/trigger
fi

for f in /vendor/lib/mtk-ril.so /vendor/lib64/mtk-ril.so /vendor/lib/libmtk-ril.so /vendor/lib64/libmtk-ril.so; do
    [ ! -f $f ] && continue
    # shellcheck disable=SC2010
    ctxt="$(ls -lZ "$f" | grep -oE 'u:object_r:[^:]*:s0')"
    b="$(echo "$f" | tr / _)"

    cp -a "$f" "/mnt/phh/$b"
    sed -i \
        -e 's/AT+EAIC=2/AT+EAIC=3/g' \
        "/mnt/phh/$b"
    chcon "$ctxt" "/mnt/phh/$b"
    mount -o bind "/mnt/phh/$b" "$f"

    setprop persist.sys.phh.radio.force_cognitive true
    setprop persist.sys.radio.ussd.fix true
done

if getprop ro.vendor.build.fingerprint | grep -iq -e iaomi/cactus -e iaomi/cereus; then
    setprop debug.stagefright.omx_default_rank.sw-audio 1
    setprop debug.stagefright.omx_default_rank 0
fi

mount -o bind /mnt/phh/empty /vendor/lib/libpdx_default_transport.so
mount -o bind /mnt/phh/empty /vendor/lib64/libpdx_default_transport.so

mount -o bind /mnt/phh/empty /vendor/overlay/SysuiDarkTheme/SysuiDarkTheme.apk || true
mount -o bind /mnt/phh/empty /vendor/overlay/SysuiDarkTheme/SysuiDarkThemeOverlay.apk || true

if grep -qF 'PowerVR Rogue GE8100' /vendor/lib/egl/GLESv1_CM_mtk.so ||
    grep -qF 'PowerVR Rogue' /vendor/lib/egl/libGLESv1_CM_mtk.so ||
    ( (getprop ro.product.board; getprop ro.board.platform) | grep -qiE -e msm8917 -e msm8937 -e msm8940); then

    setprop debug.hwui.renderer opengl
    setprop ro.skia.ignore_swizzle true
    if [ -n "$vndk" ] && { [ "$vndk" = 26 ] || [ "$vndk" = 27 ]; }; then
       setprop debug.hwui.use_buffer_age false

    fi
fi

#If we have both Samsung and AOSP power hal, take Samsung's
if [ -f /vendor/bin/hw/vendor.samsung.hardware.miscpower@1.0-service ] && [ -n "$vndk" ] && [ "$vndk" -lt 28 ]; then
    mount -o bind /mnt/phh/empty /vendor/bin/hw/android.hardware.power@1.0-service
fi

if [ -n "$vndk" ] && { [ "$vndk" = 27 ] || [ "$vndk" = 26 ]; }; then
    mount -o bind /system/phh/libnfc-nci-oreo.conf /system/etc/libnfc-nci.conf
fi

if busybox_phh unzip -p /vendor/app/ims/ims.apk classes.dex | grep -qF -e Landroid/telephony/ims/feature/MmTelFeature -e Landroid/telephony/ims/feature/MMTelFeature; then
    mount -o bind /mnt/phh/empty /vendor/app/ims/ims.apk
fi

if getprop ro.hardware | grep -qF exynos; then
    setprop debug.sf.latch_unsignaled 1
fi

if getprop ro.product.model | grep -qF ANE; then
    setprop debug.sf.latch_unsignaled 1
fi

if getprop ro.vendor.product.device | grep -q -e nora -e rhannah; then
    setprop debug.sf.latch_unsignaled 1
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e xiaomi/daisy; then
    setprop debug.sf.latch_unsignaled 1
    setprop debug.sf.enable_hwc_vds 1
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e Redmi/merlin; then
    setprop debug.sf.latch_unsignaled 1
    setprop debug.sf.enable_hwc_vds 0
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e Redmi/lancelot -e Redmi/galahad; then
    setprop debug.renderengine.backend skiaglthreaded
    setprop debug.sf.use_phase_offsets_as_durations 1
    setprop debug.sf.late.sf.duration 27600000
    setprop debug.sf.late.app.duration 20000000
    setprop debug.sf.early.sf.duration 27600000
    setprop debug.sf.early.app.duration 20000000
    setprop debug.sf.earlyGl.sf.duration 27600000
    setprop debug.sf.earlyGl.app.duration 20000000
    setprop debug.sf.hwc.min.duration 17000000
    setprop debug.sf.disable_client_composition_cache 1
    setprop debug.sf.predict_hwc_composition_strategy 0
    setprop debug.stagefright.c2inputsurface -1
    setprop media.stagefright.thumbnail.prefer_hw_codecs true
    setprop ro.surface_flinger.enable_frame_rate_override false
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e Redmi/rosemary \
    -e Redmi/secret -e Redmi/maltose; then
    setprop debug.sf.latch_unsignaled 1
    setprop debug.sf.enable_hwc_vds 0

    # Exclude FP input devices
    mount -o bind /system/phh/rosemary-excluded-input-devices.xml /system/etc/excluded-input-devices.xml
fi

if getprop ro.vendor.build.fingerprint | grep -iq -E -e 'huawei|honor' || getprop persist.sys.overlay.huawei | grep -iq -E -e 'true'; then
    p=/product/etc/nfc/libnfc_nxp_*_*.conf
    mount -o bind "$p" /system/etc/libnfc-nxp.conf ||
        mount -o bind /product/etc/libnfc-nxp.conf /system/etc/libnfc-nxp.conf || true

    p=/product/etc/nfc/libnfc_brcm_*_*.conf
    mount -o bind "$p" /system/etc/libnfc-brcm.conf ||
        mount -o bind /product/etc/libnfc-nxp.conf /system/etc/libnfc-nxp.conf || true

    mount -o bind /system/phh/libnfc-nci-huawei.conf /system/etc/libnfc-nci.conf
fi

if getprop ro.vendor.build.fingerprint | grep -qE -e ".*(crown|star)[q2]*lte.*" -e ".*(SC-0[23]K|SCV3[89]).*" && [ -n "$vndk" ] && [ "$vndk" -lt 28 ]; then
    for f in /vendor/lib/libfloatingfeature.so /vendor/lib64/libfloatingfeature.so; do
        [ ! -f "$f" ] && continue
        # shellcheck disable=SC2010
        ctxt="$(ls -lZ "$f" | grep -oE 'u:object_r:[^:]*:s0')"
        b="$(echo "$f" | tr / _)"

        cp -a "$f" "/mnt/phh/$b"
        sed -i \
            -e 's;/system/etc/floating_feature.xml;/system/ph/sam-9810-flo_feat.xml;g' \
            "/mnt/phh/$b"
        chcon "$ctxt" "/mnt/phh/$b"
        mount -o bind "/mnt/phh/$b" "$f"

	setprop ro.audio.monitorRotation true
    done
fi

# This matches both Razer Phone 1 & 2
if getprop ro.vendor.build.fingerprint |grep -qE razer/cheryl;then
	setprop ro.audio.monitorRotation true
	mount -o bind /mnt/phh/empty /vendor/overlay/BluetoothResCommon.apk
	mount -o bind /mnt/phh/empty /vendor/overlay/RazerCherylBluetoothRes.apk
fi

if getprop ro.vendor.build.fingerprint | grep -qiE '^samsung'; then
    if getprop ro.hardware | grep -q qcom; then
        setprop persist.sys.overlay.devinputjack false
    fi

    if getprop ro.hardware | grep -q -e samsungexynos7870 -e qcom; then
        if [ -n "$vndk" ] && [ "$vndk" -le 27 ]; then
            setprop persist.sys.phh.sdk_override /vendor/bin/hw/rild=27
        fi
    fi
fi

if getprop ro.vendor.build.fingerprint | grep -qE '^xiaomi/wayne/wayne.*'; then
    # Fix camera on DND, ugly workaround but meh
    setprop audio.camerasound.force true
fi

if [ $(find /vendor/etc/audio -type f |wc -l) -le 3 ];then
	mount -o bind /mnt/phh/empty_dir /vendor/etc/audio || true
fi

for f in /vendor/lib{,64}/hw/com.qti.chi.override.so /vendor/lib{,64}/libVD*;do
    [ ! -f $f ] && continue
    # shellcheck disable=SC2010
    ctxt="$(ls -lZ "$f" | grep -oE 'u:object_r:[^:]*:s0')"
    b="$(echo "$f" | tr / _)"

    cp -a "$f" "/mnt/phh/$b"
    sed -i \
        -e 's/ro.product.manufacturer/sys.phh.xx.manufacturer/g' \
        -e 's/ro.product.brand/sys.phh.xx.brand/g' \
        -e 's/ro.product.model/sys.phh.xx.model/g' \
        "/mnt/phh/$b"
    chcon "$ctxt" "/mnt/phh/$b"
    mount -o bind "/mnt/phh/$b" "$f"

    manufacturer=$(getprop ro.product.vendor.manufacturer)
    [ -z "$manufacturer" ] && manufacturer=$(getprop ro.product.manufacturer)
    model=$(getprop ro.product.vendor.model)
    [ -z "$model" ] && model=$(getprop ro.product.odm.model)
    setprop sys.phh.xx.manufacturer "$manufacturer"
    setprop sys.phh.xx.brand "$(getprop ro.product.vendor.brand)"
    setprop sys.phh.xx.model "$model"
done

if [ -n "$(getprop ro.boot.product.hardware.sku)" ] && [ -z "$(getprop ro.hw.oemName)" ];then
	setprop ro.hw.oemName "$(getprop ro.boot.product.hardware.sku)"
fi

if getprop ro.vendor.build.fingerprint | grep -qiE '^samsung/' && [ -n "$vndk" ] && [ "$vndk" -ge 28 ];then
	setprop persist.sys.phh.samsung_fingerprint 0
	#obviously broken perms
	if [ "$(stat -c '%U' /sys/class/sec/tsp/cmd)" == "root" ] &&
		[ "$(stat -c '%G' /sys/class/sec/tsp/cmd)" == "root" ];then

		chcon u:object_r:sysfs_ss_writable:s0 /sys/class/sec/tsp/ear_detect_enable
		chown system /sys/class/sec/tsp/ear_detect_enable

		chcon u:object_r:sysfs_ss_writable:s0 /sys/class/sec/tsp/cmd{,_list,_result,_status}
		chown system /sys/class/sec/tsp/cmd{,_list,_result,_status}

		chown system /sys/class/power_supply/battery/wc_tx_en
		chcon u:object_r:sysfs_app_writable:s0 /sys/class/power_supply/battery/wc_tx_en
	fi

	if [ "$(stat -c '%U' /sys/class/sec/tsp/input/enabled)" == "root" ] &&
		[ "$(stat -c '%G' /sys/class/sec/tsp/input/enabled)" == "root" ];then
			chown system:system /sys/class/sec/tsp/input/enabled
			chcon u:object_r:sysfs_ss_writable:s0 /sys/class/sec/tsp/input/enabled
			setprop ctl.restart sec-miscpower-1-0
	fi
	if [ "$(stat -c '%U' /sys/class/camera/flash/rear_flash)" == "root" ] &&
		[ "$(stat -c '%G' /sys/class/camera/flash/rear_flash)" == "root" ];then
        chown system:system /sys/class/camera/flash/rear_flash
        chcon u:object_r:sysfs_camera_writable:s0 /sys/class/camera/flash/rear_flash
    fi
fi

# For Nubia Red Magic 6 audio policy configuration
if getprop ro.vendor.build.fingerprint | grep -q -e nubia/NX669; then
    umount /vendor/etc/audio
    sku="$(getprop ro.boot.product.vendor.sku)"
    mount /vendor/etc/audio/sku_${sku}_qssi/audio_policy_configuration.xml /vendor/etc/audio/sku_$sku/audio_policy_configuration.xml
    chmod 0666 /sys/kernel/lcd_enhance/hbm_state
    # Disable back panel touch (which would hide keyboard if accidentally touched)
    setprop persist.sys.phh.evgrab nubia_sar0_channel0
fi

# For ZF8, the "best" audio policy isn't the one for QSSI
if getprop ro.vendor.build.fingerprint |grep -q -e /ASUS_I006D:;then
    umount /vendor/etc/audio
    sku="$(getprop ro.boot.product.vendor.sku)"
    mount /vendor/etc/audio/ZS590KS/audio_policy_configuration_ZS590KS.xml /vendor/etc/audio/sku_$sku/audio_policy_configuration.xml
fi

# For G75 5G, fix internet blocks caused by forbidden string "paros" in the user agent
if getprop ro.vendor.build.fingerprint | grep -iq -e motorola/paros; then
    resetprop_phh ro.product.vendor.model "moto g75 5G"
fi

setprop ctl.stop console
copyprop() {
    p="$(getprop "$2")"
    if [ "$p" ]; then
        resetprop_phh "$1" "$(getprop "$2")"
    fi
}

# OPPO/Realme devices with MTK vendor kernels include an oppo_root_check
# kernel module or init service that detects an unlocked bootloader and forces
# a reboot to recovery.  set the boot-state properties to "locked" so any
# userspace component of the check sees a locked device.
#
# only apply the bypass if the vendor actually contains the root-check
# service. custom ROM vendors (e.g. InfinityX) often remove this service,
# and the property spoofing causes a ~100s boot delay on those devices.
brand="$(getprop ro.product.vendor.brand)"
if [ "$brand" = OPPO ] || [ "$brand" = realme ]; then
    if grep -r -q "oppo_root_check" /vendor/etc/init/ 2>>/dev/null; then
        resetprop_phh ro.boot.flash.locked 1
        resetprop_phh ro.boot.vbmeta.device_state locked
        resetprop_phh ro.boot.verifiedbootstate green
        resetprop_phh ro.boot.warranty_bit 0
        resetprop_phh ro.vendor.warranty_bit 0
        resetprop_phh ro.warranty_bit 0
        resetprop_phh -n sys.oem_unlock_allowed 0
        setprop ctl.stop oppo_root_check 2>>/dev/null
    fi
fi

# Detect GSI system image changes before securize overwrites fingerprints.
# PackageManagerService uses PackagePartitions.FINGERPRINT (a SHA-1 digest of
# all ro.*.build.fingerprint properties) to detect upgrades and set mIsUpgrade.
# Because securize replaces every system fingerprint property with the vendor
# fingerprint, PMS always sees the same digest after first boot and mIsUpgrade
# is always false, even after a real GSI upgrade.
#
# To work around this we compare the original (pre-securize) system fingerprint
# against a persisted value and, when they differ, set persist.pm.mock-upgrade=true
# which PMS ORs into isDeviceUpgrading(). The same comparison gates the
# (no longer used for adb; release builds keep adb fully disabled.)
#
# Storage selection: neither /cache nor /metadata is universally available
# at the time rw-system.sh runs.
#   /metadata: real persistent partition on post-Q A/B devices; tmpfs on
#              pre-Q devices without a metadata partition (see 00-fix-metadata.sh).
#   /cache:    real persistent partition on non-A/B devices; a symlink to
#              /data/cache on A/B devices, but /data is not yet mounted at
#              this point in boot so writes would be silently lost.
# Strategy: prefer /metadata if it is a real mountpoint (not tmpfs), else
# prefer /cache if it is a real mountpoint, else skip persistence (best-effort).
gsi_fp="$(getprop ro.system.build.fingerprint)"
gsi_fp_dir=""
if mountpoint -q /metadata && [ "$(stat -f -c %T /metadata)" != "tmpfs" ]; then
    gsi_fp_dir="/metadata/phh"
elif mountpoint -q /cache; then
    gsi_fp_dir="/cache/phh"
fi
gsi_fp_file=""
prev_gsi_fp=""
if [ -n "$gsi_fp_dir" ]; then
    mkdir -p "$gsi_fp_dir"
    gsi_fp_file="$gsi_fp_dir/gsi_fingerprint"
    [ -f "$gsi_fp_file" ] && prev_gsi_fp="$(cat "$gsi_fp_file")"
fi
if [ -n "$gsi_fp" ] && [ -n "$prev_gsi_fp" ] && [ "$gsi_fp" != "$prev_gsi_fp" ]; then
    # fingerprint changed: GSI upgrade detected.
    [ -n "$gsi_fp_file" ] && echo "$gsi_fp" > "$gsi_fp_file"
    # signal PMS to run post-upgrade steps.
    resetprop_phh -n persist.pm.mock-upgrade true
elif [ -n "$gsi_fp" ] && [ -z "$prev_gsi_fp" ]; then
    # no prior fingerprint: fresh flash, factory reset, or no persistent storage.
    [ -n "$gsi_fp_file" ] && echo "$gsi_fp" > "$gsi_fp_file"
    resetprop_phh -n persist.pm.mock-upgrade false
else
    # same build as last boot: clear upgrade flag.
    resetprop_phh -n persist.pm.mock-upgrade false
fi

if [ ! -f /metadata/securize_disable ]; then
    copyprop ro.build.device ro.vendor.build.device
    copyprop ro.system.build.fingerprint ro.vendor.build.fingerprint
    copyprop ro.bootimage.build.fingerprint ro.vendor.build.fingerprint
    copyprop ro.build.fingerprint ro.vendor.build.fingerprint
    copyprop ro.system_ext.build.fingerprint ro.vendor.build.fingerprint
    copyprop ro.product.build.fingerprint ro.vendor.build.fingerprint
    copyprop ro.build.device ro.vendor.product.device
    copyprop ro.product.system.device ro.vendor.product.device
    copyprop ro.product.device ro.vendor.product.device
    copyprop ro.product.system.device ro.product.vendor.device
    copyprop ro.product.device ro.product.vendor.device
    copyprop ro.product.system_ext.device ro.vendor.product.device
    copyprop ro.product.product.device ro.vendor.product.device
    copyprop ro.product.system_ext.device ro.product.vendor.device
    copyprop ro.product.product.device ro.product.vendor.device
    copyprop ro.product.system.name ro.vendor.product.name
    copyprop ro.product.name ro.vendor.product.name
    copyprop ro.product.system.name ro.product.vendor.name
    copyprop ro.product.name ro.product.vendor.name
    copyprop ro.product.system_ext.name ro.vendor.product.name
    copyprop ro.product.product.name ro.vendor.product.name
    copyprop ro.product.system_ext.name ro.product.vendor.name
    copyprop ro.product.product.name ro.product.vendor.name
    copyprop ro.system.product.brand ro.vendor.product.brand
    copyprop ro.product.brand ro.vendor.product.brand
    copyprop ro.product.system.brand ro.vendor.product.brand
    copyprop ro.product.system_ext.brand ro.vendor.product.brand
    copyprop ro.product.product.brand ro.product.vendor.brand
    copyprop ro.system.product.brand ro.product.vendor.brand
    copyprop ro.product.brand ro.product.vendor.brand
    copyprop ro.product.system.brand ro.product.vendor.brand
    copyprop ro.product.system_ext.brand ro.product.vendor.brand
    copyprop ro.product.product.brand ro.product.vendor.brand
    copyprop ro.product.system.model ro.vendor.product.model
    copyprop ro.product.model ro.vendor.product.model
    copyprop ro.product.system_ext.model ro.vendor.product.model
    copyprop ro.product.product.model ro.vendor.product.model
    copyprop ro.product.system.model ro.product.vendor.model
    copyprop ro.product.model ro.product.vendor.model
    copyprop ro.build.product ro.vendor.product.model
    copyprop ro.build.product ro.product.vendor.model
    copyprop ro.product.system_ext.model ro.product.vendor.model
    copyprop ro.product.product.model ro.product.vendor.model
    copyprop ro.system.product.manufacturer ro.vendor.product.manufacturer
    copyprop ro.product.manufacturer ro.vendor.product.manufacturer
    copyprop ro.product.system.manufacturer ro.vendor.product.manufacturer
    copyprop ro.product.product.manufacturer ro.vendor.product.manufacturer
    copyprop ro.product.system_ext.manufacturer ro.vendor.product.manufacturer
    copyprop ro.system.product.manufacturer ro.product.vendor.manufacturer
    copyprop ro.product.manufacturer ro.product.vendor.manufacturer
    copyprop ro.product.system.manufacturer ro.product.vendor.manufacturer
    copyprop ro.product.product.manufacturer ro.product.vendor.manufacturer
    copyprop ro.product.system_ext.manufacturer ro.product.vendor.manufacturer

    # spoof security-sensitive props early so root detection apps cannot read
    # the real values before phh-on-boot.sh reinforces them post-boot
    resetprop_phh ro.debuggable 0
    resetprop_phh ro.secure 1

    # update usb gadget strings so the device appears correctly when connected
    model="$(getprop ro.product.model)"
    manufacturer="$(getprop ro.product.manufacturer)"
    gadget_strings="/config/usb_gadget/g1/strings/0x409"
    legacy_usb="/sys/class/android_usb/android0"
    if [ -d "$gadget_strings" ]; then
        [ -n "$model" ] && echo "$model" > "$gadget_strings/product"
        [ -n "$manufacturer" ] && echo "$manufacturer" > "$gadget_strings/manufacturer"
    elif [ -d "$legacy_usb" ]; then
        [ -n "$model" ] && echo "$model" > "$legacy_usb/iProduct"
        [ -n "$manufacturer" ] && echo "$manufacturer" > "$legacy_usb/iManufacturer"
    fi

    # Hide system/xbin/su
    mount /mnt/phh/empty_dir /system/xbin
    mount /mnt/phh/empty_dir /system/app/me.phh.superuser
    mount /mnt/phh/empty /system/xbin/phh-su
else
    mkdir /mnt/phh/xbin
    chmod 0755 /mnt/phh/xbin
    chcon u:object_r:system_file:s0 /mnt/phh/xbin

    #phh-su will bind over this empty file to make a real su
    touch /mnt/phh/xbin/su
    chcon u:object_r:system_file:s0 /mnt/phh/xbin/su

    mount -o bind /mnt/phh/xbin /system/xbin
fi

for abi in "" 64;do
    f=/vendor/lib$abi/libstagefright_foundation.so
    if [ -f "$f" ];then
        for vndk in 26 27 28 29;do
            mount "$f" /system/system_ext/apex/com.android.vndk.v$vndk/lib$abi/libstagefright_foundation.so
        done
    fi
done

[ -n "$vndk" ] && setprop ro.product.first_api_level "$vndk"

if getprop ro.boot.boot_devices |grep -v , |grep -qE .;then
    ln -s /dev/block/platform/$(getprop ro.boot.boot_devices) /dev/block/bootdevice
fi

if [ -c /dev/dsm ];then
    # /dev/dsm is a magic device on Kirin chipsets that teecd needs to access.
    # Make sure that permissions are right.
    chown system:system /dev/dsm
    chmod 0660 /dev/dsm

    # The presence of /dev/dsm indicates that we have a teecd,
    # which needs /sec_storage and /data/sec_storage_data

    mkdir -p /data/sec_storage_data
    chown system:system /data/sec_storage_data
    chcon -R u:object_r:teecd_data_file:s0 /data/sec_storage_data

    if mount | grep -q " on /sec_storage " ; then
        # /sec_storage is already mounted by the vendor, don't try to create and mount it
        # ourselves. However, some devices have /sec_storage owned by root, which means that
        # the fingerprint daemon (running as system) cannot access it.
        chown -R system:system /sec_storage
        chmod -R 0660 /sec_storage
        chcon -R u:object_r:teecd_data_file:s0 /sec_storage
    else
        # No /sec_storage provided by vendor, mount /data/sec_storage_data to it
        mount /data/sec_storage_data /sec_storage
        chown system:system /sec_storage
        chcon u:object_r:teecd_data_file:s0 /sec_storage
    fi
fi

#Weird /odm/phone.prop Huawei stuff
HW_PRODID="$(sed -nE 's/.*productid=([0-9xa-f]*).*/\1/p' /proc/cmdline)"
[ -z "$HW_PRODID" ] && HW_PRODID="0x$(od -A none -t x1 /sys/firmware/devicetree/base/hisi,modem_id | sed s/' '//g)"
for part in odm vendor;do
    if [ -f /$part/phone.prop ];then
        if [ -n "$HW_PRODID" ];then
            eval "$(awk 'BEGIN { a=0 }; /\[.*\].*/ { a=0 }; tolower($0) ~ /.*'"$HW_PRODID"'.*/ { a=1 }; /.*=.*/ { if(a == 1) print $0 }' /$part/phone.prop |sed -nE 's/(.*)=(.*)/setprop \1 "\2";/p')"
        fi
    fi
done

# Fix sprd adf for surfaceflinger to start
# Somehow the names of the device nodes are incorrect on Android 10; fix them by mknod
if [ -e /dev/sprd-adf-dev ];then
    mknod -m666 /dev/adf0 c 250 0
    mknod -m666 /dev/adf-interface0.0 c 250 1
    mknod -m666 /dev/adf-overlay-engine0.0 c 250 2
    restorecon /dev/adf0 /dev/adf-interface0.0 /dev/adf-overlay-engine0.0

    # SPRD GL causes crashes in system_server (not currently observed in other processes)
    # Tell the system to avoid using hardware acceleration in system_server.
    setprop ro.config.avoid_gfx_accel true
fi

# Fix sensor services crashing on SPRD devices with Pie vendor
if getprop ro.hardware.keystore | grep -iq sprd && [ -n "$vndk" ] && [ "$vndk" -le 28 ]; then
    setprop persist.sys.phh.disable_sensor_direct_report true
fi

# Fix manual network selection with old modem
# https://github.com/LineageOS/android_hardware_ril/commit/e3d006fa722c02fc26acdfcaa43a3f3a1378eba9
if getprop ro.vendor.build.fingerprint | grep -iq \
    -e xiaomi/polaris -e xiaomi/whyred; then
    setprop persist.sys.phh.radio.use_old_mnc_format true
fi

if getprop ro.build.overlay.deviceid |grep -qE '^RMX';then
    resetprop_phh ro.vendor.gsi.build.flavor byPass
    setprop oppo.camera.packname com.oppo.engineermode.camera
    setprop sys.phh.xx.brand realme
fi

if [ -f /sys/firmware/devicetree/base/oppo,prjversion ];then
    setprop ro.separate.soft $((0x$(od -w4 -j4  -An -tx1 /sys/firmware/devicetree/base/oppo,prjversion |tr -d ' ' |head -n 1)))
fi

if [ -f /proc/oppoVersion/prjVersion ];then
    setprop ro.separate.soft $(cat /proc/oppoVersion/prjVersion)
fi

if grep -q -F ro.separate.soft /odm/build.prop;then
	setprop ro.separate.soft "$(sed -nE 's/^ro.separate.soft=(.*)/\1/p' /odm/build.prop)"
fi

echo 1 >  /proc/tfa98xx/oppo_tfa98xx_fw_update
if ! grep -q -E -e '.*#write .*tp_fw_update' /vendor/etc/init/hw/*touch*;then
	echo 1 > /proc/touchpanel/tp_fw_update
fi

if getprop ro.build.overlay.deviceid |grep -qE '^RMX';then
    chmod 0660 /sys/devices/platform/soc/soc:fpc_fpc1020/{irq,irq_enable,wakelock_enable}
    if [ "$(stat -c '%U' /sys/devices/platform/soc/soc:fpc_fpc1020/irq)" == "root" ] &&
		[ "$(stat -c '%G' /sys/devices/platform/soc/soc:fpc_fpc1020/irq)" == "root" ];then
            chown system:system /sys/devices/platform/soc/soc:fpc_fpc1020/{irq,irq_enable,wakelock_enable}
            setprop persist.sys.phh.fingerprint.nocleanup true
    fi
fi

if [ -n "$vndk" ] && [ "$vndk" -le 28 ] && getprop ro.hardware |grep -q -e mt6761 -e mt6763 -e mt6765 -e mt6785 -e mt8768 -e mt6779 -e mt6771 -e mt8766;then
    setprop debug.stagefright.ccodec 0
fi

if getprop ro.omc.build.version |grep -qE .;then
	for f in $(find /odm -name \*.apk);do
		mount /mnt/phh/empty $f
	done
fi

if getprop ro.vendor.build.fingerprint |grep -qiE \
        -e Nokia/Plate2 \
        -e razer/cheryl ; then
    setprop media.settings.xml "/vendor/etc/media_profiles_vendor.xml"
fi
resetprop_phh service.adb.root 0

# This is for Samsung Galaxy devices with HBM FOD
# On those devices, a magic Layer usageBits switches to "mask_brightness"
# But default is 255, so set it to max instead
cat /sys/class/backlight/*/max_brightness |sort -n |tail -n 1 > /sys/class/lcd/panel/mask_brightness

if getprop ro.vendor.build.fingerprint |grep -qiE '^xiaomi/';then
    setprop persist.sys.phh.fod.xiaomi true
fi

if getprop ro.vendor.build.fingerprint |grep -qiE '^samsung/' && \
        grep -q sysfs_lcd_writable /vendor/etc/selinux/vendor_file_contexts && \
        ! grep -q vendor_sysfs_graphics /vendor/etc/selinux/vendor_file_contexts ;then
    for f in /sys/class/lcd/panel/actual_mask_brightness /sys/class/lcd/panel/mask_brightness /sys/class/lcd/panel/device/backlight/panel/brightness /sys/class/backlight/panel0-backlight/brightness;do
        if [ "$(stat -c '%U' "$f")" == "root" ] || [ "$(ls -lZ "$f" | grep -oE 'u:object_r:[^:]*:s0')" == "u:object_r:sysfs:s0" ];then
            chcon u:object_r:sysfs_lcd_writable:s0 $f
            chmod 0644 $f
            chown system:system $f
        fi
    done

    setprop persist.sys.phh.fod.samsung true
fi

if getprop ro.vendor.build.fingerprint | grep -q -e samsung/o1s -e samsung/t2s -e samsung/p3s; then
    setprop persist.sys.phh.ultrasonic_udfps true
fi

if getprop ro.vendor.build.fingerprint |grep -qiE -e ASUS_I006D -e ASUS_I003;then
	setprop persist.sys.phh.fod.asus true
fi

# For Asus usb port picker
setprop sys.usb.all_controllers "$(ls /sys/class/udc |tr ' ' ',')"

if (getprop ro.vendor.build.fingerprint;getprop ro.odm.build.fingerprint) |grep -qiE '^oneplus/' ||
	getprop ro.build.overlay.deviceid |grep -qiE -e '^RMX' -e '^CPH' ||
	[ -n "$(getprop ro.separate.soft)" ];then
    setprop persist.sys.phh.fod.bbk true
fi

if getprop ro.build.overlay.deviceid |grep -iq -e RMX1941 -e RMX1945 -e RMX1943 -e RMX1942;then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
    setprop persist.sys.phh.mainkeys 0
fi

if getprop ro.build.overlay.deviceid |grep -iq -e RMX2185 -e RMX1941 -e RMX1945 -e RMX1943 -e RMX1942;then
    setprop persist.sys.overlay.devinputjack true
fi


board="$(getprop ro.board.platform)"

if [ "$board" = atoll ] || [ "$board" = sm6250 ]; then
	setprop ro.netflix.bsp_rev Q6250-19132-1
fi

if [ "$board" = msmnile ]; then
	setprop ro.netflix.bsp_rev Q855-16947-1
fi

if [ "$board" = sm6150 ]; then
	setprop ro.netflix.bsp_rev Q6150-17263-1
fi

if [ "$board" = mt6768 ]; then
	setprop ro.netflix.bsp_rev MTK6768-19055-1
fi

if [ "$board" = lahaina ]; then
	setprop ro.netflix.bsp_rev Q875-32774-1
fi

if [ "$board" = universal8825 ];then
	setprop ro.netflix.bsp_rev EXYNOS1280-34993-1
fi

if getprop ro.vendor.build.fingerprint |grep -qi Nokia/Phoenix;then
    setprop ro.netflix.bsp_rev Q845-05000-1
    setprop debug.sf.latch_unsignaled 1
    setprop sys.use_fifo_ui 1
    setprop media.settings.xml "/vendor/etc/media_profiles_vendor.xml"
fi

# Set props for Vsmart Live's fod
if getprop ro.vendor.build.fingerprint |grep -q vsmart/V620A_open;then
    setprop persist.sys.fp.fod.location.X_Y 447,1812
    setprop persist.sys.fp.fod.size.width_height 186,186
fi

setprop vendor.display.res_switch_en 1

if getprop ro.bionic.cpu_variant |grep -q kryo300;then
    resetprop_phh ro.bionic.cpu_variant cortex-a75
    setprop dalvik.vm.isa.arm64.variant cortex-a75
    setprop dalvik.vm.isa.arm64.features runtime
fi


if [ -f /vendor/etc/init/vendor.ozoaudio.media.c2@1.0-service.rc ];then
    if [ -n "$vndk" ] && [ "$vndk" -le 29 ]; then
        mount /system/etc/seccomp_policy/mediacodec.policy /vendor/etc/seccomp_policy/codec2.vendor.base.policy
    fi
fi

if [ -n "$vndk" ] && [ "$vndk" -le 27 ];then
    setprop persist.sys.phh.no_present_or_validate true
fi

[ -d /mnt/vendor/persist ] && mount /mnt/vendor/persist /persist

if [ -e /sys/bus/i2c/drivers/fts_ts ] || [ -e /sys/bus/spi/drivers/fts_ts ]; then
    for f in $(find /sys -name fts_gesture_mode 2>/dev/null);do
        setprop persist.sys.phh.focaltech_node "$f"
    done
fi

if [ -n "$vndk" ] && [ "$vndk" -le 27 ] && [ -f /vendor/bin/mnld ];then
    setprop persist.sys.phh.sdk_override /vendor/bin/mnld=26
fi

if [ -n "$vndk" ] && [ "$vndk" -le 30 ];then
	# on older vendors color management was disabled by default; use -n so
	# we only set a fallback and never override a value already present
	resetprop_phh -n ro.surface_flinger.use_color_management false
fi

if [ "$(stat -c '%U'  /dev/nxp_smartpa_dev)" == "root" ] &&
	[ "$(stat -c '%G' /dev/nxp_smartpa_dev)" == "root" ];then
    chown root:audio /dev/nxp_smartpa_dev
    chmod 0660 /dev/nxp_smartpa_dev
fi
if getprop ro.odm.build.fingerprint |grep -q Huawei/Chicago/Chicago_VTR;then
    setprop ctl.stop aptouch
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e GOME/GOME_U9; then
    cp -a /system/etc/smartpa_params /mnt/phh/smartpa_params
    cp /system/phh/gome/fs16xx_01s_left.preset /mnt/phh/smartpa_params/fs16xx_01s_left.preset
    cp /system/phh/gome/fs16xx_01s_mono.preset /mnt/phh/smartpa_params/fs16xx_01s_mono.preset
    cp /system/phh/gome/fs16xx_01s_right.preset /mnt/phh/smartpa_params/fs16xx_01s_right.preset
    chmod 0644 /mnt/phh/smartpa_params/fs16xx_01s_left.preset
    chmod 0644 /mnt/phh/smartpa_params/fs16xx_01s_mono.preset
    chmod 0644 /mnt/phh/smartpa_params/fs16xx_01s_right.preset
    mount -o bind /mnt/phh/smartpa_params /system/etc/smartpa_params
    restorecon -R /system/etc/smartpa_params
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e UMIDIGI/UMIDIGI_X; then
    cp -a /system/etc/smartpa_params /mnt/phh/smartpa_params
    cp /system/phh/umidigi/fs16xx_01s_mono.preset /mnt/phh/smartpa_params/fs16xx_01s_mono.preset
    chmod 0644 /mnt/phh/smartpa_params/fs16xx_01s_mono.preset
    mount -o bind /mnt/phh/smartpa_params /system/etc/smartpa_params
    restorecon -R /system/etc/smartpa_params
fi

if getprop ro.vendor.build.fingerprint |grep -iq motorola/kane;then
	setprop persist.sys.bt.esco_transport_unit_size 16
fi

if [ -f /vendor/bin/ccci_rpcd ];then
    setprop debug.phh.props.ccci_rpcd vendor
fi

if getprop ro.vendor.build.fingerprint | grep -iq -e motorola/liber; then
  cp /vendor/etc/audio_policy_configuration.xml /mnt/phh/
  sed -i '/r_submix_audio_policy_configuration/a \t<xi:include href="/vendor/etc/a2dp_audio_policy_configuration.xml"/>' /mnt/phh/audio_policy_configuration.xml
  mount -o bind /mnt/phh/audio_policy_configuration.xml /vendor/etc/audio_policy_configuration.xml
  chcon -h u:object_r:vendor_configs_file:s0 /vendor/etc/audio_policy_configuration.xml
  chmod 644 /vendor/etc/audio_policy_configuration.xml
fi

mount /mnt/phh/empty /vendor/etc/permissions/samsung.hardware.uwb.xml
mount /mnt/phh/empty /vendor/bin/install-recovery.sh

if getprop ro.vendor.radio.default_network |grep -qE '[0-9]';then
  setprop ro.telephony.default_network $(getprop ro.vendor.radio.default_network)
fi

if getprop ro.vendor.build.fingerprint |grep -iq redmi/camellia;then
	setprop persist.sys.qcom-brightness 4095
fi

mount -o bind /mnt/phh/empty_dir /vendor/app/qti-logkit
mount -o bind /mnt/phh/empty_dir /vendor/app/qti-logkit-lite

# Redirect vendor props for QCOM hwcomposer
setprop debug.phh.props.omposer-service vendor

# On those Unisoc chips, Android's bluetooth stack will try to send a LE_EXTENDED_SCAN command, which isn't actually supported
# The support of that command inherits from a "le vendor version". Force this at 0 to disable the use of that command
if getprop ro.vendor.gnsschip |grep -q -e marlin3 -e marlin3lite || getprop ro.board.platform |grep -q msm8996;then
    setprop persist.sys.bt.max_vendor_cap 0
fi

if getprop ro.boot.hardware.sku | grep -q -e fuxi -e nuwa -e ishtar; then
    setprop ro.surface_flinger.set_idle_timer_ms 1000

    setprop ro.surface_flinger.set_touch_timer_ms 800

    setprop ro.surface_flinger.set_display_power_timer_ms 4000

    setprop debug.sf.frame_rate_multiple_threshold 120

    setprop persist.phh.xiaomi.fod.enrollment.id 4
fi

if getprop ro.boot.hardware.sku | grep -q -e taoyao -e cupid -e daumier; then
    setprop ro.surface_flinger.set_idle_timer_ms 1000

    setprop ro.surface_flinger.set_touch_timer_ms 800

    setprop ro.surface_flinger.set_display_power_timer_ms 4000

    setprop debug.sf.frame_rate_multiple_threshold 120

    setprop persist.phh.xiaomi.fod.enrollment.id 10
fi

# Fixes Vibrator on TECNO POVA 4 Pro

if getprop ro.product.vendor.device | grep -q -e TECNO-LG8n; then
    chown -R system:system /sys/class/leds/vibrator_single/
fi


# Force disable Sunlight Reading Mode to prevent weird color issues
if getprop ro.vendor.build.fingerprint | grep -iq -e xiaomi/renoir; then
    resetprop_phh ro.vendor.sre.enable false
fi

# brightness fix for platform ums512 And ums9230
if getprop ro.board.platform |grep -iq -e ums512 -e ums9230;then
    setprop persist.sys.qcom-brightness "$(cat /sys/class/backlight/sprd_backlight/max_brightness)"
fi

# Fix for non-AMOLED Transsion devices where brightness would be dimmer than usual
if [ -n "$(getprop ro.vendor.transsion.backlight_12bit)" ];then
    setprop ro.vendor.transsion.backlight_hal.optimization $(getprop ro.vendor.transsion.backlight_12bit)
fi

# Fix default orientation on Rebecco K70
if getprop ro.vendor.build.fingerprint | grep -iq -e Rebecco/K70_ROW; then
    resetprop_phh ro.surface_flinger.primary_display_orientation ORIENTATION_0
fi

# Enable pen mode on Lenovo/goodix
if [ -e /sys/devices/platform/goodix_ts.0/support_pen ]; then
    echo 1 > /sys/devices/platform/goodix_ts.0/support_pen
fi
