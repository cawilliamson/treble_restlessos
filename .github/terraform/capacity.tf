data "external" "spot_score" {
  program = ["${path.module}/spot-score.sh"]

  query = {
    region        = var.region
    instance_type = var.large_instance_type
  }
}

output "subnet_id" {
  description = "subnet in the AZ with the best spot placement score"
  value       = endswith(data.external.spot_score.result.az, "a") ? aws_subnet.build_a.id : endswith(data.external.spot_score.result.az, "b") ? aws_subnet.build_b.id : aws_subnet.build_c.id
}
