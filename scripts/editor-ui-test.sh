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
"$binary" editor assets/scenes/gltf-garden.swan.json --x11 --validation --third-person --save-scene "$work/saved.json" >"$work/swan.log" 2>&1 &
spid=$!
input() { xdotool "$@" >"$work/input.log" 2>&1; sleep .15; }
# Fixed initial panel layout and pinned ImGui version make these positions stable.
for ((i=0;i<100;i++)); do
    if xdotool search --name SWAN >"$work/windows" 2>/dev/null; then break; fi
    kill -0 "$spid" || { cat "$work/swan.log"; exit 1; }
    sleep .05
done
sleep .5
input mousemove 100 254 click 1 # Select pedestal.
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
input mousemove 1040 162 click 1 # Focus selection.
input mousemove 1140 162 click 1 # Duplicate selected entity.
input mousemove 80 129 click 1 # Save duplicate.
python3 - "$work/saved.json" <<'PYDUP'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
original = next(e for e in scene['entities'] if e['id'] == 'pedestal')
copies = [e for e in scene['entities'] if e.get('name') == original.get('name', '') + ' copy']
assert len(copies) == 1, scene['entities']
assert copies[0]['id'] != original['id']
assert copies[0]['transform'] == original['transform']
PYDUP
input mousemove 24 59 click 1 # Undo duplication.
input mousemove 40 82 click 1 # Create cube ahead of camera.
input mousemove 80 129 click 1 # Save new cube.
python3 - "$work/saved.json" <<'PYCUBE'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
cubes = [e for e in scene['entities'] if e.get('name') == 'Cube']
assert len(cubes) == 1
assert sum(v*v for v in cubes[0]['transform']['position']) > 1
PYCUBE
input mousemove 100 254 click 1 # Select pedestal in hierarchy.
input mousemove 640 400 click 1 # Pick the new cube in the viewport.
input mousemove 110 82 click 1 # Delete picked cube.
input mousemove 80 129 click 1 # Save and verify picked identity.
python3 - "$work/saved.json" <<'PYPICK'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
assert not any(e.get('name') == 'Cube' for e in scene['entities'])
assert any(e['id'] == 'pedestal' for e in scene['entities'])
PYPICK
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
print('GUI mesh picking, cube creation, selection, transform, focus, duplication, undo/redo, save and Play/Stop passed; Vulkan errors: 0')
PY

# A fresh editor session needs no scene or save-path arguments.
"$binary" editor --x11 --validation --frames 10 >"$work/new-editor.log" 2>&1
python3 - "$work/new-editor.log" <<'PYNEW'
import sys
from pathlib import Path
text = Path(sys.argv[1]).read_text()
assert 'Scene: 0 render objects' in text, text
assert 'EDITOR | AUTHORING' in text and 'Validation errors: 0' in text, text
print('swan editor opens an empty workspace without scene/save arguments')
PYNEW
