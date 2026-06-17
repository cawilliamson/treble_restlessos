output "subnet_id_a" {
  description = "build subnet in AZ a"
  value       = aws_subnet.build_a.id
}

output "subnet_id_b" {
  description = "build subnet in AZ b"
  value       = aws_subnet.build_b.id
}

output "subnet_id_c" {
  description = "build subnet in AZ c"
  value       = aws_subnet.build_c.id
}

output "security_group_id" {
  description = "build security group for ephemeral instances"
  value       = aws_security_group.build.id
}

output "ami_id" {
  description = "latest canonical ubuntu 24.04 ami"
  value       = data.aws_ami.ubuntu.id
}
