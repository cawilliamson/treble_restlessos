#!/system/bin/sh
# discover unsupported vendor power HAL modes by monitoring logcat for
# "unknown type" / "unsupported" errors after boot.  runs briefly after
# boot and populates a persist property so the framework can skip those
# modes on subsequent boots.
#
# the framework's PowerManagerService reads this property and blocks
# modes listed in it from being forwarded to the vendor HAL.

PROP="persist.sys.phh.blocked_power_modes"
INTERVAL=10
MAX_LOOPS=15
last_type=""

existing="$(getprop "$PROP")"
log -t power-mode-monitor "starting (prop=${existing:-empty})"

i=0
while [ "$i" -lt "$MAX_LOOPS" ]; do
    # dump the current log buffer (all buffers) and scan for errors.
    # use brief format: P/TAG(PID): message
    logcat -b all -d -v brief 2>/dev/null | while IFS= read -r line; do
        case "$line" in
            *"[setMode] type:"*)
                # extract the type number.  format:
                # I/mtkpower@impl( 1338): [setMode] type:6, enabled:1
                last_type="${line##*type:}"
                last_type="${last_type%%,*}"
                ;;
            *"[setMode] unknown"*|\
            *"[setMode] unsupported"*|\
            *"setMode: unknown"*|\
            *"setMode: unsupported"*)
                if [ -n "$last_type" ]; then
                    # check whether already blocked
                    if ! echo ",$existing," | grep -q ",$last_type,"; then
                        if [ -z "$existing" ]; then
                            existing="$last_type"
                        else
                            existing="$existing,$last_type"
                        fi
                        setprop "$PROP" "$existing"
                        log -t power-mode-monitor \
                            "blocked unsupported power mode $last_type (prop=$existing)"
                    fi
                    last_type=""
                fi
                ;;
        esac
    done

    sleep "$INTERVAL"
    i=$((i + 1))
done

log -t power-mode-monitor "finished after $((MAX_LOOPS * INTERVAL))s (prop=$(getprop "$PROP"))"
