#!/system/bin/sh
# discover unsupported vendor power HAL modes by watching for "unknown type"
# errors in logcat.  runs briefly after boot and populates a persist property
# so the framework can skip those modes on subsequent boots.
# currently only supports MTK devices (mtkpower@impl log format).

# only run on mediatek devices
hw="$(getprop ro.hardware)"
if ! echo "$hw" | grep -qi mt; then
    exit 0
fi
PROP="persist.sys.phh.blocked_power_modes"
TIMEOUT=90
last_type=""

existing="$(getprop "$PROP")"

# monitor the main log buffer for vendor power HAL errors.
# the pattern covers MTK's mtkpower@impl and similar vendor tags.
timeout "$TIMEOUT" logcat -b main -v raw 2>/dev/null | while IFS= read -r line; do
    case "$line" in
        *"[setMode] type:"*)
            last_type="$(echo "$line" | sed -n 's/.*type:\([0-9]*\).*/\1/p')"
            ;;
        *"[setMode] unknown"*|*"[setMode] unsupported"*|*"setMode: unknown"*)
            if [ -n "$last_type" ]; then
                if ! echo ",$existing," | grep -q ",$last_type,"; then
                    if [ -z "$existing" ]; then
                        existing="$last_type"
                    else
                        existing="$existing,$last_type"
                    fi
                    setprop "$PROP" "$existing"
                    log -t power-mode-monitor "blocked unsupported power mode $last_type (prop=$existing)"
                fi
                last_type=""
            fi
            ;;
    esac
done
