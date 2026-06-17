data "external" "spot_score" {
  program = ["bash", "${path.module}/scripts/spot-score.sh"]

  query = {
    region        = var.region
    instance_type = var.large_instance_type
  }
}
