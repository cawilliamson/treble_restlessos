import {
  to = aws_vpc.build
  id = "vpc-01737f432ba3ff50c"
}

import {
  to = aws_internet_gateway.build
  id = "igw-035ed13dc0cff4152"
}

import {
  to = aws_subnet.build["a"]
  id = "subnet-05b4405ebd4eaf060"
}

import {
  to = aws_subnet.build["b"]
  id = "subnet-0fc99f8ce97ad502a"
}

import {
  to = aws_subnet.build["c"]
  id = "subnet-00a9ea820ac60a0c0"
}

import {
  to = aws_route_table.build
  id = "rtb-0653804d3a6f242f5"
}

import {
  to = aws_route_table_association.build["a"]
  id = "subnet-05b4405ebd4eaf060/rtb-0653804d3a6f242f5"
}

import {
  to = aws_route_table_association.build["b"]
  id = "subnet-0fc99f8ce97ad502a/rtb-0653804d3a6f242f5"
}

import {
  to = aws_route_table_association.build["c"]
  id = "subnet-00a9ea820ac60a0c0/rtb-0653804d3a6f242f5"
}

import {
  to = aws_security_group.build
  id = "sg-072d86797a735bfe3"
}

import {
  to = aws_vpc.build_ireland
  id = "vpc-0e6c094e3beb88480"
}

import {
  to = aws_internet_gateway.build_ireland
  id = "igw-086c653013bd8a7ff"
}

import {
  to = aws_subnet.build_ireland["a"]
  id = "subnet-07cbbebf71437cf82"
}

import {
  to = aws_subnet.build_ireland["b"]
  id = "subnet-0455ffe5942e72c66"
}

import {
  to = aws_subnet.build_ireland["c"]
  id = "subnet-0d061b649309bb70a"
}

import {
  to = aws_route_table.build_ireland
  id = "rtb-0f29bc668b29fe7f3"
}

import {
  to = aws_route_table_association.build_ireland["a"]
  id = "subnet-07cbbebf71437cf82/rtb-0f29bc668b29fe7f3"
}

import {
  to = aws_route_table_association.build_ireland["b"]
  id = "subnet-0455ffe5942e72c66/rtb-0f29bc668b29fe7f3"
}

import {
  to = aws_route_table_association.build_ireland["c"]
  id = "subnet-0d061b649309bb70a/rtb-0f29bc668b29fe7f3"
}

import {
  to = aws_security_group.build_ireland
  id = "sg-0e9e3c101891e227d"
}

import {
  to = aws_iam_service_linked_role.spot
  id = "arn:aws:iam::690475792081:role/aws-service-role/spot.amazonaws.com/AWSServiceRoleForEC2Spot"
}
