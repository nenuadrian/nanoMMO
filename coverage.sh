#!/bin/sh
set -eu

# Each test's process writes what it ran to a file of its own.
export LLVM_PROFILE_FILE=/tmp/coverage/%p.profraw
xvfb-run --auto-servernum ctest --test-dir build --output-on-failure
llvm-profdata merge -o /tmp/coverage/tests.profdata /tmp/coverage/*.profraw

set -- -instr-profile /tmp/coverage/tests.profdata \
  -ignore-filename-regex '_deps|_test\.cc|\.pb\.'
# The programs too, so that code no test reaches counts, at zero.
for binary in build/bin/nanommo_*; do
  set -- "$@" -object "$binary"
done
llvm-cov show "$@" -format html -output-dir /coverage \
  -show-line-counts-or-regions -project-title nanoMMO
llvm-cov report "$@"
llvm-cov export "$@" -summary-only > /coverage/summary.json
# llvm-cov's folders shut out other users, and in CI the pages upload reads
# them as another user.
chmod -R a+rX /coverage
