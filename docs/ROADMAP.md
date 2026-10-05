# Swan feature roadmap

The next milestones focus on usable features rather than small renderer optimizations. Authoring is API-first: the Lua automation API, the headless C++ SDK, and `swan render` captures replace a GUI editor.

1. **Third-person play (0.11):** reusable orbit/follow camera rig, conservative camera collision, camera-relative controls, and a temporary avatar proxy. Keep the controller and presentation camera separate.
2. **Static glTF import (0.12 geometry path delivered):** Assimp now converts glTF/GLB node primitives to engine-owned geometry with baked node matrices. Next add imported materials/textures and editable runtime node transforms. Keep parsing behind the importer boundary and convert imported materials/textures into engine-owned assets. The first static path bakes general node matrices; establish full matrix/quaternion runtime transforms before exposing those nodes for editing or animation. Keep OBJ and old scene files readable through explicit schema migration. Start with a small documented subset and real sample assets; report unsupported features.
3. **GUI editor (removed in 0.18):** The docking Dear ImGui editor (0.14–0.17) was removed: its tests needed Xvfb and a CPU Vulkan driver, and authoring is better served by scriptable APIs. `EditorDocument` (previews, atomic batches, labeled history, saved-revision tracking) remains the authoring core behind Lua, `swan-scene`, and the SDK. Next: grow those APIs (picking/bounds queries, material and texture creation, copy/paste, prefab-like reuse) instead of GUI panels.
4. **Scripting and headless SDK (0.16 delivered):** Lua 5.4 through the C API with a sandbox, instruction budget, and memory cap; per-entity behaviours (scene version 6) running in game and headless simulation; `swan script` automation over undoable document commands; LuaLS type definitions; and an installable `swan::headless` CMake package with an out-of-tree example. Next: script hot reload during Play, more events (trigger volumes, timers, input actions), and physics queries for scripts.
5. **Animation:** scene timelines (0.17) already keyframe entity transforms, attached-effect emission, material color/emission, environment exposure/bloom/background, a shot camera, and timed effect events, with linear/step/smooth/in/out easing. Next: reusable animation clip assets, node channels, playback, and blending. Add skinning with joint/weight vertex data, skeleton poses, and GPU joint buffers after the static importer and transform contracts are proven. Do not stretch the garden's procedural bob/spin component into a skeletal animation system.
6. **FX (0.17 delivered for headless authoring):** deterministic CPU emitters (point/sphere/box/ring/disc shapes, rate curves, bursts, looping windows, gravity, drag, turbulence, orbit, local space) with size/color-over-life curves; a billboard pass with procedural sprites, velocity stretching, additive and sorted alpha blending; HDR rendering with bloom and Reinhard/ACES tone mapping; scene environments; and headless capture (`swan render`, `Preview:render_sheet`) with labeled contact sheets and JSON statistics for AI- and CI-driven iteration. Next: texture flipbooks, soft-particle depth fade, trails/ribbons, sub-emitters, and GPU simulation only for demonstrated scale needs.

## Boundaries to preserve

- Importers produce CPU assets; source-format structs never reach the renderer.
- Authored scene data stays separate from runtime state and document selection/history.
- Physics uses current simulation transforms; rendering uses immutable presentation snapshots.
- Camera rigs consume targets and collision queries, independently of a model's skeleton.
- Asset identity/lifetime and edit commands form shared contracts for importer, runtime, and authoring APIs.
- Vulkan ownership stays in the renderer; upload, rendering, and retirement are distinct responsibilities.

Each milestone should deliver a runnable sample, focused CPU tests, a Vulkan smoke test, and documented limits. Full-matrix transforms and skinning will require deliberate shader/scene changes; anticipate these contracts, but do not build a speculative universal framework first.
