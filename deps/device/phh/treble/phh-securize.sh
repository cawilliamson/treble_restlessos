#!/system/bin/sh

if [ -e /system/bin/magisk ]
then
    # remove bind-mount of phh-su overriding /system/bin/su -> ./magisk
    umount -l /system/bin/magisk
    # we need to modify the real system partition
    MAGISK_MIRROR="$(magisk --path)/.magisk/mirror"
    SYSTEM=$MAGISK_MIRROR/system
    MOUNTPOINT_LIST="$MAGISK_MIRROR/system_root $MAGISK_MIRROR/system"
else
    SYSTEM=/system
    MOUNTPOINT_LIST="/system /"
fi

# remove bind-mount of phh-su (preventing $SYSTEM/xbin/su to be removed)
umount -l /system/xbin/su

for MOUNTPOINT in $MOUNTPOINT_LIST
do
    [ -d $MOUNTPOINT ] && mountpoint -q $MOUNTPOINT && break
done

mount -o remount,rw $MOUNTPOINT
remount

touch $SYSTEM/phh/secure
rm $SYSTEM/xbin/su
rm $SYSTEM/bin/phh-su
rm $SYSTEM/etc/init/su.rc
rm $SYSTEM/bin/phh-securize.sh
rm -Rf $SYSTEM/{app,priv-app}/me.phh.superuser/
rm -Rf /data/su || true
mount -o remount,ro $MOUNTPOINT
sync
mkdir /metadata/phh
touch /metadata/phh/secure
reboot
