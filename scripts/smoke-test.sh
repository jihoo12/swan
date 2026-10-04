#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root inside nix develop.
command -v Xvfb >/dev/null
: "${SWAN_SOFTWARE_ICD:?Enter nix develop to select the software Vulkan driver}"
work=$(mktemp -d)
xvfb_pid=
cleanup() {
    if [[ -n "$xvfb_pid" ]]; then kill "$xvfb_pid" 2>/dev/null || true; wait "$xvfb_pid" 2>/dev/null || true; fi
    rm -rf "$work"
}
trap cleanup EXIT
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$work/display" >"$work/xvfb.log" 2>&1 &
xvfb_pid=$!
for ((i=0; i<100; i++)); do
    [[ -s "$work/display" ]] && break
    if ! kill -0 "$xvfb_pid" 2>/dev/null; then cat "$work/xvfb.log"; exit 1; fi
    sleep 0.05
done
if [[ ! -s "$work/display" ]]; then cat "$work/xvfb.log"; exit 1; fi
export DISPLAY=":$(cat "$work/display")"
unset WAYLAND_DISPLAY
export VK_DRIVER_FILES="$SWAN_SOFTWARE_ICD"
export VK_LAYER_VALIDATE_SYNC=1
export VK_LOADER_LAYERS_DISABLE='~implicit~'
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
binary=${1:-./build/swan}
# These commands must work without any window-system display.
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --export-scene "$work/garden.swan.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/garden.swan.json"
timeout 60s "$binary" --x11 --validation --frames 90 --resize-test
timeout 60s "$binary" --x11 --validation --overview --frames 45
timeout 60s "$binary" --x11 --validation --scene "$work/garden.swan.json" --frames 45
timeout 60s "$binary" --x11 --validation --scene assets/scenes/playground.swan.json --frames 45
mesh_scene=assets/scenes/mesh-garden.swan.json
installed_scene="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/mesh-garden.swan.json"
if [[ -f "$installed_scene" ]]; then mesh_scene="$installed_scene"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$mesh_scene" --export-scene "$work/mesh-garden.swan.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/mesh-garden.swan.json"
timeout 60s "$binary" --x11 --validation --verify-mesh-uploads --scene "$work/mesh-garden.swan.json" --reload-test --resize-test --frames 90 | tee "$work/mesh-uploads.log"
python3 - "$work/mesh-uploads.log" <<'PYTEST'
import re
import sys
from pathlib import Path
text = Path(sys.argv[1]).read_text()
meshes = re.search(r'Mesh uploads: (\d+); resident meshes: (\d+)', text)
storage = re.search(r'Mesh storage: device-local; uploaded bytes=(\d+); resident bytes=(\d+); verified uploads=(\d+)', text)
if not meshes or not storage:
    raise RuntimeError('Missing mesh upload diagnostics')
uploads, resident = map(int, meshes.groups())
uploaded_bytes, resident_bytes, verified = map(int, storage.groups())
assert uploads == verified == 4 and resident == 2
assert 0 < resident_bytes < uploaded_bytes
assert 'Validation errors: 0' in text
print(f'Mesh staging verified: {verified} uploads, {resident} resident buffers')
PYTEST
texture_scene=assets/scenes/textured-garden.swan.json
installed_texture_scene="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/textured-garden.swan.json"
if [[ -f "$installed_texture_scene" ]]; then texture_scene="$installed_texture_scene"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$texture_scene" --export-scene "$work/textured-garden.swan.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/textured-garden.swan.json"
timeout 60s "$binary" --x11 --validation --scene "$work/textured-garden.swan.json" --reload-test --resize-test --frames 90

hierarchy_scene=assets/scenes/hierarchy-garden.swan.json
installed_hierarchy="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/hierarchy-garden.swan.json"
if [[ -f "$installed_hierarchy" ]]; then hierarchy_scene="$installed_hierarchy"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$hierarchy_scene" --export-scene "$work/hierarchy.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/hierarchy.json"
timeout 60s "$binary" --x11 --validation --scene "$work/hierarchy.json" --reload-test --resize-test --frames 90
# Compare totals with culling disabled, using the same static camera and frame count.
timeout 60s "$binary" --x11 --validation --frames 15 >"$work/culled.log" 2>&1
timeout 60s "$binary" --x11 --validation --no-culling --frames 15 >"$work/full.log" 2>&1
python3 - "$work/culled.log" "$work/full.log" <<'PY'
import re
import sys
from pathlib import Path
def counts(path):
    text = Path(path).read_text()
    match = re.search(r'Draw statistics: frames=(\d+) submitted=(\d+) culled=(\d+)', text)
    if not match or 'Validation errors: 0' not in text:
        raise RuntimeError(f'Missing draw statistics or validation failed: {text}')
    return tuple(map(int, match.groups()))
frames, submitted, culled = counts(sys.argv[1])
full_frames, full_submitted, full_culled = counts(sys.argv[2])
assert frames == full_frames and frames > 0
assert full_culled == 0 and culled > 0
assert submitted + culled == full_submitted and submitted < full_submitted
print(f'Culling comparison: {submitted}/{full_submitted} draw calls submitted over {frames} frames')
PY

timeout 60s "$binary" --x11 --validation --third-person --frames 90 --resize-test
timeout 60s "$binary" --x11 --validation --third-person --scene "$work/hierarchy.json" --reload-test --resize-test --frames 90

gltf_scene=assets/scenes/gltf-garden.swan.json
installed_gltf="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/gltf-garden.swan.json"
if [[ -f "$installed_gltf" ]]; then gltf_scene="$installed_gltf"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$gltf_scene" --export-scene "$work/gltf.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/gltf.json"
timeout 60s "$binary" --x11 --validation --verify-mesh-uploads --third-person --scene "$work/gltf.json" --reload-test --resize-test --frames 90
editor_binary="$(dirname "$(readlink -f "$binary")")/swan-scene"
env -u DISPLAY -u WAYLAND_DISPLAY "$editor_binary" assets/scenes/playground.swan.json "$work/edited.json" assets/edits/move-pedestal.json
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/edited.json"
python3 - "$work/edited.json" <<'PY'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
entity = next(entity for entity in scene['entities'] if entity['id'] == 'pedestal')
assert all(abs(a-b) < 1e-6 for a, b in zip(entity['transform']['position'], [2, 0.3, 0]))
PY
timeout 60s "$binary" --x11 --validation --editor --scene assets/scenes/gltf-garden.swan.json --frames 90 --resize-test
