# Repository Guidelines

## Project Structure & Module Organization

Swan is a C++20 Vulkan 1.3 engine with a garden demo and an initial GUI editor.

- `src/`: core scene/assets/physics, Vulkan renderer, runtime loop, demo gameplay, and editor document/UI. CMake separates these into `swan_core`, `swan_renderer`, `swan_runtime`, `swan_demo`, and `swan_editor`.
- `shaders/`: GLSL sources compiled to SPIR-V during the build.
- `tests/`: standalone C++ regression executables registered with CTest.
- `assets/`: versioned scene JSON, models, textures, and example edit commands.
- `scripts/`: rendering and GUI interaction smoke tests.
- `docs/`: roadmap and screenshots. `build/` and `result` are generated outputs.

## Build, Test, and Development Commands

Run from the repository root:

```sh
nix develop path:.                       # Enter the pinned development environment
cmake -S . -B build -G Ninja              # Configure
cmake --build build                      # Build applications, shaders, and tests
ctest --test-dir build --output-on-failure
./build/swan --editor --scene assets/scenes/gltf-garden.swan.json
nix build path:.                         # Build the installed package
nix flake check path:.                   # Validate available flake checks
```

Inside `nix develop`, run `bash scripts/smoke-test.sh` for software Vulkan validation and `bash scripts/editor-ui-test.sh` for actual GUI input tests.

## Coding Style & Naming Conventions

Use four-space indentation in C++ and two spaces in CMake/Nix. Match surrounding compact formatting; avoid unrelated reformatting. Use `snake_case` filenames, `PascalCase` types, `camelCase` functions/members, and the `swan` namespace. Keep declarations in `.hpp` and implementations in `.cpp`. No formatter configuration is currently checked in; use compiler warnings and `git diff --check`.

## Testing Guidelines

Name tests `tests/<feature>_test.cpp` and register them in CMake. Use explicit runtime checks that remain active in Release builds. Add focused regression tests for behavior changes, especially invalid input, persistence, reload, lifetime, and editor history. No numerical coverage threshold is configured. Renderer changes should pass synchronization validation; GUI changes should pass the interaction script.

## Commit & Pull Request Guidelines

History uses imperative, descriptive subjects such as `Add conservative frustum culling and draw statistics`; no prefix convention is required. Keep commits cohesive. PR descriptions should explain resulting behavior, relevant limitations, and commands actually tested. Link related issues when applicable and include screenshots for visible editor/rendering changes.

## Architecture & Documentation Rules

Keep Vulkan ownership in the renderer and importer structs behind CPU asset boundaries. Preserve authored/runtime scene separation and validated editor commands. Resolve stable entity keys across document revisions instead of retaining handles. Maintain scene-version compatibility explicitly. Write all `README.md` files in English and update the roadmap when feature scope changes.
