data "external" "spot_score" {
  program = ["bash", "-c", <<EOF
set -euo pipefail
eval "$(jq -r '@sh "REGION=\\(.region) INST=\\(.instance_type)"')"
scores=$(aws ec2 get-spot-placement-scores \\
  --instance-types "$$INST" \\
  --target-capacity 1 \\
  --single-availability-zone \\
  --region "$$REGION" \\
  --output json)
best_az=$(echo "$$scores" | jq -r '.SpotPlacementScores | max_by(.Score) | .AvailabilityZone')
jq -n --arg az "$$best_az" '{az:$$az}'
EOF
  ]

  query = {
    region        = var.region
    instance_type = var.large_instance_type
  }
}
