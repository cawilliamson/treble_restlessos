import {
  to = aws_vpc.build
  id = "vpc-01737f432ba3ff50c"
}

import {
  to = aws_internet_gateway.build
  id = "igw-035ed13dc0cff4152"
}

import {
  to = aws_subnet.build
  id = "subnet-05b4405ebd4eaf060"
}

import {
  to = aws_route_table.build
  id = "rtb-0653804d3a6f242f5"
}

import {
  to = aws_route_table_association.build
  id = "subnet-05b4405ebd4eaf060/rtb-0653804d3a6f242f5"
}

import {
  to = aws_security_group.build
  id = "sg-072d86797a735bfe3"
}

import {
  to = aws_iam_service_linked_role.spot
  id = "arn:aws:iam::690475792081:role/aws-service-role/spot.amazonaws.com/AWSServiceRoleForEC2Spot"
}