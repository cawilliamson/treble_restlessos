#!/system/bin/sh
set -o pipefail

display_usage() {
    echo -e "\nUsage:\n ./phh-prop-handler.sh [prop]\n"
}

if [ "$#" -ne 1 ]; then
    display_usage
    exit 1
fi

prop_value=$(getprop "$1")

xiaomi_toggle_dt2w_proc_node() {
    DT2W_PROC_NODES=("/proc/touchpanel/wakeup_gesture"
        "/proc/tp_wakeup_gesture"
        "/proc/tp_gesture")
    for node in "${DT2W_PROC_NODES[@]}"; do
        [ ! -f "${node}" ] && continue
        echo "Trying to set dt2w mode with /proc node: ${node}"
        echo "$1" >"${node}"
        [[ "$(cat "${node}")" -eq "$1" ]] # Check result
        return
    done
    return 1
}

xiaomi_toggle_dt2w_event_node() {
    for ev in $(
        cd /sys/class/input || return
        echo event*
    ); do
        isTouchscreen=false
        if getevent -p /dev/input/$ev |grep -e 0035 -e 0036|wc -l |grep -q 2;then
            isTouchscreen=true
        fi
        [ ! -f "/sys/class/input/${ev}/device/device/gesture_mask" ] &&
            [ ! -f "/sys/class/input/${ev}/device/wake_gesture" ] &&
            ! $isTouchscreen && continue
        echo "Trying to set dt2w mode with event node: /dev/input/${ev}"
        if [ "$1" -eq 1 ]; then
            # Enable
            sendevent /dev/input/"${ev}" 0 1 5
            return
        else
            # Disable
            sendevent /dev/input/"${ev}" 0 1 4
            return
        fi
    done
    return 1
}

xiaomi_toggle_dt2w_ioctl() {
    if [ -c "/dev/xiaomi-touch" ]; then
        echo "Trying to set dt2w mode with ioctl on /dev/xiaomi-touch"
        # 14 - Touch_Doubletap_Mode
        xiaomi-touch 14 "$1"
        return
    fi
    return 1
}

restartAudio() {
    setprop ctl.restart audioserver
    audioHal="$(getprop |sed -nE 's/.*init\.svc\.(.*audio-hal[^]]*).*/\1/p')"
    setprop ctl.restart "$audioHal"
    setprop ctl.restart vendor.audio-hal-2-0
    setprop ctl.restart audio-hal-2-0
}

if [ "$1" == "persist.sys.phh.asus.dt2w" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi
    if [[ "$prop_value" == 1 ]];then
        setprop persist.asus.dclick 1
    else
        setprop persist.asus.dclick 0
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.asus.usb.port" ]; then
        setprop persist.vendor.usb.controller.default "$prop_value"
    exit
fi

if [ "$1" == "persist.sys.phh.xiaomi.dt2w" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    xiaomi_toggle_dt2w_proc_node "$prop_value"
    # Fallback to event node method
    xiaomi_toggle_dt2w_event_node "$prop_value"
    # Fallback to ioctl method
    xiaomi_toggle_dt2w_ioctl "$prop_value"
    exit
fi

if [ "$1" == "persist.sys.phh.oppo.dt2w" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    echo "$prop_value" >/proc/touchpanel/double_tap_enable
    exit
fi

if [ "$1" == "persist.sys.phh.oppo.gaming_mode" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    echo "$prop_value" >/proc/touchpanel/game_switch_enable
    exit
fi

if [ "$1" == "persist.sys.phh.securize" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi
    if [[ "$prop_value" == 1 ]]; then
        rm -f /metadata/securize_disable
    else
        touch /metadata/securize_disable
    fi
fi

if [ "$1" == "persist.sys.phh.oppo.usbotg" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi
    if [ -e /sys/class/power_supply/usb/otg_switch ]; then
        echo "$prop_value" >/sys/class/power_supply/usb/otg_switch
    else
        echo "$prop_value" >/sys/class/oplus_chg/usb/otg_switch
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.transsion.usbotg" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi
    OTG_PATH=$(find /sys/ -path *tran_battery/OTG_CTL)
    if [ -n "$OTG_PATH" ]; then
        echo "$prop_value" >$OTG_PATH
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.transsion.dt2w" ]; then
    if [[ "$prop_value" != "1" && "$prop_value" != "2" ]]; then
        exit 1
    fi
    echo cc${prop_value} > /proc/gesture_function
    exit
fi

if [ "$1" == "persist.sys.phh.allow_binder_thread_on_incoming_calls" ]; then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == 1 ]];then
        resetprop_phh ro.telephony.block_binder_thread_on_incoming_calls false
    else
        resetprop_phh --delete ro.telephony.block_binder_thread_on_incoming_calls
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.disable_audio_effects" ];then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == 1 ]];then
        resetprop_phh ro.audio.ignore_effects true
    else
        resetprop_phh --delete ro.audio.ignore_effects
    fi
    restartAudio
    exit
fi

if [ "$1" == "persist.sys.phh.caf.audio_policy" ];then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    sku="$(getprop ro.boot.product.vendor.sku)"
    if [[ "$prop_value" == 1 ]];then
        umount /vendor/etc/audio
        umount /vendor/etc/audio

        if [ -f /vendor/etc/audio_policy_configuration_sec.xml ];then
            mount /vendor/etc/audio_policy_configuration_sec.xml /vendor/etc/audio_policy_configuration.xml
        elif [ -f /vendor/etc/audio/sku_${sku}_qssi/audio_policy_configuration.xml ] && [ -f /vendor/etc/audio/sku_$sku/audio_policy_configuration.xml ];then
            umount /vendor/etc/audio
            mount /vendor/etc/audio/sku_${sku}_qssi/audio_policy_configuration.xml /vendor/etc/audio/sku_$sku/audio_policy_configuration.xml
        elif [ -f /vendor/etc/audio/audio_policy_configuration.xml ];then
            mount /vendor/etc/audio/audio_policy_configuration.xml /vendor/etc/audio_policy_configuration.xml
        elif [ -f /vendor/etc/audio_policy_configuration_base.xml ];then
            mount /vendor/etc/audio_policy_configuration_base.xml /vendor/etc/audio_policy_configuration.xml
        fi
    else
        umount /vendor/etc/audio_policy_configuration.xml
        umount /vendor/etc/audio/sku_$sku/audio_policy_configuration.xml
        if [ $(find /vendor/etc/audio -type f |wc -l) -le 3 ];then
            mount /mnt/phh/empty_dir /vendor/etc/audio
        fi
    fi
    restartAudio
    exit
fi

if [ "$1" == "persist.sys.phh.vsmart.dt2w" ];then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == 1 ]];then
        echo 0 > /sys/class/vsm/tp/gesture_control
    else
        echo > /sys/class/vsm/tp/gesture_control
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.backlight.scale" ];then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == 1 ]];then
        if [ -f /sys/class/leds/lcd-backlight/max_brightness ];then
            setprop persist.sys.qcom-brightness "$(cat /sys/class/leds/lcd-backlight/max_brightness)"
        elif [ -f /sys/class/backlight/panel0-backlight/max_brightness ];then
            setprop persist.sys.qcom-brightness "$(cat /sys/class/backlight/panel0-backlight/max_brightness)"
        elif [ -f /sys/class/backlight/sprd_backlight/max_brightness ];then
            setprop persist.sys.qcom-brightness "$(cat /sys/class/backlight/sprd_backlight/max_brightness)"
        fi
    else
        setprop persist.sys.qcom-brightness -1
    fi
    exit
fi

if [ "$1" == "persist.sys.phh.disable_soundvolume_effect" ];then
    if [[ "$prop_value" != "0" && "$prop_value" != "1" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == 1 ]];then
        mount /mnt/phh/empty /vendor/lib/soundfx/libvolumelistener.so
        mount /mnt/phh/empty /vendor/lib64/soundfx/libvolumelistener.so
    else
        umount /vendor/lib/soundfx/libvolumelistener.so
        umount /vendor/lib64/soundfx/libvolumelistener.so
    fi
    restartAudio
    exit
fi

if [ "$1" == "persist.bluetooth.system_audio_hal.enabled" ]; then
    # Migrate from 0/1 to false/true first
    if [[ "$prop_value" == "0" ]]; then
        setprop persist.bluetooth.system_audio_hal.enabled false
        exit 1
    elif [[ "$prop_value" == "1" ]]; then
        setprop persist.bluetooth.system_audio_hal.enabled true
        exit 1
    fi

    if [[ "$prop_value" != "false" && "$prop_value" != "true" ]]; then
        exit 1
    fi

    if [[ "$prop_value" == "true" ]]; then
        setprop persist.bluetooth.bluetooth_audio_hal.disabled false
        setprop persist.bluetooth.a2dp_offload.disabled true
        resetprop_phh ro.bluetooth.a2dp_offload.supported false
    else
        resetprop_phh --delete persist.bluetooth.bluetooth_audio_hal.disabled
        resetprop_phh --delete persist.bluetooth.a2dp_offload.disabled
        resetprop_phh --delete ro.bluetooth.a2dp_offload.supported
    fi
    restartAudio
    exit
fi
