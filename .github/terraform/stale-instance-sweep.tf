# ---------------------------------------------------------------------------
# hourly sweep: terminate any tagged build instance older than 2 hours
# ---------------------------------------------------------------------------

resource "aws_ssm_document" "terminate_stale" {
  name            = "gsi-build-terminate-stale-instances"
  document_type   = "Automation"
  document_format = "YAML"
  tags            = { Project = var.project_tag }

  content = yamlencode({
    schemaVersion = "0.3"
    description   = "terminate GSI build instances older than ${var.instance_max_age_hours} hours"
    assumeRole    = aws_iam_role.ssm_automation.arn
    mainSteps = [
      {
        name   = "terminateStaleInstances"
        action = "aws:executeScript"
        inputs = {
          Runtime = "python3.11"
          Handler = "handler"
          Script  = <<-PYTHON
            import boto3
            import datetime

            def handler(event, context):
                ec2 = boto3.client("ec2", region_name="${var.region}")
                now = datetime.datetime.now(datetime.timezone.utc)
                cutoff = now - datetime.timedelta(hours=${var.instance_max_age_hours})
                response = ec2.describe_instances(
                    Filters=[
                        {"Name": "tag:Project", "Values": ["${var.project_tag}"]},
                        {"Name": "tag:Ephemeral", "Values": ["true"]},
                        {"Name": "instance-state-name", "Values": ["running", "pending"]},
                    ]
                )
                terminated = []
                for reservation in response["Reservations"]:
                    for instance in reservation["Instances"]:
                        if instance["LaunchTime"] < cutoff:
                            instance_id = instance["InstanceId"]
                            ec2.terminate_instances(InstanceIds=[instance_id])
                            terminated.append(instance_id)
                return {"terminated": terminated}
          PYTHON
        }
      },
    ]
  })
}

# ---------------------------------------------------------------------------
# EventBridge: trigger the sweep every hour
# ---------------------------------------------------------------------------

resource "aws_cloudwatch_event_rule" "stale_cleanup" {
  name                = "gsi-build-stale-cleanup"
  description         = "terminate GSI build EC2 instances older than ${var.instance_max_age_hours} hours"
  schedule_expression = "rate(1 hour)"
  tags                = { Project = var.project_tag }
}

resource "aws_cloudwatch_event_target" "stale_cleanup" {
  rule     = aws_cloudwatch_event_rule.stale_cleanup.name
  arn      = "arn:aws:ssm:${var.region}:${data.aws_caller_identity.current.account_id}:automation-definition/${aws_ssm_document.terminate_stale.name}"
  role_arn = aws_iam_role.eventbridge_ssm.arn

  input = jsonencode({
    AutomationAssumeRole = [aws_iam_role.ssm_automation.arn]
  })
}
