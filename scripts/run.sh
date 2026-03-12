#!/usr/bin/env bash
set -euo pipefail

./scripts/build.sh
./build/synth_host "$@"
