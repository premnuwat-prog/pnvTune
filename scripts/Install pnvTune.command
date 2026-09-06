#!/bin/bash
set -euo pipefail
cd "$(dirname "$0")"
if [[ ! -d pnvTune.component || ! -d pnvTune.vst3 ]]; then
  echo "Keep this installer beside pnvTune.component and pnvTune.vst3."
  exit 1
fi
plugin_root="$HOME/Library/Audio/Plug-Ins"
mkdir -p "$plugin_root/Components" "$plugin_root/VST3"
ditto pnvTune.component "$plugin_root/Components/PremTune.component"
ditto pnvTune.vst3 "$plugin_root/VST3/PremTune.vst3"
echo "pnvTune installed or updated. Reopen Logic Pro and select Audio Units > PNV Audio > pnvTune."
