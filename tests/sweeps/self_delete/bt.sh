#!/usr/bin/env bash
# bt.sh JOB...: C++ backtrace of one run_pair job under poisoned malloc
HERE=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
REPO=$(cd "$HERE/../../.." && pwd)
cd "$HERE" || exit 1
env GLIBC_TUNABLES=glibc.malloc.tcache_count=0 MALLOC_PERTURB_=165 QT_QPA_PLATFORM=offscreen \
  PYTHONPATH="${SQP_BUILD:-$REPO/build-venv}:." \
  timeout 120 gdb -q -batch -ex "set debuginfod enabled off" -ex "set pagination off" -ex run -ex "bt 25" \
  --args "$REPO/.venv/bin/python" run_pair.py "$@" 2>&1 \
  | grep -E "^#|SIG" | sed -E 's/ \(.{0,400}\) at / at /' | cut -c1-230 | head -30
