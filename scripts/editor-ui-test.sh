#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root inside nix develop.
: "${SWAN_SOFTWARE_ICD:?Enter nix develop first}"
binary=${1:-./build/swan}
work=$(mktemp -d)
xpid= spid=
cleanup() {
    [[ -z "$spid" ]] || kill "$spid" 2>/dev/null || true
    [[ -z "$xpid" ]] || kill "$xpid" 2>/dev/null || true
    rm -rf "$work"
}
trap cleanup EXIT
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$work/display" >"$work/xvfb.log" 2>&1 &
xpid=$!
for ((i=0;i<100;i++)); do [[ -s "$work/display" ]] && break; sleep .05; done
export DISPLAY=":$(cat "$work/display")"
unset WAYLAND_DISPLAY
export VK_DRIVER_FILES="$SWAN_SOFTWARE_ICD" VK_LAYER_VALIDATE_SYNC=1 VK_LOADER_LAYERS_DISABLE='~implicit~'
"$binary" --x11 --validation --editor --third-person --scene assets/scenes/gltf-garden.swan.json --save-scene "$work/saved.json" >"$work/swan.log" 2>&1 &
spid=$!
input() { xdotool "$@" >"$work/input.log" 2>&1; sleep .15; }
# Fixed initial panel layout and pinned ImGui version make these positions stable.
for ((i=0;i<100;i++)); do
    if xdotool search --name SWAN >"$work/windows" 2>/dev/null; then break; fi
    kill -0 "$spid" || { cat "$work/swan.log"; exit 1; }
    sleep .05
done
sleep .5
input mousemove 100 231 click 1 # Select pedestal.
input mousemove 1010 71 click 1 # Local position X.
input key ctrl+a
input type --clearmodifiers 2
input mousemove 1040 139 click 1 # Apply.
input mousemove 24 59 click 1 # Undo.
input mousemove 71 59 click 1 # Redo.
input mousemove 80 129 click 1 # Save authored scene.
python3 - "$work/saved.json" <<'PY'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
pedestal = next(e for e in scene['entities'] if e['id'] == 'pedestal')
assert abs(pedestal['transform']['position'][0] - 2) < 1e-6
PY
input mousemove 23 37 click 1 # Play.
input mousemove 23 37 click 1 # Stop.
input key Escape
wait "$spid"
spid=
python3 - "$work/swan.log" <<'PY'
import sys
from pathlib import Path
text = Path(sys.argv[1]).read_text()
assert 'EDITOR | PLAY' in text and text.count('EDITOR | AUTHORING') >= 2, text
assert 'Validation errors: 0' in text, text
print('GUI selection, transform, undo/redo, save and Play/Stop passed; Vulkan errors: 0')
PY
