#!/system/bin/sh

set -ex

if ! [ "$(getprop ro.boot.dynamic_partitions)" = true ];then
    echo "OTA is supported only for devices with dynamic partitions!"
    exit 1
fi

flavor=$(getprop ro.product.product.name)
if [ -f /system/phh/secure ];then
	flavor=${flavor}-secure
fi
nextVersion=$(curl --silent -L https://raw.githubusercontent.com/phhusson/treble_experimentations/master/ota/squeak/$flavor/date)
if [ -z "$nextVersion" ];then
    echo "Couldn't find any OTA for $flavor"
    exit 1
fi

url=$(curl --silent -L https://raw.githubusercontent.com/phhusson/treble_experimentations/master/ota/squeak/$flavor/url)
size=$(curl --silent -L https://raw.githubusercontent.com/phhusson/treble_experimentations/master/ota/squeak/$flavor/size)

if [ "$(getprop ro.product.build.date.utc)" = "$nextVersion" ];then
    echo "Installing $nextVersion onto itself, aborting"
    exit 1
fi

if ! curl --silent -L https://raw.githubusercontent.com/phhusson/treble_experimentations/master/ota/squeak/$flavor/known_releases |grep -q $(getprop ro.product.build.date.utc);then
    echo "Warning! The build you are currently running is unknown. Type YES to confirm you want to apply OTA from $url"
    read answer
    if ! [ "$answer" = YES ];then
        exit 1
    fi
fi

if [ -b /dev/tmp-phh ] && ! tune2fs -l /dev/tmp-phh  |grep 'Last mount time' |grep -q n/a;then
    echo "Warning! It looks like you modified your system image! Flashing this OTA will revert this!"
    echo "Type YES to acknowledge"
    read answer
    if ! [ "$answer" = YES ];then
        exit 1
    fi
fi

echo "Flashing from ${url}..."

dmDevice=$(phh-ota new-slot)
curl -L "$url" | busybox_phh xz -d -c > $dmDevice
phh-ota switch-slot

reboot
exit 0
