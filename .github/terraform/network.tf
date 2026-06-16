# ---------------------------------------------------------------------------
# VPC, subnet, and security group for build instances
# ---------------------------------------------------------------------------

resource "aws_vpc" "build" {
  cidr_block           = "10.0.0.0/16"
  enable_dns_hostnames = true
  enable_dns_support   = true
  tags                 = { Name = "gsi-build", Project = var.project_tag }
}

resource "aws_internet_gateway" "build" {
  vpc_id = aws_vpc.build.id
  tags   = { Name = "gsi-build", Project = var.project_tag }
}

resource "aws_subnet" "build" {
  vpc_id                  = aws_vpc.build.id
  cidr_block              = "10.0.1.0/24"
  availability_zone       = "${var.region}a"
  map_public_ip_on_launch = true
  tags                    = { Name = "gsi-build-a", Project = var.project_tag }
}

resource "aws_route_table" "build" {
  vpc_id = aws_vpc.build.id
  tags   = { Name = "gsi-build", Project = var.project_tag }

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.build.id
  }
}

resource "aws_route_table_association" "build" {
  subnet_id      = aws_subnet.build.id
  route_table_id = aws_route_table.build.id
}

resource "aws_security_group" "build" {
  name        = "gsi-build"
  description = "GSI build instances - all outbound, no inbound"
  vpc_id      = aws_vpc.build.id
  tags        = { Name = "gsi-build", Project = var.project_tag }

  egress {
    from_port   = 0
    to_port     = 0
    protocol    = "-1"
    cidr_blocks = ["0.0.0.0/0"]
  }
}