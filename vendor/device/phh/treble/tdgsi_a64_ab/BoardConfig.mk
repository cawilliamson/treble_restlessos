include build/make/target/board/generic/BoardConfig.mk
include device/phh/treble/board-base.mk

# Copy pasted from build/make/target/board/generic_arm64/BoardConfig.mk
BOARD_SEPOLICY_DIRS += build/make/target/board/generic_arm64/sepolicy

ifeq ($(BOARD_SYSTEMIMAGE_PARTITION_RESERVED_SIZE),)
BOARD_SYSTEMIMAGE_PARTITION_SIZE := 1572864000
endif
TARGET_USES_64_BIT_BINDER := true
