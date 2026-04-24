# ---------------------------------------------------------------------------
# VPC, subnet, and security group for build instances — eu-west-1 (Ireland)
# ---------------------------------------------------------------------------
# Mirrors network.tf but uses the eu-west-1 provider so the same build
# infrastructure exists in both regions.  The workflow discovers subnets
# and security groups by tag/name, so no workflow changes are needed beyond
# adding the region to the failover list.

resource "aws_vpc" "build_ireland" {
  provider = aws.ireland

  cidr_block           = "10.1.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true
  tags                 = { Name = "gsi-build", Project = var.project_tag }
}

resource "aws_internet_gateway" "build_ireland" {
  provider = aws.ireland

  vpc_id = aws_vpc.build_ireland.id
  tags   = { Name = "gsi-build", Project = var.project_tag }
}

resource "aws_subnet" "build_ireland" {
  provider = aws.ireland

  for_each                = toset(["a", "b", "c"])
  vpc_id                  = aws_vpc.build_ireland.id
  cidr_block              = "10.1.${index(["a", "b", "c"], each.key) + 1}.0/24"
  availability_zone       = "eu-west-1${each.key}"
  map_public_ip_on_launch = true
  tags                    = { Name = "gsi-build-${each.key}", Project = var.project_tag }
}

resource "aws_route_table" "build_ireland" {
  provider = aws.ireland

  vpc_id = aws_vpc.build_ireland.id
  tags   = { Name = "gsi-build", Project = var.project_tag }

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.build_ireland.id
  }
}

resource "aws_route_table_association" "build_ireland" {
  provider = aws.ireland

  for_each       = aws_subnet.build_ireland
  subnet_id      = each.value.id
  route_table_id = aws_route_table.build_ireland.id
}

resource "aws_security_group" "build_ireland" {
  provider = aws.ireland

  name        = "gsi-build"
  description = "GSI build instances - all outbound, no inbound"
  vpc_id      = aws_vpc.build_ireland.id
  tags        = { Name = "gsi-build", Project = var.project_tag }

  # all outbound (github, upstream repos, build.chrisaw.io rsync)
  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }

  # no inbound — runner connects out to github, not the other way round
}