#!/usr/bin/env bash
# Regenerates mpc/params.json and mpc/presets.json from the engine's parameter
# table and patches/. Run after changing either; CI fails if they are stale.
set -euo pipefail
cd "$(dirname "$0")/.."
OB6RENDER="${OB6RENDER:-build/ob6render}"
"$OB6RENDER" mpc-params > mpc/params.json
# Init first: the first preset is the plugin's default sound.
"$OB6RENDER" mpc-presets patches/init.ob6 patches/brass.ob6 patches/strings.ob6 patches/synclead.ob6 \
    patches/bass.ob6 patches/pluck.ob6 patches/xmodbell.ob6 patches/notchsweep.ob6 > mpc/presets.json
