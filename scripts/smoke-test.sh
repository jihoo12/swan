#!/usr/bin/env bash
set -euo pipefail
# Run from the repository root inside nix develop, in a graphical session (Wayland or X11).
# Every render uses the hardware Vulkan driver; a run that lands on a CPU rasterizer fails.
if [[ -z "${WAYLAND_DISPLAY:-}" && -z "${DISPLAY:-}" ]]; then
    echo "smoke-test: windowed runs need a graphical session (WAYLAND_DISPLAY or DISPLAY)" >&2
    exit 1
fi
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
export VK_LAYER_VALIDATE_SYNC=1
export VK_LOADER_LAYERS_DISABLE='~implicit~'
hardware() { # hardware LOG: the renderer reported a GPU that is not a software rasterizer
    grep -Eq '^GPU: .* \((discrete|integrated|virtual)\)$' "$1" || { cat "$1" >&2; echo "FAIL: not rendered on a hardware GPU" >&2; exit 1; }
}
window() { # window ARGS...: a validated windowed run on the desktop
    local log; log=$(mktemp -p "$work")
    timeout 60s "$binary" --validation "$@" 2>&1 | tee "$log"
    hardware "$log"
}
binary=${1:-./build/swan}
cmake -S . -B build -G Ninja
cmake --build build
ctest --test-dir build --output-on-failure
# These commands must work without any window-system display.
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --export-scene "$work/garden.swan.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/garden.swan.json"
window --frames 90 --resize-test
window --overview --frames 45
window --scene "$work/garden.swan.json" --frames 45
window --scene assets/scenes/playground.swan.json --frames 45
mesh_scene=assets/scenes/mesh-garden.swan.json
installed_scene="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/mesh-garden.swan.json"
if [[ -f "$installed_scene" ]]; then mesh_scene="$installed_scene"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$mesh_scene" --export-scene "$work/mesh-garden.swan.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/mesh-garden.swan.json"
window --verify-mesh-uploads --scene "$work/mesh-garden.swan.json" --reload-test --resize-test --frames 90 | tee "$work/mesh-uploads.log"
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
window --scene "$work/textured-garden.swan.json" --reload-test --resize-test --frames 90

hierarchy_scene=assets/scenes/hierarchy-garden.swan.json
installed_hierarchy="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/hierarchy-garden.swan.json"
if [[ -f "$installed_hierarchy" ]]; then hierarchy_scene="$installed_hierarchy"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$hierarchy_scene" --export-scene "$work/hierarchy.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/hierarchy.json"
window --scene "$work/hierarchy.json" --reload-test --resize-test --frames 90
# Compare totals with culling disabled, using the same static camera and frame count.
window --frames 15 >"$work/culled.log" 2>&1
window --no-culling --frames 15 >"$work/full.log" 2>&1
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

window --third-person --frames 90 --resize-test
window --third-person --scene "$work/hierarchy.json" --reload-test --resize-test --frames 90

gltf_scene=assets/scenes/gltf-garden.swan.json
installed_gltf="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/gltf-garden.swan.json"
if [[ -f "$installed_gltf" ]]; then gltf_scene="$installed_gltf"; fi
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --scene "$gltf_scene" --export-scene "$work/gltf.json"
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/gltf.json"
window --verify-mesh-uploads --third-person --scene "$work/gltf.json" --reload-test --resize-test --frames 90
scene_tool="$(dirname "$(readlink -f "$binary")")/swan-scene"
env -u DISPLAY -u WAYLAND_DISPLAY "$scene_tool" assets/scenes/playground.swan.json "$work/edited.json" assets/edits/move-pedestal.json
env -u DISPLAY -u WAYLAND_DISPLAY "$binary" --validate-scene "$work/edited.json"
python3 - "$work/edited.json" <<'PY'
import json
import sys
from pathlib import Path
scene = json.loads(Path(sys.argv[1]).read_text())
entity = next(entity for entity in scene['entities'] if entity['id'] == 'pedestal')
assert all(abs(a-b) < 1e-6 for a, b in zip(entity['transform']['position'], [2, 0.3, 0]))
PY
# Behaviour scripts in windowed play, including reload (which rebuilds the Lua runtime).
scripted_scene=assets/scenes/scripted-garden.swan.json
installed_scripted="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/scripted-garden.swan.json"
if [[ -f "$installed_scripted" ]]; then scripted_scene="$installed_scripted"; fi
window --scene "$scripted_scene" --reload-test --resize-test --frames 90
"$binary" script examples/scripts/garden-bot.lua "$scripted_scene"
# Particle effects and timelines: headless renders (no display), then windowed play.
fx_scene=assets/scenes/fx-showcase.swan.json
installed_fx="$(dirname "$(readlink -f "$binary")")/../share/swan/assets/scenes/fx-showcase.swan.json"
if [[ -f "$installed_fx" ]]; then fx_scene="$installed_fx"; fi
env -u DISPLAY -u WAYLAND_DISPLAY timeout 120s "$binary" render "$fx_scene" --validation --sheet --count 4 --columns 2 -o "$work/fx-sheet.png" 2>"$work/fx-render.log" || { cat "$work/fx-render.log"; exit 1; }
grep -q 'Validation errors: 0' "$work/fx-render.log"
hardware "$work/fx-render.log"
env -u DISPLAY -u WAYLAND_DISPLAY timeout 120s "$binary" render "$fx_scene" --validation --sequence "$work/fx-frames" --from 3.3 --to 3.6 --fps 10 --size 320x180 --stats >"$work/fx-stats.jsonl" 2>"$work/fx-sequence.log" || { cat "$work/fx-sequence.log"; exit 1; }
grep -q 'Validation errors: 0' "$work/fx-sequence.log"
hardware "$work/fx-sequence.log"
env -u DISPLAY -u WAYLAND_DISPLAY SWAN_VALIDATION=1 timeout 120s "$binary" script examples/scripts/make-fx-showcase.lua "$work/fx-generated.swan.json" "$work/fx-script-sheet.png" >"$work/fx-script.log" 2>&1 || { cat "$work/fx-script.log"; exit 1; }
grep -q 'Validation errors: 0' "$work/fx-script.log"
hardware "$work/fx-script.log"
python3 - "$work" <<'PY'
import json
import struct
import sys
from pathlib import Path
work = Path(sys.argv[1])
def size(path):
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', path
    return struct.unpack('>II', data[16:24])
# 2x2 sheet of 320x180 frames with 4-pixel gaps.
assert size(work / 'fx-sheet.png') == (2 * 320 + 3 * 4, 2 * 180 + 3 * 4), size(work / 'fx-sheet.png')
frames = sorted((work / 'fx-frames').glob('frame_*.png'))
assert len(frames) == 4 and all(size(f) == (320, 180) for f in frames), frames
stats = [json.loads(line) for line in (work / 'fx-stats.jsonl').read_text().splitlines()]
assert len(stats) == 4 and any(e['effect'] == 'explosion' for s in stats for e in s['emitters'])
assert size(work / 'fx-script-sheet.png')[0] > 400
print(f'FX renders verified: sheet, {len(frames)} sequence frames, script sheet')
PY
window --scene "$fx_scene" --overview --frames 90 --resize-test --reload-test | tee "$work/fx-game.log"
grep -Eq 'Particles drawn: [1-9]' "$work/fx-game.log"
