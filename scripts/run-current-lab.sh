#!/usr/bin/env bash

set -euo pipefail

repo_root=$(git rev-parse --show-toplevel)
branch=$(git branch --show-current)
result_dir="$repo_root/results/$branch"
result_file="$result_dir/grade.txt"

if [[ -z "$branch" ]]; then
  echo "error: detached HEAD is not supported" >&2
  exit 1
fi

mkdir -p "$result_dir"

{
  echo "date: $(date --iso-8601=seconds)"
  echo "branch: $branch"
  echo "commit-before-test: $(git rev-parse HEAD)"
  echo "status-before-test:"
  git status --short
  echo
  echo "command: make clean && make grade"
} | tee "$result_file"

cd "$repo_root"
make clean 2>&1 | tee -a "$result_file"
make grade 2>&1 | tee -a "$result_file"
