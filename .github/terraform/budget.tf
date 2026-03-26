# ---------------------------------------------------------------------------
# $200/month cost budget
#
# AWS budget actions (STOP_EC2_INSTANCES) require static instance IDs at
# creation time, so they can't dynamically target ephemeral spot instances.
# instead, the hourly EventBridge + SSM sweep in stale-instance-sweep.tf
# handles the hard kill. this budget exists as a monitoring/alerting layer.
# ---------------------------------------------------------------------------

resource "aws_budgets_budget" "monthly" {
  name         = "gsi-build-monthly-${var.monthly_budget_usd}"
  budget_type  = "COST"
  limit_amount = tostring(var.monthly_budget_usd)
  limit_unit   = "USD"
  time_unit    = "MONTHLY"

  cost_types {
    include_credit             = false
    include_discount           = true
    include_other_subscription = true
    include_recurring          = true
    include_refund             = false
    include_subscription       = true
    include_support            = true
    include_tax                = true
    include_upfront            = true
    use_amortized              = false
    use_blended                = false
  }

  notification {
    comparison_operator        = "GREATER_THAN"
    notification_type          = "ACTUAL"
    threshold                  = 100
    threshold_type             = "PERCENTAGE"
    subscriber_email_addresses = [var.notification_email]
  }
}
