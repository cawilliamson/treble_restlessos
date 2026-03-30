variable "region" {
  description = "AWS region for build instances"
  type        = string
  default     = "eu-west-2"
}


variable "project_tag" {
  description = "tag value used to identify build instances"
  type        = string
  default     = "treble-graphiteos"
}
