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
          "ec2:CreateTags",
          "ec2:DescribeImages",
          "ec2:DescribeInstanceStatus",
          "ec2:DescribeInstances",
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

# ---------------------------------------------------------------------------
# IAM role for EventBridge to invoke SSM automation
# ---------------------------------------------------------------------------

resource "aws_iam_role" "eventbridge_ssm" {
  name = "gsi-build-eventbridge-ssm"
  tags = { Project = var.project_tag }

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect    = "Allow"
        Principal = { Service = "events.amazonaws.com" }
        Action    = "sts:AssumeRole"
      },
    ]
  })
}

resource "aws_iam_role_policy" "eventbridge_ssm" {
  name = "invoke-ssm-and-terminate"
  role = aws_iam_role.eventbridge_ssm.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = "ssm:StartAutomationExecution"
        Resource = aws_ssm_document.terminate_stale.arn
      },
      {
        Effect   = "Allow"
        Action   = ["ec2:DescribeInstances", "ec2:TerminateInstances"]
        Resource = "*"
      },
      {
        Effect   = "Allow"
        Action   = "iam:PassRole"
        Resource = aws_iam_role.ssm_automation.arn
      },
    ]
  })
}

# ---------------------------------------------------------------------------
# IAM role for SSM automation execution
# ---------------------------------------------------------------------------

resource "aws_iam_role" "ssm_automation" {
  name = "gsi-build-ssm-automation"
  tags = { Project = var.project_tag }

  assume_role_policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect    = "Allow"
        Principal = { Service = "ssm.amazonaws.com" }
        Action    = "sts:AssumeRole"
      },
    ]
  })
}

resource "aws_iam_role_policy" "ssm_automation" {
  name = "ec2-terminate"
  role = aws_iam_role.ssm_automation.id

  policy = jsonencode({
    Version = "2012-10-17"
    Statement = [
      {
        Effect   = "Allow"
        Action   = ["ec2:DescribeInstances", "ec2:TerminateInstances"]
        Resource = "*"
      },
    ]
  })
}
