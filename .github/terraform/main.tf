# grapheneos GSI build infrastructure — safety guardrails
#
# sets up all AWS resources needed to prevent runaway costs from
# EC2 spot instances. the actual instances are launched ephemerally
# by the github actions workflow — this only manages the safety net.

terraform {
  required_version = ">= 1.5"

  required_providers {
    aws = {
      source  = "hashicorp/aws"
      version = "~> 5.0"
    }
  }
}

provider "aws" {
  region = var.region
}

data "aws_caller_identity" "current" {}
