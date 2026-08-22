#!/usr/bin/env bash
# Set up the MIT 6.S081 / xv6 2021 toolchain in Ubuntu WSL.
set -euo pipefail

export DEBIAN_FRONTEND=noninteractive

apt-get update
apt-get install -y \
  binutils-riscv64-linux-gnu \
  build-essential \
  gcc-riscv64-linux-gnu \
  gdb-multiarch \
  git \
  perl \
  python3 \
  qemu-system-misc

printf 'xv6 WSL toolchain is ready.\n'
