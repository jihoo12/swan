# Repository Guidelines

## Project Structure & Module Organization

Swan is a C++20 Vulkan 1.3 engine with a garden demo, Lua automation, and a headless C++ SDK. There is no GUI editor; authoring goes through `EditorDocument` (Lua, `swan-scene`, C++).

- `src/`: core scene/assets/physics, Vulkan renderer, runtime loop, demo gameplay, the scene document, and Lua scripting. CMake separates these into `swan_core`, `swan_script`, `swan_renderer`, `swan_runtime`, `swan_demo`, and `swan_headless`.
- `shaders/`: GLSL sources compiled to SPIR-V during the build.
- `tests/`: standalone C++ regression executables registered with CTest.
- `assets/`: versioned scene JSON, models, textures, and example edit commands.
- `scripts/`: the hardware-GPU rendering smoke test.
- `docs/`: roadmap and screenshots. `build/` and `result` are generated outputs.

## Build, Test, and Development Commands

Run from the repository root:

```sh
nix develop path:.                       # Enter the pinned development environment
cmake -S . -B build -G Ninja              # Configure
cmake --build build                      # Build applications, shaders, and tests
ctest --test-dir build --output-on-failure
./build/swan --scene assets/scenes/gltf-garden.swan.json         # Play a scene in a window
./build/swan script examples/scripts/garden-bot.lua            # Headless Lua automation
./build/swan render assets/scenes/fx-showcase.swan.json --sheet -o /tmp/sheet.png  # Headless FX render
nix build path:.                         # Build the installed package
```

Always build incrementally with CMake in `build/`; do not run `nix flake check` locally. It rebuilds everything from scratch in the Nix sandbox and runs only in CI (`.github/workflows/check.yml`, on pushes to `main` and pull requests): package build with `ctest`, the out-of-tree SDK example, and LuaLS type checks.

Inside `nix develop` and a graphical session, run `bash scripts/smoke-test.sh` for synchronization-validated rendering on the hardware GPU. Do not reintroduce Xvfb or a software Vulkan driver (Lavapipe) into tests; the script fails when a run reports a `(cpu)` device.

## Coding Style & Naming Conventions

Use four-space indentation in C++ and two spaces in CMake/Nix. Match surrounding compact formatting; avoid unrelated reformatting. Use `snake_case` filenames, `PascalCase` types, `camelCase` functions/members, and the `swan` namespace. Keep declarations in `.hpp` and implementations in `.cpp`. No formatter configuration is currently checked in; use compiler warnings and `git diff --check`.

## Testing Guidelines

Name tests `tests/<feature>_test.cpp` and register them in CMake. Use explicit runtime checks that remain active in Release builds. Add focused regression tests for behavior changes, especially invalid input, persistence, reload, lifetime, and document history. No numerical coverage threshold is configured. Renderer changes should pass synchronization validation in the smoke test. Prefer headless checks (`swan render`, `swan script`, `Preview`) over windowed ones where they cover the behavior.

## Commit & Pull Request Guidelines

History uses imperative, descriptive subjects such as `Add conservative frustum culling and draw statistics`; no prefix convention is required. Keep commits cohesive. PR descriptions should explain resulting behavior, relevant limitations, and commands actually tested. Link related issues when applicable and include screenshots for visible rendering changes.

## Architecture & Documentation Rules

Keep Vulkan ownership in the renderer and importer structs behind CPU asset boundaries. Particle and timeline simulation stays in the CPU `FxRuntime` (deterministic, fixed tick); the renderer only draws `ParticleBatch` snapshots. Effects, timelines, and environments share one JSON schema (`fx_io.cpp`) between scene files and Lua; keep `docs/FX.md` and `lua/types/swan.lua` in sync with it. Frontends change authored scenes only through `EditorDocument::showPreview()`/`apply()`.

Lua is bound through the plain C API (`src/script_lua.hpp`); do not add a binding library. Lua errors longjmp past C++ destructors, so binding functions wrap their bodies in `lua::protect()`, validate arguments by throwing C++ exceptions, call script code only through `lua_pcall` (`lua::callProtected`), and read script tables with raw access. Behaviour scripts stay sandboxed and mutate only runtime scenes. Keep `lua/types/swan.lua` in sync with the bindings; CI type-checks bundled scripts with LuaLS. Preserve authored/runtime scene separation and validated document commands. Resolve stable entity keys across document revisions instead of retaining handles. Maintain scene-version compatibility explicitly. Write all `README.md` files in English and update the roadmap when feature scope changes.
