#!/system/bin/sh

mount -o remount,rw /
mount -o remount,rw /system
remount system

rm -Rf /system/priv-app/TeleService
mount -o remount,ro /
mount -o remount,ro /system
sync
reboot
