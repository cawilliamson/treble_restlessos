#!/system/bin/sh

if [ -z "$cache_log" ];then
	if [ -f /cache/phh/flash ];then
		rm -f /cache/phh/flash
	else
		exit 0
	fi
	cache_log=1 exec /system/bin/sh -x "$0" > /cache/phh/logs 2>&1
fi

#init.rc hooks are based on "0" (non-configfs) or "1" (configfs), so set it to 2 so that noone is triggered
if [ -z "$nosystem" ];then
	configfs="$(getprop sys.usb.configfs)"
	export configfs
	setprop sys.usb.configfs 2
	setprop service.adb.tcp.port 5555

	mount -o private,recursive rootfs /

	mkdir /dev/new-system/
	chmod 0755 /dev/new-system
	mkdir /dev/old-system

	cp -R --preserve=all /system/lib64 /dev/new-system/lib64
	cp -R --preserve=all /system/lib /dev/new-system/lib
	cp -R --preserve=all /system/bin /dev/new-system/bin
	cp -R --preserve=all /system/xbin /dev/new-system/xbin
	cp -R --preserve=all /system/etc /dev/new-system/etc

	getprop | \
	    grep -e restarting -e running | \
	    sed -nE -e 's/\[([^]]*).*/\1/g'  -e 's/init.svc.(.*)/\1/p' |
	    while read svc ;do
		setprop ctl.stop $svc
	    done

	setenforce 0
	umount /sbin/adbd
	mount -o move /system /dev/old-system
	/dev/new-system/bin/busybox_phh mount -o bind /dev/new-system /system
	nosystem=1 exec /system/bin/sh -x "$0"
fi

umount /dev/old-system
setprop service.adb.root 1

if [ "$configfs" == 1 ];then
	mount -t configfs none /config
	rm -Rf /config/usb_gadget
	mkdir -p /config/usb_gadget/g1

	echo 0x12d1 > /config/usb_gadget/g1/idVendor
	echo 0x103A > /config/usb_gadget/g1/idProduct
	mkdir -p /config/usb_gadget/g1/strings/0x409
	echo phh > /config/usb_gadget/g1/strings/0x409/serialnumber
	echo phh > /config/usb_gadget/g1/strings/0x409/manufacturer
	echo phh > /config/usb_gadget/g1/strings/0x409/product

	mkdir /config/usb_gadget/g1/functions/ffs.adb
	mkdir /config/usb_gadget/g1/functions/mtp.gs0
	mkdir /config/usb_gadget/g1/functions/ptp.gs1

	mkdir /config/usb_gadget/g1/configs/c.1/
	mkdir /config/usb_gadget/g1/configs/c.1/strings/0x409
	echo 'ADB MTP' > /config/usb_gadget/g1/configs/c.1/strings/0x409/configuration

	mkdir /dev/usb-ffs
	chmod 0770 /dev/usb-ffs
	chown shell:shell /dev/usb-ffs
	mkdir /dev/usb-ffs/adb/
	chmod 0770 /dev/usb-ffs/adb
	chown shell:shell /dev/usb-ffs/adb

	mount -t functionfs -o uid=2000,gid=2000 adb /dev/usb-ffs/adb

	/dev/new-system/bin/adbd &

	sleep 1
	echo none > /config/usb_gadget/g1/UDC
	ln -s /config/usb_gadget/g1/functions/ffs.adb /config/usb_gadget/g1/configs/c.1/f1
	ls /sys/class/udc |head -n 1 > /config/usb_gadget/g1/UDC

	sleep 2
	echo 2 > /sys/devices/virtual/android_usb/android0/port_mode
else
	mkdir /dev/usb-ffs
	chmod 0770 /dev/usb-ffs
	chown shell:shell /dev/usb-ffs
	mkdir /dev/usb-ffs/adb/
	chmod 0770 /dev/usb-ffs/adb
	chown shell:shell /dev/usb-ffs/adb

	mount -t functionfs -o uid=2000,gid=2000 adb /dev/usb-ffs/adb
	echo adb > /sys/class/android_usb/android0/f_ffs/aliases
	setprop sys.usb.config adb

	echo 0 > /sys/class/android_usb/android0/enable
	echo 18d1 > /sys/class/android_usb/android0/idVendor
	echo 4EE7 > /sys/class/android_usb/android0/idProduct 4EE7
	echo adb > /sys/class/android_usb/android0/functions
	getprop ro.boot.serialno |tr -d '\n' |cat > /sys/class/android_usb/android0/iSerial
	echo phh > /sys/class/android_usb/android0/iManufacturer
	echo phh > /sys/class/android_usb/android0/iProduct
	echo 1 > /sys/class/android_usb/android0/enable

	/dev/new-system/bin/adbd &
fi


death() {
	sleep 180
	reboot
}

dev="$(sed -n 1p /cache/phh/block.map)"
                    devbase="$(echo $dev | sed -nE 's;(/dev/block/.*/)userdata;\1;p')"
[ -z "$devbase" ] && devbase="$(echo $dev | sed -nE 's;(/dev/block/.*/)data;\1;p')"
[ -z "$devbase" ] && devbase="$(echo $dev | sed -nE 's;(/dev/block/.*/)USERDATA;\1;p')"
for i in system system_a SYSTEM;do
	v="$devbase/$i"
	[ -b "$v" ] && system="$v"
done
#Failed...
[ -z "$system" ] && death

blockdev --setrw "$system"


for method in xz-sparse sparse raw;do
	(
		size="$(sed -En '2s/^([0-9]+) .*/\1/p' /cache/phh/block.map)"
		block_size="$(sed -En '2s/.* ([0-9]*)$/\1/p' /cache/phh/block.map)"
		n_ranges="$(sed -n 3p /cache/phh/block.map)"
		block_id=0
		for i in $(seq 1 $n_ranges);do
			range_start="$(sed -En $((i+3))'s/^([0-9]+) .*/\1/p' /cache/phh/block.map)"
			range_end="$(sed -En $((i+3))'s/^.* ([0-9]+)$/\1/p' /cache/phh/block.map)"
			n_blocks=$((range_end-range_start))
			busybox_phh dd bs=$block_size skip=$range_start count=$n_blocks if=$dev

			block_id=$((block_id+n_blocks))
		done
	) | (
		set -e
		if [ "$method" == xz-sparse ];then
			busybox_phh xz -d -c | simg2img_simple > $system
		elif [ "$method" == sparse ];then
			simg2img_simple > $system
		elif [ "$method" == raw ];then
			cat > $system
		fi
	) && break
done

sync
reboot
