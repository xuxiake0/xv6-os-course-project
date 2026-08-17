#!/usr/bin/env bash

set -eu

echo "== Operating system =="
uname -a
if [[ -r /etc/os-release ]]; then
  cat /etc/os-release
fi

echo "== Toolchain =="
git --version
make --version | head -n 1
qemu-system-riscv64 --version | head -n 1
riscv64-linux-gnu-gcc --version | head -n 1
riscv64-linux-gnu-ld --version | head -n 1
gdb-multiarch --version | head -n 1

echo "== Repository =="
git status --short --branch
git remote -v
git rev-parse HEAD
