variable "region" {
  description = "AWS region for build instances"
  type        = string
  default     = "eu-west-2"
}

variable "project_tag" {
  description = "tag value used to identify build instances"
  type        = string
  default     = "gsi-build"
}

variable "large_instance_type" {
  description = "large instance type used to determine best AZ via spot placement score"
  type        = string
  default     = "c7i.48xlarge"
}
