include build/make/target/board/generic_arm_ab/BoardConfig.mk
include device/phh/treble/board-base.mk

ifeq ($(BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE),)
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1572864000
endif
TARGET_USES_64_BIT_BINDER := true
BOARD_SYSTEMIMAGE_AS_SYSTEM := true
