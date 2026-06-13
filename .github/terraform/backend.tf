terraform {
  backend "s3" {
    bucket       = "gsi-build-tfstate"
    key          = "terraform.tfstate"
    region       = "eu-west-2"
    encrypt      = true
    use_lockfile = true
  }
}
