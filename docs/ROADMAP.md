# Swan feature roadmap

The next milestones focus on usable features rather than small renderer optimizations. The GUI editor does not have to wait for a complete engine.

1. **Third-person play (0.11):** reusable orbit/follow camera rig, conservative camera collision, camera-relative controls, and a temporary avatar proxy. Keep the controller and presentation camera separate.
2. **Static glTF import (0.12 geometry path delivered):** Assimp now converts glTF/GLB node primitives to engine-owned geometry with baked node matrices. Next add imported materials/textures and editable runtime node transforms. Keep parsing behind the importer boundary and convert imported materials/textures into engine-owned assets. The first static path bakes general node matrices; establish full matrix/quaternion runtime transforms before exposing those nodes for editing or animation. Keep OBJ and old scene files readable through explicit schema migration. Start with a small documented subset and real sample assets; report unsupported features.
3. **Minimal GUI editor (0.14 initial UI delivered):** The authored document, transactional edit commands, undo/redo, selection, play-copy isolation, and headless editing frontend now exist. The first Dear ImGui hierarchy/transform inspector, input capture, save/reload, and Play/Stop now use that layer. Next add material editing, viewport selection, and gizmos. scene hierarchy, selection, transform/material inspector, play/stop, save/reload. Add scene edit commands and undo/redo first, plus explicit input capture. Edit the authored scene, and create a separate runtime scene for play. The editor uses scene APIs and render snapshots; it must not manipulate Vulkan resources directly. Embed a small GUI library rather than inventing widgets. Docking and gizmos can follow this usable first version.
4. **Animation:** animation clip assets, node channels, time sampling, playback, and blending. Add skinning with joint/weight vertex data, skeleton poses, and GPU joint buffers after the static importer and transform contracts are proven. Do not stretch the garden's procedural bob/spin component into a skeletal animation system.
5. **FX:** begin with a CPU particle emitter and simple billboard rendering, then add lifetime/color/size curves and texture animation. Put simulation in the runtime and rendering in a dedicated pass. Transparency, depth ordering, and blend state need explicit treatment. Leave GPU particle simulation for demonstrated scale needs.

## Boundaries to preserve

- Importers produce CPU assets; source-format structs never reach the renderer.
- Authored scene data stays separate from runtime state and editor selection/history.
- Physics uses current simulation transforms; rendering uses immutable presentation snapshots.
- Camera rigs consume targets and collision queries, independently of a model's skeleton.
- Asset identity/lifetime and edit commands form shared contracts for importer, runtime, and editor.
- Vulkan ownership stays in the renderer; upload, rendering, and retirement are distinct responsibilities.

Each milestone should deliver a runnable sample, focused CPU tests, a Vulkan smoke test, and documented limits. Full-matrix transforms and skinning will require deliberate shader/scene changes; anticipate these contracts, but do not build a speculative universal framework first.
