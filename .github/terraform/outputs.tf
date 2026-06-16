output "subnet_id" {
  description = "build subnet in AZ a (eu-west-2a)"
  value       = aws_subnet.build["a"].id
}

output "security_group_id" {
  description = "build security group for ephemeral instances"
  value       = aws_security_group.build.id
}

output "ami_id" {
  description = "latest canonical ubuntu 24.04 ami"
  value       = data.aws_ami.ubuntu.id
}