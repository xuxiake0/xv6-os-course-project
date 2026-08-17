#!/usr/bin/env bash

set -euo pipefail

labs=(util syscall pgtbl traps cow thread net lock fs mmap)
repo_root=$(git rev-parse --show-toplevel)
stamp=$(date +%Y%m%d-%H%M%S)
result_root=${RESULT_ROOT:-"$(dirname "$repo_root")/xv6-all-test-results/$stamp"}
scratch=$(mktemp -d /tmp/xv6-all-tests.XXXXXX)

cleanup() {
  if [[ -n ${scratch:-} && $scratch == /tmp/xv6-all-tests.* && -d $scratch ]]; then
    rm -rf -- "$scratch"
  fi
}
trap cleanup EXIT

if [[ -n $(git -C "$repo_root" status --porcelain) ]]; then
  echo "error: the main worktree is not clean; refusing to start" >&2
  exit 1
fi

mkdir -p "$result_root"
git clone --quiet --local --no-hardlinks "$repo_root" "$scratch/repo"

{
  echo "date: $(date --iso-8601=seconds)"
  echo "source: $repo_root"
  echo "output: $result_root"
  echo "labs: ${labs[*]}"
} | tee "$result_root/summary.txt"

for lab in "${labs[@]}"; do
  echo "== $lab ==" | tee -a "$result_root/summary.txt"
  git -C "$scratch/repo" switch --quiet --detach "origin/$lab"
  (
    cd "$scratch/repo"
    echo "branch: $lab"
    echo "commit: $(git rev-parse HEAD)"
    make clean
    "./grade-lab-$lab" -v
  ) 2>&1 | tee "$result_root/$lab.txt"
  tail -n 1 "$result_root/$lab.txt" | tee -a "$result_root/summary.txt"
done

echo "all lab graders completed; logs: $result_root" | tee -a "$result_root/summary.txt"

