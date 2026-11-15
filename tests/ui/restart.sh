#!/bin/sh
# Separate processes ensure workbook settings survive an actual application exit.
set -eu
scratch=$(mktemp -d "${TMPDIR:-/tmp}/isaac-restart.XXXXXXXX")
trap 'rm -rf -- "$scratch"' EXIT HUP INT TERM
export ISAAC_TEST_RESTART_PATH="$scratch/settings.gnumeric"
ISAAC_TEST_RESTART_WRITE=1 "$1"
"$1"
