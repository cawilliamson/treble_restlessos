#!/usr/bin/env bash
set -euo pipefail

REGION="${AWS_REGION:?}"
MOUNT_POINT="/mnt/src"
LABEL="builddata"

# retry a command: retry <attempts> <delay_seconds> <cmd...>
retry() {
  local n=$1 d=$2; shift 2
  for i in $(seq 1 "$n"); do
    "$@" && return 0
    [ "$i" -eq "$n" ] && return 1
    echo "  retry ${i}/${n} in ${d}s..." >&2; sleep "$d"
  done
}

# --------------------------------------------------------------------------
# attach: attach EBS volume to instance, wait in-use
# env: VOLUME_ID EC2_INSTANCE_ID
# --------------------------------------------------------------------------
attach() {
  local vid="${VOLUME_ID:?}" iid="${EC2_INSTANCE_ID:?}"
  echo "Attaching ${vid} to ${iid}..."
  retry 3 10 aws ec2 attach-volume --region "$REGION" \
    --volume-id "$vid" --instance-id "$iid" --device /dev/sdf
  aws ec2 wait volume-in-use --region "$REGION" --volume-ids "$vid"
  echo "Volume attached"
}

# --------------------------------------------------------------------------
# detach: detach EBS volume, wait available
# env: VOLUME_ID
# --------------------------------------------------------------------------
detach() {
  local vid="${VOLUME_ID:?}"
  echo "Detaching ${vid}..."
  retry 3 10 aws ec2 detach-volume --region "$REGION" \
    --volume-id "$vid" --force
  aws ec2 wait volume-available --region "$REGION" --volume-ids "$vid"
  echo "Volume available"
}

# --------------------------------------------------------------------------
# delete: delete EBS volume (with retry for stuck state)
# env: VOLUME_ID
# --------------------------------------------------------------------------
delete() {
  local vid="${VOLUME_ID:-}"
  [ -z "$vid" ] && { echo "No volume to delete"; exit 0; }
  echo "Deleting ${vid}..."
  aws ec2 delete-volume --region "$REGION" --volume-id "$vid" || true
  for i in $(seq 1 60); do
    local state
    state=$(aws ec2 describe-volumes --region "$REGION" --volume-ids "$vid" \
      --query 'Volumes[0].State' --output text 2>/dev/null) || { echo "Volume deleted"; exit 0; }
    [ "$state" = "deleted" ] && { echo "Volume deleted"; exit 0; }
    echo "  volume still ${state} (${i}/60)"; sleep 10
  done
  echo "ERROR: volume still not deleted"; exit 1
}

# --------------------------------------------------------------------------
# mount: find data volume (by label or NVMe serial), format if needed, mount
# env: [VOLUME_ID] — set for first mount (serial lookup), omit for subsequent
# --------------------------------------------------------------------------
mount_vol() {
  local dev=""

  # try by label first (subsequent mounts)
  dev=$(findfs "LABEL=${LABEL}" 2>/dev/null) || true

  # fall back to NVMe serial (first mount)
  if [ -z "$dev" ] && [ -n "${VOLUME_ID:-}" ]; then
    local serial; serial=$(echo "${VOLUME_ID}" | tr -d '-')
    local root_dev; root_dev=$(findmnt -n -o SOURCE / 2>/dev/null | sed 's|^/dev/||;s|p[0-9]*$||') || true
    for i in $(seq 1 30); do
      for ctrl in /sys/class/nvme/nvme*; do
        [ -d "$ctrl" ] || continue
        local node; node="$(basename "$ctrl")n1"
        [ "$node" = "$root_dev" ] && continue
        [ "$(tr -d '[:space:]' < "$ctrl/serial" 2>/dev/null)" = "$serial" ] && { dev="/dev/${node}"; break 2; }
      done
      echo "  waiting for device (${i}/30)"; sleep 5
    done
  fi

  [ -z "$dev" ] && { echo "ERROR: data volume not found"; exit 1; }
  echo "Data volume: ${dev}"

  # unmount if auto-mounted by udisks2
  local mp; mp=$(lsblk -n -o MOUNTPOINT "$dev" 2>/dev/null | grep -v '^$' | head -1) || true
  [ -n "$mp" ] && sudo umount "$mp"

  # format if no filesystem
  if ! blkid "$dev" 2>/dev/null | grep -q 'TYPE='; then
    echo "Formatting ${dev}..."
    sudo mkfs.ext4 -L "$LABEL" "$dev"
  fi

  sudo mkdir -p "$MOUNT_POINT"
  sudo mount "$dev" "$MOUNT_POINT"
  sudo chown github:github "$MOUNT_POINT"
  echo "Mounted at ${MOUNT_POINT}"
}

# --------------------------------------------------------------------------
# umount: sync + lazy unmount
# --------------------------------------------------------------------------
umount_vol() {
  sync
  if sudo umount -l "$MOUNT_POINT"; then
    echo "Unmounted ${MOUNT_POINT}"
  else
    echo "WARN: umount failed"; exit 1
  fi
}

case "${1:-}" in
  attach)  shift; attach "$@" ;;
  detach)  shift; detach "$@" ;;
  delete)  shift; delete "$@" ;;
  mount)   shift; mount_vol "$@" ;;
  umount)  shift; umount_vol "$@" ;;
  *) echo "Usage: $0 {attach|detach|delete|mount|umount}"; exit 1 ;;
esac