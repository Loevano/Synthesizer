#!/usr/bin/env bash
set -euo pipefail

if [[ "$(uname -s)" == "Darwin" ]]; then
  DEFAULT_MODULE="build/modules/liblowpass_module.dylib"
else
  DEFAULT_MODULE="build/modules/liblowpass_module.so"
fi

MODULE_PATH="${1:-${DEFAULT_MODULE}}"

./scripts/build.sh
./build/synth_host --module "${MODULE_PATH}"
