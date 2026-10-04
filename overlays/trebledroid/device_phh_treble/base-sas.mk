PRODUCT_COPY_FILES += \
	device/phh/treble/apex-setup.rc:system/etc/init/apex-setup.rc \

PRODUCT_COPY_FILES += \
	system/core/rootdir/init.zygote32.rc:system/etc/init/zygote/init.zygote32.rc \
	system/core/rootdir/init.zygote32_64.rc:system/etc/init/zygote/init.zygote32_64.rc \
	system/core/rootdir/init.zygote64_32.rc:system/etc/init/zygote/init.zygote64_32.rc \
	system/core/rootdir/init.zygote64.rc:system/etc/init/zygote/init.zygote64.rc \

PRODUCT_COPY_FILES += \
	frameworks/av/apex/mediaswcodec.rc:system/etc/init/mediaswcodec-treble.rc \
	device/phh/treble/environ/init.treble-environ.rc:system/etc/init/init.treble-environ.rc \

