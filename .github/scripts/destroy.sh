#!/usr/bin/env bash
# destroy: find and tear down every ephemeral build resource by tag, so the
# cleanup is failsafe and never depends on workflow outputs being populated
# (which they often aren't on aborted builds). the terraform-managed VPC,
# subnets, security group and IAM role are left for the workflow's
# tofu-destroy step.
#
# nothing here is fatal: missing/empty resources are skipped. leaving EC2
# hosts or EBS volumes behind is what costs money, so this always sweeps by
# tag regardless of how the build ended.
set -uo pipefail

REGION="${AWS_REGION:?}"
REPO="${GITHUB_REPOSITORY:?}"
GH_API="https://api.github.com/repos/${REPO}"
GH_PAT="${GH_PAT:-}"
PROJECT_TAG="gsi-build"

# --------------------------------------------------------------------------
# terminate every instance tagged Project=gsi-build that isn't already gone
# --------------------------------------------------------------------------
echo "::group::Terminate instances (tag Project=${PROJECT_TAG})"
mapfile -t IIDS < <(aws ec2 describe-instances --region "$REGION" \
  --filters "Name=tag:Project,Values=${PROJECT_TAG}" \
            "Name=instance-state-name,Values=pending,running,stopping,shutting-down,stopped" \
  --query 'Reservations[].Instances[].InstanceId' --output text 2>/dev/null \
  | tr '\t' '\n' | grep -v '^$' || true)
if [ "${#IIDS[@]}" -gt 0 ]; then
  echo "Terminating: ${IIDS[*]}"
  aws ec2 terminate-instances --region "$REGION" --instance-ids "${IIDS[@]}" 2>/dev/null || true
  aws ec2 wait instance-terminated --region "$REGION" --instance-ids "${IIDS[@]}" 2>/dev/null || true
else
  echo "No instances to terminate"
fi
echo "::endgroup::"

# --------------------------------------------------------------------------
# delete every data volume tagged Project=gsi-build. force-detach first (a
# volume may still be attached to a terminating host) then wait for available.
# --------------------------------------------------------------------------
echo "::group::Delete data volumes (tag Project=${PROJECT_TAG})"
mapfile -t VIDS < <(aws ec2 describe-volumes --region "$REGION" \
  --filters "Name=tag:Project,Values=${PROJECT_TAG}" \
  --query 'Volumes[].VolumeId' --output text 2>/dev/null \
  | tr '\t' '\n' | grep -v '^$' || true)
for vid in "${VIDS[@]}"; do
  echo "Volume ${vid}: force-detaching and deleting"
  aws ec2 detach-volume --region "$REGION" --volume-id "$vid" --force 2>/dev/null || true
  for i in $(seq 1 30); do
    state=$(aws ec2 describe-volumes --region "$REGION" --volume-ids "$vid" \
      --query 'Volumes[0].State' --output text 2>/dev/null) || { echo "  ${vid} gone"; break; }
    case "$state" in
      available) aws ec2 delete-volume --region "$REGION" --volume-id "$vid" 2>/dev/null && { echo "  ${vid} deleted"; break; } ;;
      deleted) echo "  ${vid} deleted"; break ;;
      *) echo "  ${vid} ${state} (${i}/30)"; sleep 5 ;;
    esac
  done
done
[ "${#VIDS[@]}" -eq 0 ] && echo "No volumes to delete"
echo "::endgroup::"

# --------------------------------------------------------------------------
# deregister offline self-hosted runners whose name belongs to this build
# (the EC2 host is gone, so the runner is inert and free, but tidy it up).
# --------------------------------------------------------------------------
echo "::group::Deregister offline build runners"
if [ -n "$GH_PAT" ]; then
  mapfile -t RIDS < <(curl -sS -H "Authorization: Bearer ${GH_PAT}" \
    -H "Accept: application/vnd.github+json" \
    "${GH_API}/actions/runners?per_page=100" 2>/dev/null \
    | jq -r '.runners[] | select(.name|test("gsi-build-")) | select(.status=="offline") | .id' 2>/dev/null || true)
  for rid in "${RIDS[@]}"; do
    echo "Deregistering runner ${rid}"
    curl -sS -X DELETE -H "Authorization: Bearer ${GH_PAT}" \
      "${GH_API}/actions/runners/${rid}" 2>/dev/null || true
  done
  [ "${#RIDS[@]}" -eq 0 ] && echo "No offline build runners"
else
  echo "No GH_PAT; skipping runner deregistration"
fi
echo "::endgroup::"

echo "Sweep complete"