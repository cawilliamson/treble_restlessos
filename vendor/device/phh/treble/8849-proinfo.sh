#!/system/bin/sh
# Read Unihertz 8849 factory calibration from proinfo partition and
# expose it via system properties for TrebleApp to consume.
#
# Focus calibration: 17 CSV values at offset 0x1000cc (1048780)
# Other calibration: 3 CSV values at offset 0x10014c (1048908)

# Only run on Unihertz 8849 (Tank) devices
[ -d /sys/bus/spi/drivers/fpga_spi ] || exit 0

PROINFO=/dev/block/by-name/proinfo
[ -e "$PROINFO" ] || exit 0

# Read focus calibration
focus=$(dd if="$PROINFO" bs=1 skip=1048780 count=128 2>/dev/null | tr -d '\0')
if echo "$focus" | grep -q '^550,'; then
    setprop sys.phh.8849.focus_cal "$focus"
fi

# Read other calibration (AWB, horizontal, vertical)
other=$(dd if="$PROINFO" bs=1 skip=1048908 count=128 2>/dev/null | tr -d '\0')
if echo "$other" | grep -qE '^[0-9]+,[0-9]+,[0-9]+$'; then
    setprop sys.phh.8849.other_cal "$other"
fi
