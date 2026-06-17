#!/usr/bin/env bash
set -euo pipefail

eval "$(jq -r '@sh "REGION=\(.region) INST=\(.instance_type)"')" >&2

aws_bin=$(command -v aws 2>/dev/null || true)
if [ -z "$aws_bin" ]; then
  {
    apt-get update -qq
    apt-get install -y -qq curl unzip
    curl -sf -o /tmp/awscliv2.zip https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip
    unzip -q /tmp/awscliv2.zip -d /tmp
    /tmp/aws/install --update
  } >&2
  aws_bin=/usr/local/bin/aws
fi

scores=$($aws_bin ec2 get-spot-placement-scores \
  --instance-types "$INST" \
  --target-capacity 1 \
  --single-availability-zone \
  --region "$REGION" \
  --output json 2>&1) || {
    echo '{"az":"eu-west-2a"}' >&1
    exit 0
  }

best_az=$(echo "$scores" | jq -r '.SpotPlacementScores | max_by(.Score) | .AvailabilityZone')

jq -n --arg az "$best_az" '{az:$az}'
