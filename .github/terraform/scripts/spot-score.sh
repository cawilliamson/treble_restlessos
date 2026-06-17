#!/usr/bin/env bash
set -euo pipefail

eval "$(jq -r '@sh "REGION=\(.region) INST=\(.instance_type)"')"

aws_bin=$(command -v aws || true)
if [ -z "$aws_bin" ]; then
  apt-get update -qq
  apt-get install -y -qq curl unzip
  curl -sf -o /tmp/awscliv2.zip https://awscli.amazonaws.com/awscli-exe-linux-x86_64.zip
  unzip -q /tmp/awscliv2.zip -d /tmp
  /tmp/aws/install --update
  aws_bin=/usr/local/bin/aws
fi

scores=$($aws_bin ec2 get-spot-placement-scores \
  --instance-types "$INST" \
  --target-capacity 1 \
  --single-availability-zone \
  --region "$REGION" \
  --output json)

best_az=$(echo "$scores" | jq -r '.SpotPlacementScores | max_by(.Score) | .AvailabilityZone')

jq -n --arg az "$best_az" '{az:$az}'
