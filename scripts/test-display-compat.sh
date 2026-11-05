#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
cc -std=c11 -O2 -Wall -Wextra -Werror tests/display-compat-regression.c -o /tmp/mustardtube-display-compat-test
/tmp/mustardtube-display-compat-test
