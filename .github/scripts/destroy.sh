#!/usr/bin/env bash
# failsafe cleanup: terminate any tagged build instances and delete any tagged
# volumes left behind (e.g. on aborted runs where the per-stage terminate steps
# never ran). the terraform-managed VPC etc. is torn down by the workflow's
# tofu-destroy step. nothing here is fatal.
set -uo pipefail
REGION="eu-west-2"
FILTER=("Name=tag:Project,Values=gsi-build" "Name=tag:Ephemeral,Values=true")

# terminate leftover instances
iids=$(aws ec2 describe-instances --region "$REGION" --filters "${FILTER[@]}" \
  "Name=instance-state-name,Values=pending,running,stopping,shutting-down,stopped" \
  --query 'Reservations[].Instances[].InstanceId' --output text 2>/dev/null \
  | tr '\t' '\n' | grep . || true)
echo "$iids" | xargs -r aws ec2 terminate-instances --region "$REGION" --instance-ids 2>/dev/null || true

# delete leftover volumes: force-detach, then delete (retrying until the volume
# is available after detach). already-gone volumes are skipped.
vids=$(aws ec2 describe-volumes --region "$REGION" --filters "${FILTER[@]}" \
  --query 'Volumes[].VolumeId' --output text 2>/dev/null \
  | tr '\t' '\n' | grep . || true)
for vid in $vids; do
  state=$(aws ec2 describe-volumes --region "$REGION" --volume-ids "$vid" \
    --query 'Volumes[0].State' --output text 2>/dev/null) || continue
  [ "$state" = "deleted" ] && continue
  echo "Deleting volume ${vid} (${state})"
  aws ec2 detach-volume --region "$REGION" --volume-id "$vid" --force 2>/dev/null || true
  for i in $(seq 1 12); do
    aws ec2 delete-volume --region "$REGION" --volume-id "$vid" 2>/dev/null && break
    sleep 5
  done
done

# deregister stale offline self-hosted runners (the EC2 host is gone; the
# registration is free but tidy it up). no concurrent runs share this repo.
if command -v gh >/dev/null && [ -n "${GH_TOKEN:-}" ]; then
  for rid in $(gh api "/repos/${GITHUB_REPOSITORY}/actions/runners?per_page=100" \
      --jq '.runners[] | select(.status=="offline") | .id' 2>/dev/null); do
    echo "Deregistering offline runner ${rid}"
    gh api -X DELETE "/repos/${GITHUB_REPOSITORY}/actions/runners/${rid}" 2>/dev/null || true
  done
fi
