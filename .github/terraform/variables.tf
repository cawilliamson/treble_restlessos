variable "region" {
  description = "AWS region for build instances"
  type        = string
  default     = "eu-west-2"
}

variable "monthly_budget_usd" {
  description = "hard monthly spend cap in USD — instances are killed when this is reached"
  type        = number
  default     = 200
}

variable "instance_max_age_hours" {
  description = "maximum instance lifetime before the hourly sweep terminates it"
  type        = number
  default     = 2
}

variable "notification_email" {
  description = "email for budget breach notification (paper trail only — action is automatic)"
  type        = string
  default     = "contact@chrisaw.io"
}

variable "project_tag" {
  description = "tag value used to identify build instances"
  type        = string
  default     = "treble-grapheneos"
}
