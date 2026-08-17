#!/usr/bin/env bash

set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
lab=${1:-mmap}
mode=${2:-demo}
scratch=

case "$lab" in
  util)    commands=$'sleep 10\npingpong\nprimes\nfind . README\necho hello | xargs echo xv6' ;;
  syscall) commands=$'trace 32 grep hello README\nsysinfotest' ;;
  pgtbl)   commands=$'pgtbltest' ;;
  traps)   commands=$'call\nalarmtest' ;;
  cow)     commands=$'cowtest' ;;
  thread)  commands=$'uthread' ;;
  net)     commands=$'nettests' ;;
  lock)    commands=$'kalloctest\nbcachetest' ;;
  fs)      commands=$'symlinktest' ;;
  mmap)    commands=$'mmaptest' ;;
  *) echo "usage: $0 {util|syscall|pgtbl|traps|cow|thread|net|lock|fs|mmap} [demo|grade]" >&2; exit 2 ;;
esac

if [[ $mode != demo && $mode != grade ]]; then
  echo "error: mode must be demo or grade" >&2
  exit 2
fi
if [[ -n $(git status --porcelain) ]]; then
  echo "error: worktree is not clean; refusing to create a demo snapshot" >&2
  exit 1
fi

cleanup() {
  if [[ -n ${scratch:-} && $scratch == /tmp/xv6-defense.* && -d $scratch ]]; then
    rm -rf -- "$scratch"
  fi
}
scratch=$(mktemp -d /tmp/xv6-defense.XXXXXX)
trap cleanup EXIT

git clone --quiet --local --no-hardlinks "$repo_root" "$scratch/repo"
git -C "$scratch/repo" switch --quiet --detach "origin/$lab"
cd "$scratch/repo"

echo "branch snapshot: $lab"
echo "commit: $(git rev-parse HEAD)"
echo "cold build: make clean && make -j4"
make clean
make -j4

if [[ $mode == grade ]]; then
  "./grade-lab-$lab" -v
  exit 0
fi

echo
echo "After the xv6 shell prompt appears, run:"
printf '  %s\n' "$commands"
echo "Exit QEMU with Ctrl-a x; the main worktree remains unchanged."
make qemu
