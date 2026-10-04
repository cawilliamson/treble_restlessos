#!/bin/bash

rom_script=''
if [ -n "$1" ];then
	if echo "$1" | grep -qF /;then
        rom_script=''
        for i in "$@";do
            rom_script="$rom_script"$'\n''$(call inherit-product, '$i')'
        done
    else
		rom_script='$(call inherit-product, device/phh/treble/'$1'.mk)'
	fi
fi

echo 'PRODUCT_MAKEFILES := \' > AndroidProducts.mk

for part in a ab;do
	for apps in vanilla gapps foss gapps-go old;do
		for arch in arm64 arm a64;do
			for su in yes no;do
				apps_suffix=""
				apps_script=""
				apps_name=""
				extra_packages=""
                vndk="vndk.mk"
		optional_base=""
				if [ "$apps" == "gapps" ];then
					apps_suffix="g"
					apps_script='$(call inherit-product, device/phh/treble/gapps.mk)'
					apps_name="with GApps"
				fi
				if [ "$apps" == "gapps-go" ];then
					apps_suffix="o"
					apps_script='$(call inherit-product, device/phh/treble/gapps-go.mk)'
					apps_name="Go"
				fi
				if [ "$apps" == "foss" ];then
					apps_suffix="f"
					apps_script='$(call inherit-product, vendor/foss/foss.mk)'
					apps_name="with FOSS apps"
				fi
				if [ "$apps" == "vanilla" ];then
					apps_suffix="v"
					apps_script=''
					apps_name="vanilla"
				fi
				if [ "$apps" == "old" ];then
					apps_suffix="y"
					apps_script=''
					apps_name="vanilla old"
					optional_base='include device/phh/treble/old-vndk.mk'
				fi
				if [ "$arch" == "arm" ];then
					vndk="vndk-binder32.mk"
				fi
				if [ "$arch" == "a64" ];then
					vndk="vndk32.mk"
				fi

				su_suffix='N'
				if [ "$su" == "yes" ];then
					su_suffix='S'
					extra_packages+=' phh-su me.phh.superuser'
				fi

				part_suffix='a'
				if [ "$part" == 'ab' ];then
					part_suffix='b'
				else
					optional_base='$(call inherit-product, device/phh/treble/base-sas.mk)'
				fi

				target="treble_${arch}_${part_suffix}${apps_suffix}${su_suffix}"

				baseArch="$arch"
				if [ "$arch" = "a64" ];then
					baseArch="arm"
				fi

				zygote=32
				if [ "$arch" = "arm64" ];then
					zygote=64_32
				fi

				cat > ${target}.mk << EOF
TARGET_GAPPS_ARCH := ${baseArch}
include build/make/target/product/aosp_${baseArch}.mk
\$(call inherit-product, device/phh/treble/base.mk)
$optional_base
$apps_script
$rom_script

PRODUCT_NAME := $target
PRODUCT_DEVICE := tdgsi_${arch}_$part
PRODUCT_BRAND := google
PRODUCT_SYSTEM_BRAND := google
PRODUCT_MODEL := TrebleDroid $apps_name

# Overwrite the inherited "emulator" characteristics
PRODUCT_CHARACTERISTICS := device

PRODUCT_PACKAGES += $extra_packages

EOF
echo -e '\t$(LOCAL_DIR)/'$target.mk '\' >> AndroidProducts.mk
			done
		done
	done
done
echo >> AndroidProducts.mk
