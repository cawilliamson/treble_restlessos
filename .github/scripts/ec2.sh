#!/usr/bin/env bash
set -euo pipefail

REGION="${AWS_REGION:?}"
REPO="${GITHUB_REPOSITORY:?}"
GH_API="https://api.github.com/repos/${REPO}"

gh_api() { curl -sf -H "Accept: application/vnd.github+json" -H "Authorization: Bearer ${GH_PAT:?}" "$@"; }

# --------------------------------------------------------------------------
# provision: launch EC2 instance, bootstrap runner, wait for online
# retry is handled by nick-fields/retry in the workflow
# --------------------------------------------------------------------------
provision() {
  local inst_type="${EC2_INSTANCE_TYPE:?}" subnet="${EC2_SUBNET_ID:?}"
  local sg="${EC2_SG_ID:?}" ami="${EC2_AMI_ID:?}"
  local label="${RUNNER_LABEL:?}" packages="${PACKAGES:-}"
  local market="${EC2_MARKET_TYPE:-}" root_vol="${EC2_ROOT_VOLUME:-30}"
  local data_vol="${EC2_DATA_VOLUME:-}" tag="${PROJECT_TAG:-gsi-build}"

  # 1. jit runner config
  echo "Fetching JIT config for label '${label}'..."
  local jit
  jit=$(gh_api -X POST "${GH_API}/actions/runners/generate-jitconfig" \
    -d "{\"name\":\"ec2-${label}\",\"runner_group_id\":1,\"labels\":[\"${label}\"]}" \
    | jq -r '.encoded_jit_config')
  [ -z "$jit" ] && { echo "ERROR: jit config fetch failed"; exit 1; }

  # 2. build cloud-config user-data
  local key_b64 jit_b64 pkg_yaml=""
  key_b64=$(printf '%s' "${BUILD_SSH_KEY:?}" | base64 --wrap=0)
  jit_b64=$(printf '%s' "$jit" | base64 --wrap=0)
  [ -n "$packages" ] && for p in $packages; do pkg_yaml+="  - ${p}"$'\n'; done

  local userdata
  userdata=$(cat <<EOF
#cloud-config
package_update: true
${pkg_yaml:+packages:
${pkg_yaml}}
users:
  - default
  - name: github
    shell: /bin/bash
    groups: [sudo, disk]
    sudo: ['ALL=(ALL) NOPASSWD:ALL']
    home: /home/github
write_files:
  - path: /home/github/.ssh/build_key
    permissions: '0600'
    encoding: b64
    content: ${key_b64}
  - path: /home/github/.ssh/config
    permissions: '0600'
    content: |
      Host github.com
        IdentityFile /home/github/.ssh/build_key
        StrictHostKeyChecking no
        User git
      Host build.chrisaw.io
        IdentityFile /home/github/.ssh/build_key
        StrictHostKeyChecking no
        User chrisaw
  - path: /opt/jitconfig
    permissions: '0600'
    encoding: b64
    content: ${jit_b64}
runcmd:
  - chown -R github:github /home/github/.ssh
  - sudo -u github git config --global user.email 'androidbuild@localhost'
  - sudo -u github git config --global user.name 'androidbuild'
  - sudo -u github git config --global color.ui false
  - curl -sf -o /usr/local/bin/repo https://storage.googleapis.com/git-repo-downloads/repo
  - chmod a+x /usr/local/bin/repo
  - |
    v=\$(curl -sf https://api.github.com/repos/actions/runner/releases/latest | jq -r '.tag_name' | sed 's/^v//')
    mkdir -p /opt/actions-runner
    curl -sf -L "https://github.com/actions/runner/releases/download/v\${v}/actions-runner-linux-x64-\${v}.tar.gz" | tar -xz -C /opt/actions-runner
    /opt/actions-runner/bin/installdependencies.sh
    chown -R github:github /opt/actions-runner
  - shutdown -P +120 'SAFETY: 2-hour limit reached'
  - cd /opt/actions-runner && sudo -u github ./run.sh --jitconfig "\$(cat /opt/jitconfig)"
EOF
)
  local ud_file; ud_file=$(mktemp); printf '%s' "$userdata" > "$ud_file"

  # 3. launch
  local -a market_args=()
  [ -n "$market" ] && market_args=(--instance-market-options "{\"MarketType\":\"${market}\",\"SpotOptions\":{\"SpotInstanceType\":\"one-time\",\"InstanceInterruptionBehavior\":\"terminate\"}}")
  local -a bdm_args=("DeviceName=/dev/sda1,Ebs={VolumeSize=${root_vol},VolumeType=gp3,DeleteOnTermination=true}")
  [ -n "$data_vol" ] && bdm_args+=("DeviceName=/dev/sdf,Ebs={VolumeSize=${data_vol},VolumeType=gp3,DeleteOnTermination=true}")

  echo "Launching ${inst_type} in ${subnet}..."
  local iid
  iid=$(aws ec2 run-instances --region "$REGION" \
    --image-id "$ami" --instance-type "$inst_type" --subnet-id "$subnet" \
    --security-group-ids "$sg" --instance-initiated-shutdown-behavior terminate \
    "${market_args[@]}" --block-device-mappings "${bdm_args[@]}" \
    --tag-specifications "ResourceType=instance,Tags=[{Key=Name,Value=${tag}-${label}},{Key=Project,Value=${tag}},{Key=Ephemeral,Value=true}]" \
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
  for i in $(seq 1 40); do
    local online
    online=$(gh_api "${GH_API}/actions/runners" \
      | jq --arg l "$label" -r '[.runners[]|select(.labels[].name==$l)|select(.status=="online")]|length')
    [ "${online:-0}" -ge 1 ] && { echo "Runner online"; break; }
    echo "  attempt ${i}/40"; sleep 15
  done

  # 7. outputs
  {
    echo "instance_id=${iid}"
    echo "subnet_id=${sn}"
    [ -n "$vid" ] && [ "$vid" != "None" ] && echo "data_volume_id=${vid}"
  } >> "$GITHUB_OUTPUT"
  echo "Provisioned ${iid} in subnet ${sn}"
}

# --------------------------------------------------------------------------
# kill: terminate instance + deregister runner (best-effort)
# --------------------------------------------------------------------------
kill_runner() {
  local iid="${EC2_INSTANCE_ID:?}" label="${RUNNER_LABEL:-}"

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
  kill)      shift; kill_runner "$@" ;;
  *) echo "Usage: $0 {provision|kill}"; exit 1 ;;
esac