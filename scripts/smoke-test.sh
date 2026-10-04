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
timeout 60s "$binary" --x11 --validation --scene "$work/mesh-garden.swan.json" --reload-test --resize-test --frames 90
