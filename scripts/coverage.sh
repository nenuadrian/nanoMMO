#!/bin/sh
set -eu

# The coverage preset has each test's process write what it ran to a file of
# its own, in build-coverage/profiles.
xvfb-run --auto-servernum ctest --preset coverage
llvm-profdata merge -o build-coverage/tests.profdata build-coverage/profiles/*.profraw

set -- -instr-profile build-coverage/tests.profdata \
  -ignore-filename-regex '_deps|_test\.cc|\.pb\.'
# The programs too, so that code no test reaches counts, at zero.
for binary in build-coverage/bin/nanommo_*; do
  set -- "$@" -object "$binary"
done
llvm-cov show "$@" -format html -output-dir /coverage \
  -show-line-counts-or-regions -project-title nanoMMO
llvm-cov report "$@"
llvm-cov export "$@" -summary-only > /coverage/summary.json
# llvm-cov's folders shut out other users, and in CI the pages upload reads
# them as another user.
chmod -R a+rX /coverage
