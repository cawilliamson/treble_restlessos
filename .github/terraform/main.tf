# graphiteos GSI build infrastructure
#
# manages the persistent AWS resources (VPC, IAM) needed by the
# github actions workflow to launch ephemeral EC2 spot instances.

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
