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

resource "aws_subnet" "build_a" {
  vpc_id                  = aws_vpc.build.id
  cidr_block              = "10.0.4.0/24"
  availability_zone       = "${var.region}a"
  map_public_ip_on_launch = true
  tags                    = { Name = "gsi-build-a", Project = var.project_tag }
}

resource "aws_subnet" "build_b" {
  vpc_id                  = aws_vpc.build.id
  cidr_block              = "10.0.2.0/24"
  availability_zone       = "${var.region}b"
  map_public_ip_on_launch = true
  tags                    = { Name = "gsi-build-b", Project = var.project_tag }
}

resource "aws_subnet" "build_c" {
  vpc_id                  = aws_vpc.build.id
  cidr_block              = "10.0.3.0/24"
  availability_zone       = "${var.region}c"
  map_public_ip_on_launch = true
  tags                    = { Name = "gsi-build-c", Project = var.project_tag }
}

resource "aws_route_table" "build" {
  vpc_id = aws_vpc.build.id
  tags   = { Name = "gsi-build", Project = var.project_tag }

  route {
    cidr_block = "0.0.0.0/0"
    gateway_id = aws_internet_gateway.build.id
  }
}

resource "aws_route_table_association" "build_a" {
  subnet_id      = aws_subnet.build_a.id
  route_table_id = aws_route_table.build.id
}

resource "aws_route_table_association" "build_b" {
  subnet_id      = aws_subnet.build_b.id
  route_table_id = aws_route_table.build.id
}

resource "aws_route_table_association" "build_c" {
  subnet_id      = aws_subnet.build_c.id
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