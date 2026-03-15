#!/usr/bin/env bash
set -euo pipefail

./scripts/build.sh
exec "./build/Synthesizer.app/Contents/MacOS/Synthesizer"
