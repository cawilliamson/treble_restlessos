# ---------------------------------------------------------------------------
# service-linked role for EC2 spot instances
# ---------------------------------------------------------------------------

resource "aws_iam_service_linked_role" "spot" {
  aws_service_name = "spot.amazonaws.com"
}