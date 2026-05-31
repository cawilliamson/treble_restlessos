#!/system/bin/sh
# If no real metadata partition was mounted by first-stage init,
# overlay the empty rootfs stub with tmpfs so aconfigd and other
# services that write to /metadata can function.

mountpoint -q /metadata || \
    mount -t tmpfs tmpfs /metadata -o mode=0775,uid=0,gid=1000
