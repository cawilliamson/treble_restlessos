# ---------------------------------------------------------------------------
# workflow outputs consumed by the build pipeline
# ---------------------------------------------------------------------------

output "availability_zones" {
  description = "json for ec2-github-runner availability-zones-config — tries each AZ in order"
  value = jsonencode([
    for v in aws_subnet.build : {
      imageId         = data.aws_ami.ubuntu.id
      subnetId        = v.id
      securityGroupId = aws_security_group.build.id
    }
  ])
}

output "subnet_id" {
  description = "build subnet in primary AZ (a)"
  value       = aws_subnet.build["a"].id
}

output "security_group_id" {
  description = "build security group for ephemeral instances"
  value       = aws_security_group.build.id
}

output "ami_id" {
  description = "latest canonical ubuntu 24.04 ami in primary region"
  value       = data.aws_ami.ubuntu.id
}