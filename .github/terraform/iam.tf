# ---------------------------------------------------------------------------
# service-linked role for EC2 spot instances
# ---------------------------------------------------------------------------

resource "aws_iam_service_linked_role" "spot" {
  aws_service_name = "spot.amazonaws.com"
}

# ---------------------------------------------------------------------------
# IAM user for github actions workflow
# ---------------------------------------------------------------------------

resource "aws_iam_user" "gha" {
  name = "gsi-builder-gha"
  tags = { Project = var.project_tag }
}

resource "aws_iam_user_policy" "gha_ec2" {
  name = "ec2-spot-build"
  user = aws_iam_user.gha.name

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Sid    = "EC2SpotBuild"
        Effect = "Allow"
        Action = [
          "ec2:CreateSnapshot",
          "ec2:CreateTags",
          "ec2:DeleteSnapshot",
          "ec2:DescribeImages",
          "ec2:DescribeInstanceStatus",
          "ec2:DescribeInstances",
          "ec2:DescribeSnapshots",
          "ec2:DescribeVolumes",
          "ec2:RunInstances",
          "ec2:TerminateInstances",
        ]
        Resource = "*"
      },
    ]
  })
}

resource "aws_iam_access_key" "gha" {
  user = aws_iam_user.gha.name
}
