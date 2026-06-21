# ---------------------------------------------------------------------------
# service-linked role for EC2 spot instances
# ---------------------------------------------------------------------------
#
# the spot service-linked role is account-scoped and aws-managed (aws creates
# it automatically on the first spot request in the account). managing it in
# terraform forced a destroy every run that raced spot-request teardown: the
# role delete failed with "Open or Active spot instance requests found" while
# a just-terminated instance's one-time spot request was still closing, which
# needlessly turned the destroy job red.
#
# drop terraform management of the role: remove it from state without
# destroying the real aws resource, so aws keeps owning its lifecycle.

removed {
  from = aws_iam_service_linked_role.spot

  lifecycle {
    destroy = false
  }
}