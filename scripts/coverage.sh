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
# llvm-cov warns of thousands of functions with mismatched data. Each is an
# inline function a file defines and doesn't use, which Clang records with a
# hash of 0, and llvm-cov counts it from a program that uses it. The libraries
# we fetch are made before CMakeLists.txt turns the counters on, so none of
# their functions, such as GoogleTest's main(), is counted in place of ours.

# A report from before goes, but for its history, which the trend goes on from.
find /coverage -mindepth 1 -maxdepth 1 ! -name history.json -exec rm -rf {} +
llvm-cov show "$@" -format html -output-dir /coverage/files \
  -show-line-counts-or-regions -project-title nanoMMO
llvm-cov report "$@"
llvm-cov export "$@" -summary-only > /coverage/summary.json
# The summary page, the trend and the badge, and llvm-cov's pages in the API
# docs' look.
python3 scripts/coverage/report.py /coverage \
  "$(llvm-cov --version | grep -o 'LLVM version [0-9.]*')"
# llvm-cov's folders shut out other users, and in CI the pages upload reads
# them as another user.
chmod -R a+rX /coverage
