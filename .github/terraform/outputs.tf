# ---------------------------------------------------------------------------
# workflow outputs consumed by the build pipeline
# ---------------------------------------------------------------------------

output "subnet_id" {
  description = "build subnet in primary region"
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