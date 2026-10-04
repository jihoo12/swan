#!/usr/bin/env bash
set -euo pipefail
# Real X11 mouse/keyboard test of the editor. Run from the repository root inside nix develop.
# Widgets are located through the SWAN_EDITOR_PROBE file, so layout changes do not break the test.
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
fail() { echo "FAIL: $*" >&2; echo "--- editor log" >&2; tail -40 "$work/swan.log" >&2 || true; exit 1; }
Xvfb -displayfd 3 -screen 0 1280x800x24 -nolisten tcp 3>"$work/display" >"$work/xvfb.log" 2>&1 &
xpid=$!
for ((i=0;i<100;i++)); do [[ -s "$work/display" ]] && break; sleep .05; done
export DISPLAY=":$(cat "$work/display")"
unset WAYLAND_DISPLAY
export VK_DRIVER_FILES="$SWAN_SOFTWARE_ICD" VK_LAYER_VALIDATE_SYNC=1 VK_LOADER_LAYERS_DISABLE='~implicit~'
# Fresh per-run preferences and layout; the probe publishes widget rectangles and state.
export SWAN_CONFIG_HOME="$work/config" SWAN_EDITOR_PROBE="$work/probe.json"
saved="$work/saved.json"
probe() { python3 - "$work/probe.json" "$@" <<'PY'
import json, sys
data = json.load(open(sys.argv[1]))
kind, key = sys.argv[2], sys.argv[3]
if kind == 'center':
    r = data['rects'].get(key)
    if r is None:
        sys.exit(f"missing widget {key!r}; known: {sorted(data['rects'])[:40]}")
    print(int((r[0] + r[2]) / 2), int((r[1] + r[3]) / 2))
elif kind == 'value':
    print(data['values'].get(key, ''))
elif kind == 'has':
    sys.exit(0 if key in data['rects'] else 1)
PY
}
wait_for() { # wait_for VALUE-NAME EXPECTED
    for ((i=0;i<100;i++)); do [[ "$(probe value "$1" 2>/dev/null)" == "$2" ]] && return; sleep .05; done
    fail "$1 is '$(probe value "$1")', expected '$2'"
}
wait_contains() { # wait_contains VALUE-NAME TEXT
    for ((i=0;i<100;i++)); do [[ "$(probe value "$1" 2>/dev/null)" == *"$2"* ]] && return; sleep .05; done
    fail "$1 does not contain '$2': $(probe value "$1")"
}
wait_widget() { for ((i=0;i<100;i++)); do probe has "$1" 2>/dev/null && return; sleep .05; done; fail "widget $1 never appeared"; }
input() { xdotool "$@" >>"$work/input.log" 2>&1; sleep .2; }
# Press and release like a person: an instantaneous X11 click can land within one GUI frame.
press() { input mousemove "$1" "$2"; xdotool mousedown 1; sleep .08; xdotool mouseup 1; sleep .2; }
click() { wait_widget "$1"; local xy; xy=$(probe center "$1") || fail "$xy"; press $xy; }
drag() { # drag FROM-X FROM-Y TO-X TO-Y, in small steps so ImGui sees the motion
    input mousemove "$1" "$2"; sleep .2; xdotool mousedown 1; sleep .15
    for step in 1 2 3 4 5 6 7 8; do xdotool mousemove $(( $1 + ($3-$1)*step/8 )) $(( $2 + ($4-$2)*step/8 )); sleep .05; done
    sleep .15; xdotool mouseup 1; sleep .3
}
save() { rm -f "$saved"; input key ctrl+s; for ((i=0;i<60;i++)); do [[ -s "$saved" ]] && return; sleep .05; done; fail "Ctrl+S did not write $saved"; }
check() { python3 - "$saved" "$1" <<'PY' || fail "saved scene check: $1"
import json, sys
scene = json.load(open(sys.argv[1]))
entities = {e['id']: e for e in scene['entities']}
assert eval(sys.argv[2], {'scene': scene, 'e': entities}), sys.argv[2]
PY
}
palette() { input key ctrl+k; wait_widget palette/input; input type --delay 30 "$1"; input key Return; }

"$binary" editor assets/scenes/gltf-garden.swan.json --x11 --validation --third-person --save-scene "$saved" >"$work/swan.log" 2>&1 &
spid=$!
for ((i=0;i<200;i++)); do [[ -s "$work/probe.json" ]] && break; kill -0 "$spid" 2>/dev/null || fail "editor exited"; sleep .05; done
wait_widget hierarchy/pedestal
sleep .5

# Hierarchy selection and a typed inspector value (click on a drag field enters text mode).
click hierarchy/pedestal
wait_for selection pedestal
click inspector/position/x
input key ctrl+a
input type --clearmodifiers 2
input key Return
wait_for undo "Move Crystal pedestal"
save
check "abs(e['pedestal']['transform']['position'][0] - 2) < 1e-6"
wait_for modified false
# Keyboard undo/redo.
input key ctrl+z
save
check "abs(e['pedestal']['transform']['position'][0]) < 1e-6"
input key ctrl+y
save
check "abs(e['pedestal']['transform']['position'][0] - 2) < 1e-6"
echo "inspector edit, Ctrl+S, Ctrl+Z, Ctrl+Y: ok"

# Gizmo: drag the move handle's screen-space center; one drag is one undo step.
wait_widget viewport/selection
read -r gx gy < <(probe center viewport/selection)
drag "$gx" "$gy" $((gx+90)) $((gy+30))
wait_for undo "Move Crystal pedestal"
save
check "abs(e['pedestal']['transform']['position'][0] - 2) > 0.05"
input key ctrl+z
save
check "abs(e['pedestal']['transform']['position'][0] - 2) < 1e-6"
echo "gizmo drag and single-step undo: ok"

# Duplicate (Ctrl+D) keeps the transform and creates a new stable ID.
input key ctrl+d
save
check "len([x for x in scene['entities'] if x['name'] == 'Crystal pedestal copy']) == 1"
check "[x for x in scene['entities'] if x['name'] == 'Crystal pedestal copy'][0]['transform'] == e['pedestal']['transform']"
input key ctrl+z
echo "duplicate: ok"

# Command palette creates a cube ahead of the camera; Esc deselects; a viewport click picks it.
palette "add cube"
save
check "len([x for x in scene['entities'] if x['name'] == 'Cube']) == 1"
input key Escape
wait_for selection ""
read -r vx vy < <(probe center viewport)
press "$vx" "$vy"
for ((i=0;i<60;i++)); do sel=$(probe value selection); [[ -n "$sel" && "$sel" != pedestal ]] && break; sleep .05; done
[[ -n "$sel" && "$sel" != pedestal ]] || fail "viewport click did not pick the cube (selection '$sel')"
input key Delete
save
check "not any(x['name'] == 'Cube' for x in scene['entities']) and 'pedestal' in e"
echo "palette, deselect, viewport picking, delete: ok"

# Drag a material from Assets onto a hierarchy row.
click assets/tab/Materials
wait_widget assets/material/gold
read -r mx my < <(probe center assets/material/gold)
read -r hx hy < <(probe center hierarchy/pedestal)
drag "$mx" "$my" "$hx" "$hy"
save
check "e['pedestal']['material'] == 'gold'"
input key ctrl+z
save
check "e['pedestal']['material'] == 'stone'"
echo "material drag and drop: ok"

# Play / Stop.
input key F5
wait_for mode play
input key F5
wait_for mode authoring
echo "play/stop: ok"

# Quitting with unsaved changes asks first; Cancel keeps the editor open, Don't Save quits.
palette "add cube"
wait_for modified true
input key ctrl+q
click modal/cancel
sleep .3; kill -0 "$spid" 2>/dev/null || fail "Cancel did not keep the editor open"
input key ctrl+q
click modal/discard
for ((i=0;i<100;i++)); do kill -0 "$spid" 2>/dev/null || break; sleep .05; done
kill -0 "$spid" 2>/dev/null && fail "Don't Save did not quit"
wait "$spid" || fail "editor exited with an error"
spid=
check "not any(x['name'] == 'Cube' for x in scene['entities'])"
python3 - "$work/swan.log" "$work/config" "$saved" <<'PY'
import json, sys
from pathlib import Path
text = Path(sys.argv[1]).read_text()
assert 'EDITOR | PLAY' in text and text.count('EDITOR | AUTHORING') >= 2, text
assert 'Validation errors: 0' in text, text
config = Path(sys.argv[2])
settings = json.loads((config / 'editor.json').read_text())
assert str(Path(sys.argv[3]).resolve()) in [str(Path(p).resolve()) for p in settings['recent_scenes']], settings
assert (config / 'imgui.ini').exists(), 'dock layout was not persisted'
print('unsaved-changes guard, settings and layout persistence: ok; Vulkan validation errors: 0')
PY

# Regression: creating from a mesh menu after another creation must not crash.
"$binary" editor --x11 --validation --save-scene "$saved" >"$work/swan.log" 2>&1 &
spid=$!
# Same sequence as the bug report: the empty-scene Add Cube button, then + > Cube twice.
click viewport/add-cube
click hierarchy/add
click hierarchy/add/builtin:cube
click hierarchy/add
click hierarchy/add/builtin:cube
sleep .3; kill -0 "$spid" 2>/dev/null || fail "editor crashed creating from the hierarchy add menu"
save
check "len([x for x in scene['entities'] if x['name'] == 'Cube']) == 3"
input key ctrl+q
wait "$spid" || fail "editor exited with an error"
spid=
echo "repeated creation from the hierarchy add menu: ok"

# Scripting: a Lua console line is one undo step; Play runs the scene's behaviour scripts.
"$binary" editor assets/scenes/scripted-garden.swan.json --x11 --validation --save-scene "$saved" >"$work/swan.log" 2>&1 &
spid=$!
wait_widget hierarchy/pedestal
input key ctrl+grave
wait_widget console/input
input type --delay 20 'doc:create{name = "Lua cube", position = vec3(0, 3, 0), script = "spinner", properties = {speed = 3}}'
input key Return
wait_contains undo "Console: doc:create"
save
check "[x.get('script') for x in scene['entities'] if x['name'] == 'Lua cube'] == ['spinner']"
check "[x['properties'] for x in scene['entities'] if x['name'] == 'Lua cube'] == [{'speed': 3}]"
input key ctrl+z
save
check "not any(x['name'] == 'Lua cube' for x in scene['entities']) and len(scene['scripts']) == 4"
input key F5
wait_for mode play
wait_contains log "Running 6 behaviour script(s)"
input key F5
wait_for mode authoring
input key ctrl+q
wait "$spid" || fail "editor exited with an error"
spid=
grep -q 'Validation errors: 0' "$work/swan.log" || fail "Vulkan validation errors in the scripting session"
echo "Lua console transaction, script persistence, scripted Play: ok"

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
