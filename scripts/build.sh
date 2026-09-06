#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")/.."
cmake_bin="${CMAKE_BIN:-.build-env/bin/cmake}"
if [[ ! -x "$cmake_bin" ]]; then cmake_bin="cmake"; fi
"$cmake_bin" -S . -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_OSX_ARCHITECTURES=arm64
"$cmake_bin" --build build -j 6
build/PremTuneDSPTests
build/PremTuneChecks_artefacts/Release/PremTuneChecks docs/pnvTune.png
codesign --force --sign - build/pnvTune_artefacts/Release/AU/pnvTune.component
codesign --force --sign - build/pnvTune_artefacts/Release/VST3/pnvTune.vst3
