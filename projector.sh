#!/usr/bin/env bash
#
# projector.sh - Unihertz 8849 (Tank) projector control via adb
#
# Usage:
#   ./projector.sh on       Turn on projector with auto-focus calibration
#   ./projector.sh off      Turn off projector and restore display
#   ./projector.sh status   Show projector status, temperature, focus values
#   ./projector.sh focus    Re-read factory calibration and re-send to daemon
#   ./projector.sh manual-focus <step>   Relative focus adjustment (e.g. +10 or -10)
#
# Requirements:
#   - Device connected via adb and rooted (su available)
#   - agold-cmd binary on device (/system/bin/agold-cmd)
#   - Agold daemon running (vendor.mediatek.hardware.agolddaemon.IAgoldDaemon)
#
# ─── Architecture Overview ───────────────────────────────────────────────
#
# The Unihertz 8849 projector is controlled through several interfaces:
#
#   1. Agold Daemon (AIDL service)
#      - agold-cmd ioctl <a> <b> <c> <d>  →  IAgoldDaemon.SendMessageToIoctl()
#        Primary control: ioctl 500 controls projector power and mode.
#          500 0 <mode> 0   → Enable projector in mode (1=portrait, 3=landscape, 6=landscape-inv)
#          500 0 0 0         → Disable projector
#          500 0 0 3         → Enable phone display while projecting
#          500 0 0 4         → Disable phone display while projecting
#      - agold-cmd cmd "<csv>"            →  IAgoldDaemon.cmd()
#        Sends focus calibration table (17 comma-separated values) to the
#        motor_routine_thread so auto-focus uses correct per-device positions.
#
#   2. FPGA SPI sysfs (/sys/bus/spi/drivers/fpga_spi/)
#      - projector_sys_status   → 0=display off, 1=display on (read)
#      - projector_status       → projector hardware state (read)
#      - projector_err          → error state, "OK" when healthy (read)
#      - projector_temp         → temperature in degrees C (read, projector must be on)
#      - projector_mode         → current mode (read)
#      - projector_rgb_current  → brightness 1-255 (write)
#      - projector_pitch_setting → keystone correction angle (write, single int)
#        Internally calls dlpc3421_pitch_set which writes I2C regs 0xBB
#        and 0x88 on the DLPC3421. Valid range approx -67 to +67.
#      - projector_fan_en       → fan speed 0-100 (write)
#      - motor_adc              → current motor ADC position (read, -9999 when off)
#      - motor_turn_mode_step   → relative focus step adjustment (write)
#      - motor_turn_mode_step_force → forced focus step (write)
#      - focus_auto_calibration → trigger hardware motor sweep (write "1")
#      - dlpc3421_awb_cali      → DLPC auto white balance calibration (read)
#      - agold_spi_flash_version → FPGA firmware version (read)
#      - projector_name         → controller chip name, e.g. "dlpc3421" (read)
#      - fpga_h3_scan_hori      → horizontal scan offset for image alignment (write)
#      - fpga_h3_scan_vert      → vertical scan offset for image alignment (write)
#      - fpga_h3_scan_mode      → H3 scan mode (write)
#      - fpga_h3_scan_0x9c      → H3 register 0x9C (read/write)
#      - fpga_h3_scan_0x9d      → H3 register 0x9D (read/write)
#      - fpga_h3_screen_rotation → H3 screen rotation (write)
#
#   3. Factory Calibration (proinfo partition)
#      - Focus table at offset 0x1000cc: 17 comma-separated values
#        Format: "550,v1,v2,...,v16" where 550 is the prefix/identifier
#        and v1-v16 are per-device motor ADC positions for 16 focus zones.
#        These are written during factory calibration and are unique to
#        each device's optics/motor assembly.
#      - Other calibration at offset 0x10014c: 3 comma-separated values
#        Format: "awb_cali,scan_hori,scan_vert" (e.g. "15,173,176")
#        Corresponds to stock Settings.Global "projector_other_calibration_value"
#        - awb_cali: DLPC3421 auto white balance → dlpc3421_awb_cali
#        - scan_hori: H3 FPGA horizontal scan    → fpga_h3_scan_hori
#        - scan_vert: H3 FPGA vertical scan      → fpga_h3_scan_vert
#        Applied by stock ElephantProduct.setOtherCalibrationData().
#
#   4. Laser Rangefinder (/sys/nds03_ctrl/nds03_read)
#      - Returns distance in mm (read)
#
# ─── Focus Calibration Details ───────────────────────────────────────────
#
# The auto-focus system uses a lookup table of 16 motor positions (ADC values)
# corresponding to focus distances. Each device has unique values due to
# manufacturing tolerances in the optics and motor assembly.
#
# Factory calibration is stored in the proinfo partition at offset 0x1000cc.
# The stock Unihertz ROM reads this at boot; on GSI/custom ROMs we must
# read it ourselves and send it via agold-cmd cmd before enabling the projector.
#
# If the factory values aren't sent, the motor_routine_thread runs with
# zeros and the ADC feedback loop fails → no auto-focus.
#
# The motor ADC channel may report failures ("ADC_get_ch fail!") on some
# GSI configurations due to IIO channel mapping differences. In that case,
# the calibration table provides static positions the motor thread uses as
# fallback targets.
#
# ─── Display Resize ─────────────────────────────────────────────────────
#
# The phone display is 1440x3200 (20:9) but the projector is 16:9.
# We resize to 1440x2560 (16:9) so the full UI fits in the projected area.
# Black bars appear on the phone screen top/bottom but the projection
# fills the surface correctly.
#
# ─── Enable Sequence ────────────────────────────────────────────────────
#
# 1. Read factory focus calibration from proinfo partition
# 2. Send calibration table via agold-cmd cmd
# 3. Disable Always-on Display (AoD) temporarily — the FPGA needs the
#    display fully off to initialize cleanly
# 4. Sleep the display (input keyevent KEYCODE_SLEEP)
# 5. Wait for projector_sys_status == 0
# 6. Send ioctl 500 to enable projector in desired mode
# 7. Wake the display (input keyevent KEYCODE_WAKEUP)
# 8. Wait for projector_sys_status == 1
# 9. Shrink display to 16:9 aspect ratio
# 10. Restore AoD setting
# 11. Set fan to a reasonable speed
#
# ─── Disable Sequence ───────────────────────────────────────────────────
#
# 1. Send ioctl 500 0 0 0 to turn off projector
# 2. Restore display to original resolution
# 3. Fan will be turned off by the daemon automatically
#
# ════════════════════════════════════════════════════════════════════════

set -euo pipefail

# ─── Configuration ──────────────────────────────────────────────────────

SYSFS="/sys/bus/spi/drivers/fpga_spi"
PROINFO_FOCUS_OFFSET=1048780     # 0x1000cc in decimal
PROINFO_FOCUS_LENGTH=128         # max length of calibration string field
PROINFO_PITCH_OFFSET=1048908     # 0x10014c in decimal
PROINFO_PITCH_LENGTH=128         # max length of pitch calibration field

# Default projector mode: 1=portrait, 3=landscape, 6=landscape-inverted
PROJECTOR_MODE="${PROJECTOR_MODE:-3}"

# Default brightness (1-255)
PROJECTOR_BRIGHTNESS="${PROJECTOR_BRIGHTNESS:-128}"

# Default fan speed (0-100)
PROJECTOR_FAN="${PROJECTOR_FAN:-50}"

# Display dimensions
DISPLAY_WIDTH=1440
DISPLAY_HEIGHT_NATIVE=3200
DISPLAY_HEIGHT_16x9=2560   # 1440 * 16/9

# ─── Helpers ────────────────────────────────────────────────────────────

log()  { echo "[projector] $*" >&2; }
warn() { echo "[projector] WARNING: $*" >&2; }
die()  { echo "[projector] ERROR: $*" >&2; exit 1; }

# Run a command on the device as root
adb_su() {
    adb shell su -c "$*"
}

# Read a sysfs file on the device (as root)
read_sysfs() {
    local path="$1"
    adb_su "cat '$path'" 2>/dev/null | tr -d '\r'
}

# Write a value to a sysfs file on the device (as root)
# Note: the redirect must be inside the su -c string so that su's
# root shell handles it, not the outer unprivileged shell.
write_sysfs() {
    local path="$1"
    local value="$2"
    adb shell "su -c 'echo $value > $path'"
}

# Send an ioctl command via agold-cmd
# Returns 0 on success, 1 if agold-cmd reports failure (Returned -1)
send_ioctl() {
    local a="$1" b="$2" c="$3" d="$4"
    log "ioctl $a $b $c $d"
    local result
    result=$(adb_su "agold-cmd ioctl $a $b $c $d" | tr -d '\r')
    echo "$result"
    if echo "$result" | grep -q 'Returned -1'; then
        return 1
    fi
    return 0
}

# Send a cmd (calibration string) via agold-cmd
# Returns 0 on success, 1 if agold-cmd reports failure
send_cmd() {
    local data="$1"
    log "cmd '$data'"
    local result
    result=$(adb_su "agold-cmd cmd '$data'" | tr -d '\r')
    echo "$result"
    if echo "$result" | grep -q 'Returned -1'; then
        return 1
    fi
    return 0
}

# Wait for projector_sys_status to reach a target value.
# Sends the appropriate sleep/wake command first, then polls.
# For sleep: uses power button (keycode 26) since KEYCODE_SLEEP only
# enters doze which doesn't fully turn off the display panel.
# For wake: uses KEYCODE_WAKEUP which is idempotent (safe to repeat).
wait_for_sys_status() {
    local target="$1"
    local timeout="${2:-10}"
    local attempt=0
    local max_attempts=$((timeout * 5))  # 0.2s per poll

    log "Waiting for projector_sys_status == $target (timeout: ${timeout}s)..."
    while [ "$attempt" -lt "$max_attempts" ]; do
        local cur
        cur=$(read_sysfs "$SYSFS/projector_sys_status")
        if [ "$cur" = "$target" ]; then
            log "projector_sys_status == $target (after $attempt polls)"
            return 0
        fi
        # cmd power sleep/wakeup are the shell equivalents of
        # PowerManager.goToSleep()/wakeUp(). Both are idempotent
        # (safe to re-send), unlike the power button which toggles.
        # Re-send on each poll, matching stock checkLcmStatus() behavior.
        if [ "$target" = "0" ]; then
            adb_su "cmd power sleep" 2>/dev/null
        else
            adb_su "cmd power wakeup" 2>/dev/null
        fi
        sleep 0.2
        attempt=$((attempt + 1))
    done
    warn "Timed out waiting for projector_sys_status == $target (stuck at $(read_sysfs "$SYSFS/projector_sys_status"))"
    return 1
}

# ─── Factory Calibration ───────────────────────────────────────────────

# Read the factory focus calibration from the proinfo partition.
# Returns the 17-value CSV string (e.g. "550,554,541,...,348")
read_factory_focus_calibration() {
    log "Reading factory focus calibration from proinfo @ offset $PROINFO_FOCUS_OFFSET..."
    local raw
    raw=$(adb_su "dd if=/dev/block/by-name/proinfo bs=1 skip=$PROINFO_FOCUS_OFFSET count=$PROINFO_FOCUS_LENGTH 2>/dev/null" | tr -d '\0\r\n')
    if [ -z "$raw" ]; then
        warn "Could not read factory focus calibration from proinfo"
        return 1
    fi
    # Validate: should start with "550," and contain comma-separated numbers
    if [[ "$raw" =~ ^550, ]]; then
        log "Factory focus calibration: $raw"
        echo "$raw"
        return 0
    else
        warn "proinfo focus field does not look like calibration data: '$raw'"
        return 1
    fi
}

# Read the factory pitch/keystone calibration from proinfo.
# Returns a 3-value CSV string (e.g. "15,173,176")
read_factory_pitch_calibration() {
    log "Reading factory pitch calibration from proinfo @ offset $PROINFO_PITCH_OFFSET..."
    local raw
    raw=$(adb_su "dd if=/dev/block/by-name/proinfo bs=1 skip=$PROINFO_PITCH_OFFSET count=$PROINFO_PITCH_LENGTH 2>/dev/null" | tr -d '\0\r\n')
    if [ -z "$raw" ]; then
        warn "Could not read factory pitch calibration from proinfo"
        return 1
    fi
    if [[ "$raw" =~ ^[0-9]+,[0-9]+,[0-9]+ ]]; then
        log "Factory pitch calibration: $raw"
        echo "$raw"
        return 0
    else
        warn "proinfo pitch field does not look like calibration data: '$raw'"
        return 1
    fi
}

# ─── Projector ON ──────────────────────────────────────────────────────

do_on() {
    log "=== Enabling projector ==="

    # Matches stock ProjectorController.powerOnOffForPrejectorOn() sequence
    # for PROJECT_NEW_FIRMWARE=true, ElephantProduct device.

    # Step 1: Verify device connectivity and agold-cmd
    adb_su "which agold-cmd" >/dev/null 2>&1 || die "agold-cmd not found on device"
    log "Device connected, agold-cmd available"

    # Step 2: Shrink display to 16:9 BEFORE anything else
    # Stock: changeScreenSize(1) is the first thing in powerOnOffForPrejectorOn
    log "Resizing display to ${DISPLAY_WIDTH}x${DISPLAY_HEIGHT_16x9} (16:9)..."
    adb shell wm size "${DISPLAY_WIDTH}x${DISPLAY_HEIGHT_16x9}" || die "Failed to resize display"

    # Step 3: Temporarily disable AoD so display can fully sleep
    # GrapheneOS has AoD enabled by default; stock Unihertz ROM does not.
    # AoD prevents projector_sys_status from reaching 0.
    local aod_was_enabled
    aod_was_enabled=$(adb shell settings get secure doze_always_on 2>/dev/null | tr -d '\r')
    if [ "$aod_was_enabled" = "1" ]; then
        log "Temporarily disabling AoD for FPGA init"
        adb shell settings put secure doze_always_on 0 || die "Failed to disable AoD"
        sleep 0.5
    fi

    # Step 4: Send focus calibration BEFORE mode switch
    # The motor_routine_thread needs the calibration table loaded before the
    # projector is enabled. Sending only after mode switch leaves motor_adc
    # stuck at ~251 (uncalibrated position).
    local focus_cal
    focus_cal=$(read_factory_focus_calibration) || die "Failed to read factory focus calibration"
    send_cmd "$focus_cal" || die "Failed to send focus calibration"
    log "Focus calibration pre-loaded"

    # Step 5: Sleep the display and wait for sys_status == 0
    # Stock uses PowerManager.goToSleep() directly; we use KEYCODE_SLEEP
    # via wait_for_sys_status which resends on each poll (matching stock).
    log "Sleeping display..."
    wait_for_sys_status 0 40 || die "Display failed to sleep (sys_status never reached 0)"

    # Step 5: Disable phone display
    # Stock: SendMessageToIoctl(500, 0, 0, 4)
    log "Disabling phone display (ioctl 500 0 0 4)..."
    send_ioctl 500 0 0 4

    # Step 6: Initialize projector hardware
    # Stock: SendMessageToIoctl(500, 0, 0, 6)
    # Returns -1 on our firmware -- see PROJECTOR_NOTES.md
    # Not fatal; skip silently if unsupported.
    log "Initializing projector hardware (ioctl 500 0 0 6)..."
    send_ioctl 500 0 0 6 || log "(ioctl 500 0 0 6 not supported on this firmware, continuing)"

    # Step 7: Wake the display and wait for sys_status == 1
    # Stock: checkLcmStatus(1) which calls wakeUp repeatedly
    log "Waking display..."
    wait_for_sys_status 1 40 || die "Display failed to wake (sys_status never reached 1)"

    # Step 8: Set projector mode
    # Stock: setProjectorModeStatus(1, -1) → SendMessageToIoctl(500, 0, mode, 0)
    log "Setting projector mode $PROJECTOR_MODE..."
    send_ioctl 500 0 "$PROJECTOR_MODE" 0
    sleep 0.1

    # Step 9: Re-enable phone display
    # Stock: SendMessageToIoctl(500, 0, 0, 3) after 100ms delay
    log "Re-enabling phone display (ioctl 500 0 0 3)..."
    send_ioctl 500 0 0 3

    # Step 10: Restore AoD if it was enabled
    if [ "$aod_was_enabled" = "1" ]; then
        log "Restoring AoD"
        adb shell settings put secure doze_always_on 1
    fi

    # Step 11: Dismiss lockscreen if present
    adb shell input keyevent KEYCODE_WAKEUP 2>/dev/null || true
    sleep 0.5
    adb shell input keyevent KEYCODE_MENU 2>/dev/null || true

    # Step 12: Apply factory "other calibration" (AWB + H3 scan offsets)
    # Stock: setOtherCalibrationData(1) is called AFTER mode is set
    local other_cal
    other_cal=$(read_factory_pitch_calibration 2>/dev/null) || true
    if [ -n "$other_cal" ]; then
        local awb_cali scan_hori scan_vert
        awb_cali=$(echo "$other_cal" | cut -d, -f1)
        scan_hori=$(echo "$other_cal" | cut -d, -f2)
        scan_vert=$(echo "$other_cal" | cut -d, -f3)
        log "Applying factory other calibration: awb=$awb_cali hori=$scan_hori vert=$scan_vert"
        write_sysfs "$SYSFS/dlpc3421_awb_cali" "$awb_cali"
        write_sysfs "$SYSFS/fpga_h3_scan_hori" "$scan_hori"
        write_sysfs "$SYSFS/fpga_h3_scan_vert" "$scan_vert"
    fi

    # Step 13: Send focus calibration
    # Stock: focus calibration is sent after mode set
    local focus_cal
    focus_cal=$(read_factory_focus_calibration) || die "Failed to read factory focus calibration"
    send_cmd "$focus_cal" || die "Failed to send focus calibration"
    log "Focus calibration sent to daemon"

    # Step 14: Reset keystone pitch to 0 (straight image when flat)
    log "Setting keystone pitch to 0 (flat baseline)..."
    write_sysfs "$SYSFS/projector_pitch_setting" "0"

    # Step 15: Set fan speed
    # Stock: elephantDevicesOpen() sets fan to 20 initially
    log "Setting fan speed to $PROJECTOR_FAN..."
    write_sysfs "$SYSFS/projector_fan_en" "$PROJECTOR_FAN"

    # Step 16: Set brightness
    log "Setting brightness to $PROJECTOR_BRIGHTNESS..."
    write_sysfs "$SYSFS/projector_rgb_current" "$PROJECTOR_BRIGHTNESS"

    log "=== Projector ON ==="
    log ""
    log "Controls:"
    log "  Brightness:    echo <1-255> > $SYSFS/projector_rgb_current"
    log "  Fan speed:     echo <0-100> > $SYSFS/projector_fan_en"
    log "  Keystone:      echo <value> > $SYSFS/projector_pitch_setting"
    log "  Manual focus:  echo <step>  > $SYSFS/motor_turn_mode_step"
    log "  Turn off:      ./projector.sh off"
}

# ─── Projector OFF ─────────────────────────────────────────────────────

do_off() {
    log "=== Disabling projector ==="

    # Matches stock ProjectorController.powerOnOffForPrejectorOff() sequence
    # for PROJECT_NEW_FIRMWARE=true, ElephantProduct device.

    # Step 1: Disable phone display
    # Stock: SendMessageToIoctl(500, 0, 0, 4)
    log "Disabling phone display..."
    send_ioctl 500 0 0 4 || die "Failed to disable phone display"
    sleep 0.5

    # Step 2: Turn off projector LED
    # Stock: SendMessageToIoctl(500, 0, 0, 1)
    log "Turning off projector (ioctl 500 0 0 1)..."
    send_ioctl 500 0 0 1 || die "Failed to turn off projector LED"

    # Step 3: Set mode to off
    # Stock: setProjectorModeStatus(mode, 0) → SendMessageToIoctl(500, 0, 0, 0)
    log "Setting projector mode off..."
    send_ioctl 500 0 0 0 || die "Failed to set projector mode off"

    # Step 4: Re-enable phone display
    # Stock: SendMessageToIoctl(500, 0, 0, 3)
    log "Re-enabling phone display..."
    send_ioctl 500 0 0 3 || die "Failed to re-enable phone display"
    sleep 0.05

    # Step 5: Sleep/wake cycle to cleanly reset display path
    # Stock (PROJECT_NEW_FIRMWARE): checkLcmStatus(0) then checkLcmStatus(1)
    local aod_was_enabled
    aod_was_enabled=$(adb shell settings get secure doze_always_on 2>/dev/null | tr -d '\r')
    if [ "$aod_was_enabled" = "1" ]; then
        log "Temporarily disabling AoD for sleep/wake cycle"
        adb shell settings put secure doze_always_on 0 || die "Failed to disable AoD"
        sleep 0.5
    fi
    log "Sleep/wake cycle for display reset..."
    wait_for_sys_status 0 40 || die "Display failed to sleep during off sequence"
    wait_for_sys_status 1 40 || die "Display failed to wake during off sequence"
    if [ "$aod_was_enabled" = "1" ]; then
        adb shell settings put secure doze_always_on 1
    fi
    sleep 0.1

    # Step 6: Restore display to native resolution
    # Stock: changeScreenSize(0) → clearForcedDisplaySize + clearForcedDisplayDensityForUser
    log "Restoring display to native resolution..."
    adb shell wm size reset || die "Failed to restore display size"
    adb shell wm density reset 2>/dev/null || true

    # Step 7: Dismiss lockscreen
    adb shell input keyevent KEYCODE_WAKEUP 2>/dev/null || true
    sleep 0.5
    adb shell input keyevent KEYCODE_MENU 2>/dev/null || true

    log "=== Projector OFF ==="
}

# ─── Status ─────────────────────────────────────────────────────────────

do_status() {
    log "=== Projector Status ==="

    echo "  sys_status:     $(read_sysfs "$SYSFS/projector_sys_status")"
    echo "  projector_status: $(read_sysfs "$SYSFS/projector_status")"
    echo "  error:          $(read_sysfs "$SYSFS/projector_err")"
    echo "  mode:           $(read_sysfs "$SYSFS/projector_mode")"
    echo "  temperature:    $(read_sysfs "$SYSFS/projector_temp")"
    echo "  motor_adc:      $(read_sysfs "$SYSFS/motor_adc")"
    echo "  fan:            $(read_sysfs "$SYSFS/projector_fan_en")"
    echo "  controller:     $(read_sysfs "$SYSFS/projector_name")"
    echo "  fpga_version:   $(read_sysfs "$SYSFS/agold_spi_flash_version")"
    echo "  awb_cali:       $(read_sysfs "$SYSFS/dlpc3421_awb_cali")"

    echo ""
    echo "  H3 scan_hori:   $(read_sysfs "$SYSFS/fpga_h3_scan_hori")"
    echo "  H3 scan_vert:   $(read_sysfs "$SYSFS/fpga_h3_scan_vert")"
    echo "  H3 scan_mode:   $(read_sysfs "$SYSFS/fpga_h3_scan_mode")"
    echo "  H3 scan_0x9c:   $(read_sysfs "$SYSFS/fpga_h3_scan_0x9c")"
    echo "  H3 scan_0x9d:   $(read_sysfs "$SYSFS/fpga_h3_scan_0x9d")"
    echo "  H3 rotation:    $(read_sysfs "$SYSFS/fpga_h3_screen_rotation")"

    echo ""
    echo "  Display size:   $(adb shell wm size 2>/dev/null | tr -d '\r')"
    echo "  AoD:            $(adb shell settings get secure doze_always_on 2>/dev/null | tr -d '\r')"

    echo ""
    log "Factory calibration (proinfo):"
    local focus_cal pitch_cal
    focus_cal=$(read_factory_focus_calibration 2>/dev/null) && echo "  focus:    $focus_cal" || echo "  focus:    (not found)"
    pitch_cal=$(read_factory_pitch_calibration 2>/dev/null) && echo "  other:    $pitch_cal  (awb_cali,scan_hori,scan_vert)" || echo "  other:    (not found)"

    # Try laser rangefinder
    local laser
    laser=$(read_sysfs "/sys/nds03_ctrl/nds03_read" 2>/dev/null)
    if [ -n "$laser" ]; then
        echo ""
        echo "  laser distance: ${laser} mm"
    fi
}

# ─── Focus ──────────────────────────────────────────────────────────────

do_focus() {
    log "=== Re-sending factory focus calibration ==="
    local focus_cal
    focus_cal=$(read_factory_focus_calibration) || die "Could not read factory calibration"
    send_cmd "$focus_cal"
    log "Done. Calibration table sent to motor_routine_thread."
}

# ─── Manual Focus ──────────────────────────────────────────────────────

do_manual_focus() {
    local step="$1"
    log "Manual focus adjustment: step=$step"
    write_sysfs "$SYSFS/motor_turn_mode_step" "$step"
    log "Current motor_adc: $(read_sysfs "$SYSFS/motor_adc")"
}

# ─── Auto Keystone ─────────────────────────────────────────────────────
#
# Auto-keystone correction based on decompiled stock Unihertz code.
#
# Stock formula (from ProjectorController.calculateOrientation):
#   1. Accumulate 10 accelerometer samples, use Y axis as raise angle
#      (X axis in tablet/side projection mode)
#   2. If angle >= 75: use Z; if <= -75: use -Z; else use raise angle
#   3. Multiply by 1.68 to get raw correction
#   4. Clamp to -100..+100
#   5. For ElephantProduct (Tank/8849): multiply by -0.4
#   6. Write result to projector_pitch_setting if change > 3 units
#
# Effective formula: pitch = -(Y * 1.68 * 0.4) = -(Y * 0.672)
# Valid DLPC3421 pitch range: -40 to +25

do_auto_keystone() {
    log "=== Auto Keystone Correction ==="
    log "Reading accelerometer and adjusting pitch. Ctrl+C to stop."
    log "Valid pitch range: -40 to +25"
    log "Formula: pitch = -(raise_angle * 1.68 * 0.4)  [from stock ROM]"
    log ""

    local last_pitch=""
    local sample_count=0
    local sum_x=0 sum_y=0 sum_z=0

    while true; do
        # Grab the most recent accelerometer reading from sensorservice
        local accel_line
        accel_line=$(adb shell "su -c 'dumpsys sensorservice'" 2>/dev/null \
            | grep 'sh3201_acc: last' -A 1 \
            | tail -1)

        if [ -z "$accel_line" ]; then
            warn "Could not read accelerometer"
            sleep 1
            continue
        fi

        # Parse X, Y, Z values from the line
        # Format: "  N (ts=..., wall=...) X, Y, Z, "
        local x y z
        x=$(echo "$accel_line" | sed 's/.*) //' | cut -d, -f1 | tr -d ' ')
        y=$(echo "$accel_line" | sed 's/.*) //' | cut -d, -f2 | tr -d ' ')
        z=$(echo "$accel_line" | sed 's/.*) //' | cut -d, -f3 | tr -d ' ')

        if [ -z "$x" ] || [ -z "$z" ]; then
            sleep 0.3
            continue
        fi

        # Stock algorithm: accumulate samples, then compute on the 10th
        local pitch
        pitch=$(awk -v x="$x" -v y="$y" -v z="$z" '
            BEGIN {
                # In landscape mode, Y is the raise angle (tilt axis)
                raise = y

                # Stock thresholds for extreme angles
                if (raise >= 75.0) {
                    gval = z
                } else if (raise <= -75.0) {
                    gval = -z
                } else {
                    if (z > 80.0) raise = -raise
                    gval = raise
                }

                # Stock scaling: raw * 1.68, clamped to -100..100
                gval = gval * 1.68
                if (gval > 100) gval = 100
                if (gval < -100) gval = -100

                # ElephantProduct scaling: * -0.4
                pitch = int(-gval * 0.4)

                # Clamp to DLPC3421 valid range
                if (pitch > 25) pitch = 25
                if (pitch < -40) pitch = -40

                printf "%d", pitch
            }
        ')

        # Only write if value changed by more than 1 (stock uses threshold of 3
        # on the pre-scaled value, which is ~1 after the 0.4 scaling)
        if [ "$pitch" != "$last_pitch" ]; then
            printf "\r  accel=(%6s,%6s,%6s)  raise_y=%6s  pitch=%-4s" "$x" "$y" "$z" "$y" "$pitch" >&2
            write_sysfs "$SYSFS/projector_pitch_setting" "$pitch" 2>/dev/null
            last_pitch="$pitch"
        fi

        sleep 0.3
    done
}

# ─── Main ──────────────────────────────────────────────────────────────

case "${1:-}" in
    on)
        do_on
        ;;
    off)
        do_off
        ;;
    status)
        do_status
        ;;
    focus)
        do_focus
        ;;
    manual-focus)
        [ -n "${2:-}" ] || die "Usage: $0 manual-focus <step>  (e.g. +10 or -10)"
        do_manual_focus "$2"
        ;;
    auto-keystone)
        do_auto_keystone
        ;;
    *)
        echo "Usage: $0 {on|off|status|focus|manual-focus <step>|auto-keystone}"
        echo ""
        echo "Commands:"
        echo "  on              Enable projector with auto-focus from factory calibration"
        echo "  off             Disable projector and restore display"
        echo "  status          Show projector status, temperature, calibration"
        echo "  focus           Re-read and re-send factory focus calibration"
        echo "  manual-focus N  Adjust focus motor by N steps (e.g. +10, -10)"
        echo "  auto-keystone   Continuous tilt-based keystone correction (Ctrl+C to stop)"
        echo ""
        echo "Environment variables:"
        echo "  PROJECTOR_MODE=3         Mode: 1=portrait, 3=landscape, 6=landscape-inv"
        echo "  PROJECTOR_BRIGHTNESS=128 Brightness: 1-255"
        echo "  PROJECTOR_FAN=50         Fan speed: 0-100"
        echo "  KEYSTONE_SCALE=1.0       Tilt-to-pitch scaling factor"
        exit 1
        ;;
esac
