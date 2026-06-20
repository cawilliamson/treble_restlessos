#!/usr/bin/env bash
set -euo pipefail

REGION="${AWS_REGION:?}"
REPO="${GITHUB_REPOSITORY:?}"
GH_API="https://api.github.com/repos/${REPO}"

gh_api() { curl -sS --fail-with-body -H "Accept: application/vnd.github+json" -H "Authorization: Bearer ${GH_PAT:?}" "$@"; }

# --------------------------------------------------------------------------
# provision: launch EC2 instance, bootstrap runner, wait for online
# retry is handled by nick-fields/retry in the workflow
# --------------------------------------------------------------------------
provision() {
  local inst_type="${EC2_INSTANCE_TYPE:?}" subnet="${EC2_SUBNET_ID:?}"
  local sg="${EC2_SG_ID:?}" ami="${EC2_AMI_ID:?}"
  local label="${RUNNER_LABEL:?}" packages="${PACKAGES:-}"
  local market="${EC2_MARKET_TYPE:-}" root_vol="${EC2_ROOT_VOLUME:-30}"
  local data_vol="${EC2_DATA_VOLUME:-}" tag="gsi-build"

  # 1. jit runner config
  echo "Fetching JIT config for label '${label}'..."
  local jit runner_name
  runner_name="ec2-${label}-$(date +%s%N)"
  jit=$(gh_api -X POST "${GH_API}/actions/runners/generate-jitconfig" \
    -d "{\"name\":\"${runner_name}\",\"runner_group_id\":1,\"labels\":[\"self-hosted\",\"${label}\"]}" \
    | jq -r '.encoded_jit_config')
  [ -z "$jit" ] && { echo "ERROR: jit config fetch failed"; exit 1; }

  # 2. build shell user-data
  # a plain bash script avoids yaml indentation fragility for the ssh key.
  local ud_file; ud_file=$(mktemp)
  local key_delim; key_delim="KEY_EOF_$(head -c 32 /dev/urandom | base64 | tr -dc 'A-Za-z0-9' | head -c 32)"
  {
    printf '%s\n' '#!/bin/bash' 'set -euo pipefail' ''
    printf '%s\n' '# create the runner user'
    printf '%s\n' 'useradd -m -s /bin/bash github 2>/dev/null || true'
    printf '%s\n' 'usermod -aG sudo,disk github'
    printf '%s\n' "echo 'github ALL=(ALL) NOPASSWD:ALL' > /etc/sudoers.d/github"
    printf '%s\n' 'chmod 440 /etc/sudoers.d/github'
    printf '\n'

    if [ -n "$packages" ]; then
      printf '%s\n' '# install requested packages'
      printf 'DEBIAN_FRONTEND=noninteractive apt-get update && apt-get install -y %s\n' "$packages"
      printf '\n'
    fi

    printf '%s\n' '# ssh key and config'
    printf '%s\n' 'mkdir -p /home/github/.ssh'
    printf "cat > /home/github/.ssh/build_key <<'%s'\n" "$key_delim"
    printf '%s\n' "${BUILD_SSH_KEY:?}"
    printf '%s\n' "$key_delim"
    printf '%s\n' 'chmod 600 /home/github/.ssh/build_key'
    printf '%s\n' "cat > /home/github/.ssh/config <<'SSHEOF'"
    printf '%s\n' 'Host github.com'
    printf '%s\n' '  IdentityFile /home/github/.ssh/build_key'
    printf '%s\n' '  StrictHostKeyChecking no'
    printf '%s\n' '  User git'
    printf '%s\n' ''
    printf '%s\n' 'Host build.chrisaw.io'
    printf '%s\n' '  IdentityFile /home/github/.ssh/build_key'
    printf '%s\n' '  StrictHostKeyChecking no'
    printf '%s\n' '  User chrisaw'
    printf '%s\n' 'SSHEOF'
    printf '%s\n' 'chmod 600 /home/github/.ssh/config'
    printf '%s\n' 'chown -R github:github /home/github/.ssh'
    printf '\n'

    printf '%s\n' '# git identity (required for repo sync)'
    printf '%s\n' "sudo -u github git config --global user.email 'androidbuild@localhost'"
    printf '%s\n' "sudo -u github git config --global user.name 'androidbuild'"
    printf '%s\n' 'sudo -u github git config --global color.ui false'
    printf '\n'

    printf '%s\n' '# repo tool'
    printf '%s\n' 'curl -sf -o /usr/local/bin/repo https://storage.googleapis.com/git-repo-downloads/repo'
    printf '%s\n' 'chmod a+x /usr/local/bin/repo'
    printf '\n'

    printf '%s\n' '# github actions runner'
    printf '%s\n' "v=\$(curl -sf https://api.github.com/repos/actions/runner/releases/latest | jq -r '.tag_name' | sed 's/^v//')"
    printf '%s\n' 'mkdir -p /opt/actions-runner'
    printf '%s\n' 'curl -sf -L "https://github.com/actions/runner/releases/download/v${v}/actions-runner-linux-x64-${v}.tar.gz" | tar -xz -C /opt/actions-runner'
    printf '%s\n' '/opt/actions-runner/bin/installdependencies.sh'
    printf '%s\n' 'chown -R github:github /opt/actions-runner'
    printf '\n'

    printf '%s\n' '# jit runner config'
    printf '%s\n' "printf '%s' '$jit' > /opt/jitconfig"
    printf '%s\n' 'chmod 600 /opt/jitconfig'
    printf '\n'

    printf '%s\n' '# safety shutdown'
    printf '%s\n' "shutdown -P +120 'SAFETY: 2-hour limit reached'"
    printf '\n'

    printf '%s\n' '# start runner'
    printf '%s\n' 'cd /opt/actions-runner && sudo -u github ./run.sh --jitconfig "$(cat /opt/jitconfig)"'
  } > "$ud_file"

  # 3. launch
  local -a market_args=()
  [ -n "$market" ] && market_args=(--instance-market-options "{\"MarketType\":\"${market}\",\"SpotOptions\":{\"SpotInstanceType\":\"one-time\",\"InstanceInterruptionBehavior\":\"terminate\"}}")
  local -a bdm_args=("DeviceName=/dev/sda1,Ebs={VolumeSize=${root_vol},VolumeType=gp3,DeleteOnTermination=true}")
  [ -n "$data_vol" ] && bdm_args+=("DeviceName=/dev/sdf,Ebs={VolumeSize=${data_vol},VolumeType=gp3,DeleteOnTermination=true}")

  # 3. launch
  echo "Launching ${inst_type} in ${subnet}..."
  local iid
  iid=$(aws ec2 run-instances --region "$REGION" \
    --image-id "$ami" --instance-type "$inst_type" --subnet-id "$subnet" \
    --security-group-ids "$sg" --instance-initiated-shutdown-behavior terminate \
    "${market_args[@]}" --block-device-mappings "${bdm_args[@]}" \
    --tag-specifications "ResourceType=instance,Tags=[{Key=Name,Value=${tag}-${label}},{Key=Project,Value=${tag}},{Key=Ephemeral,Value=true}]" \
                    "ResourceType=volume,Tags=[{Key=Project,Value=${tag}},{Key=Ephemeral,Value=true}]" \
    --user-data "file://${ud_file}" --query 'Instances[0].InstanceId' --output text)
  rm -f "$ud_file"

  # 4. wait running
  echo "Instance ${iid} — waiting for running state..."
  aws ec2 wait instance-running --region "$REGION" --instance-ids "$iid"

  # 5. get subnet + data volume id
  local sn vid=""
  sn=$(aws ec2 describe-instances --region "$REGION" --instance-ids "$iid" \
    --query 'Reservations[0].Instances[0].SubnetId' --output text)
  for i in $(seq 1 10); do
    vid=$(aws ec2 describe-instances --region "$REGION" --instance-ids "$iid" \
      --query "Reservations[0].Instances[0].BlockDeviceMappings[?DeviceName!=\`/dev/sda1\`].Ebs.VolumeId" \
      --output text 2>/dev/null) || true
    [ -n "$vid" ] && [ "$vid" != "None" ] && break
    sleep 5
  done

  # 6. wait for runner online
  echo "Waiting for runner '${label}' to register..."
  local registered=false
  for i in $(seq 1 40); do
    local online
    online=$(gh_api "${GH_API}/actions/runners?per_page=100" \
      | jq --arg l "$label" -r '[.runners[]|select(.labels[].name==$l)|select(.status=="online")]|length')
    echo "  attempt ${i}/40: ${online:-0} online runner(s) with label '${label}'"
    if [ "${online:-0}" -ge 1 ] 2>/dev/null; then
      echo "Runner online"
      registered=true
      break
    fi
    sleep 15
  done

  # dump console output if runner never came online
  if [ "$registered" = false ]; then
    echo "=== ERROR: runner did not register — fetching EC2 console output ==="
    aws ec2 get-console-output --region "$REGION" --instance-id "$iid" \
      --query 'Output' --output text 2>/dev/null | tail -100 || true
    echo "=== end console output ==="
    exit 1
  fi

  # 7. outputs
  {
    echo "instance_id=${iid}"
    echo "subnet_id=${sn}"
    [ -n "$vid" ] && [ "$vid" != "None" ] && echo "data_volume_id=${vid}"
  } >> "$GITHUB_OUTPUT"
  echo "Provisioned ${iid} in subnet ${sn}"
}

# --------------------------------------------------------------------------
# select_az: pick the AZ with the best spot placement score for the large
# instance type and print the corresponding subnet id.
# --------------------------------------------------------------------------
select_az() {
  local inst_type="${EC2_LARGE_INSTANCE:?}"
  local subnet_a="${EC2_SUBNET_ID_A:?}"
  local subnet_b="${EC2_SUBNET_ID_B:?}"
  local subnet_c="${EC2_SUBNET_ID_C:?}"

  local az_map
  az_map=$(aws ec2 describe-availability-zones \
    --region "$REGION" \
    --filters "Name=state,Values=available" \
    --query 'AvailabilityZones[?starts_with(ZoneName, `'"$REGION"'`)].{id: ZoneId, name: ZoneName}' \
    --output json) || {
    echo "ERROR: failed to describe availability zones" >&2
    exit 1
  }

  local scores
  scores=$(aws ec2 get-spot-placement-scores \
    --instance-types "$inst_type" \
    --target-capacity 1 \
    --single-availability-zone \
    --region "$REGION" \
    --region-names "$REGION" \
    --output json) || {
    echo "ERROR: failed to fetch spot placement scores" >&2
    exit 1
  }

  echo "$scores" | jq -r '.SpotPlacementScores[] | "\(.AvailabilityZoneId): \(.Score)"' >&2

  local best
  best=$(echo "$scores" | jq -r '.SpotPlacementScores | max_by(.Score)')
  local best_az_id score
  best_az_id=$(echo "$best" | jq -r '.AvailabilityZoneId')
  score=$(echo "$best" | jq -r '.Score')
  if [ -z "$best_az_id" ] || [ "$best_az_id" = "null" ]; then
    echo "ERROR: spot placement scores returned no usable AZ" >&2
    exit 1
  fi

  local best_az_name
  best_az_name=$(echo "$az_map" | jq -r --arg id "$best_az_id" '.[] | select(.id == $id) | .name')
  if [ -z "$best_az_name" ] || [ "$best_az_name" = "null" ]; then
    echo "ERROR: could not map AZ id $best_az_id to a name" >&2
    exit 1
  fi

  echo "selected $best_az_id ($best_az_name) with score $score" >&2

  case "$best_az_name" in
    *a) echo '{"subnet":"'$subnet_a'","az":"'$best_az_name'"}' ;;
    *b) echo '{"subnet":"'$subnet_b'","az":"'$best_az_name'"}' ;;
    *c) echo '{"subnet":"'$subnet_c'","az":"'$best_az_name'"}' ;;
    *) echo "ERROR: unexpected AZ name $best_az_name" >&2; exit 1 ;;
  esac
}

# --------------------------------------------------------------------------
# kill: terminate instance + deregister runner (best-effort)
# --------------------------------------------------------------------------
kill_runner() {
  local iid="${EC2_INSTANCE_ID:-}" label="${RUNNER_LABEL:-}"
  [ -z "$iid" ] && { echo "No instance to kill"; return 0; }

  echo "Terminating ${iid}..."
  aws ec2 terminate-instances --region "$REGION" --instance-ids "$iid" || true

  if [ -n "$label" ]; then
    local rid
    rid=$(gh_api "${GH_API}/actions/runners" \
      | jq --arg l "$label" -r '.runners[]|select(.labels[].name==$l)|.id' 2>/dev/null | head -1) || true
    if [ -n "$rid" ] && [ "$rid" != "null" ]; then
      gh_api -X DELETE "${GH_API}/actions/runners/${rid}" || true
      echo "Deregistered runner ${rid}"
    fi
  fi
}

case "${1:-}" in
  provision) shift; provision "$@" ;;
  select-az) shift; select_az "$@" ;;
  kill)      shift; kill_runner "$@" ;;
  *) echo "Usage: $0 {provision|select-az|kill}"; exit 1 ;;
esac
