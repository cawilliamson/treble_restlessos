# ---------------------------------------------------------------------------
# values needed for github repository secrets
# ---------------------------------------------------------------------------

output "aws_access_key_id" {
  description = "access key for the gsi-builder-gha IAM user — set as AWS_ACCESS_KEY_ID secret"
  value       = aws_iam_access_key.gha.id
  sensitive   = true
}

output "aws_secret_access_key" {
  description = "secret key for the gsi-builder-gha IAM user — set as AWS_SECRET_ACCESS_KEY secret"
  value       = aws_iam_access_key.gha.secret
  sensitive   = true
}

output "security_group_id" {
  description = "build security group — set as AWS_SECURITY_GROUP_ID secret"
  value       = aws_security_group.build.id
}


