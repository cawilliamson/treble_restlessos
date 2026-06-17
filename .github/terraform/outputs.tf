output "subnet_id" {
  description = "build subnet in the AZ with the best spot placement score"
  value       = data.external.spot_score.result.az == null ? aws_subnet.build_a.id : (endswith(data.external.spot_score.result.az, "a") ? aws_subnet.build_a.id : endswith(data.external.spot_score.result.az, "b") ? aws_subnet.build_b.id : aws_subnet.build_c.id)
}

output "security_group_id" {
  description = "build security group for ephemeral instances"
  value       = aws_security_group.build.id
}

output "ami_id" {
  description = "latest canonical ubuntu 24.04 ami"
  value       = data.aws_ami.ubuntu.id
}
