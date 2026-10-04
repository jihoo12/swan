# Swan

A small C++20 / Vulkan 1.3 3D engine with a playable camera demo, **The Quiet Garden**: a tiled courtyard, a circle of ruined pillars, stylized trees, and a hovering cyan crystal. Everything is procedural; no external art assets are needed.

![The Quiet Garden rendered by Swan](docs/garden.png)

## Run

Linux with Nix flakes enabled and a Vulkan 1.3 driver is required. Both x86_64 and aarch64 Linux flake outputs are provided.

```sh
nix develop path:.
cmake -S . -B build -G Ninja
cmake --build build
./build/swan
```

GLFW chooses the desktop backend automatically. Pass `--x11` to force X11; the automated smoke test uses this option to ensure it runs inside Xvfb even from a Wayland desktop.

`path:.` includes newly created files even before they are tracked by Git. Once the project files are tracked, plain `nix develop` also works. `flake.lock` pins nixpkgs for reproducible dependencies.

To build the installable package:

```sh
nix build path:.
./result/bin/swan
```

The package installs its SPIR-V shaders alongside the executable. Shader lookup uses the installed executable's location, then the CMake build directory; `--shader-dir PATH` overrides it.

## Controls

| Input | Action |
| --- | --- |
| Left click | Capture the mouse for looking around |
| Mouse | Look while captured |
| W / A / S / D | Move horizontally |
| Q / E | Descend / ascend |
| Left Shift | Sprint |
| Space | Pause the crystal animation |
| R | Reset the camera |
| Escape | Release the mouse; press again to quit |

The window title shows FPS and object count. Movement uses elapsed time and stops at a minimum camera height. Losing focus releases the mouse.

## Engine

- GLFW window and keyboard/mouse input.
- Vulkan 1.3 dynamic rendering, graphics/present queue selection, FIFO presentation.
- Perspective camera, depth testing, per-object transforms, and procedural cube geometry generated in the vertex shader.
- Directional sunlight, local cyan lighting, emissive materials, distance fog, and simple tone mapping.
- Swapchain and depth attachment recreation on resize, including waiting while minimized.
- One frame in flight, a frame fence, an acquire semaphore, and a presentation semaphore per swapchain image. This deliberately simple arrangement serializes shared depth-buffer use.
- Optional Khronos validation with error reporting and a nonzero exit status for runtime validation errors.
- Cleanup of partially initialized resources if startup fails.

This is an engine foundation with a visual demo. It currently has no mesh/texture import, physics, audio, editor, shadow maps, or bloom. The emissive crystal is lit geometry, not a post-processing glow effect. Scene objects are defined in `src/scene.hpp`; extend this file to create new worlds.

## Verification

```sh
nix develop path:.
ctest --test-dir build --output-on-failure
./build/swan --validation --frames 90 --resize-test
```

For automated execution without a desktop or hardware GPU:

```sh
nix develop path:. --command bash scripts/smoke-test.sh
```

The smoke test uses Xvfb and Mesa Lavapipe, enables synchronization validation, renders 90 frames, and changes the window size twice. The CPU scene test checks generated geometry dimensions, finite coordinates, animation count, and camera direction normalization.

```sh
nix flake check path:.
./build/swan --help
```

`nix flake check` builds the package and runs the CPU scene test; the graphical smoke test is separate.

## Layout

| File | Responsibility |
| --- | --- |
| `flake.nix`, `flake.lock` | Pinned toolchain, dependencies, shell, package, build check |
| `CMakeLists.txt` | C++ build, GLSL-to-SPIR-V compilation, installation, tests |
| `src/main.cpp` | Command-line options and error reporting |
| `src/engine.hpp`, `src/engine.cpp` | Vulkan resource ownership, input, camera, render loop |
| `src/scene.hpp` | Procedural world and camera direction |
| `shaders/scene.vert`, `shaders/scene.frag` | Geometry, transforms, lighting, fog |
| `scripts/smoke-test.sh` | Software-rendered validation and resize test |

## Driver troubleshooting

`vulkaninfo --summary` should list a working device. On NixOS, enable graphics support in the system configuration; the development shell supplies libraries and tools, while hardware drivers remain a system responsibility. On other Linux distributions, use the host Vulkan driver setup (a NixGL wrapper may be needed for Nix applications). The smoke test selects its own software driver and does not require this integration.

## References

The renderer follows Vulkan's documented dynamic-rendering, depth, and synchronization rules. Useful references: [Khronos Vulkan tutorial](https://docs.vulkan.org/tutorial/latest/01_Overview.html), [depth buffering](https://docs.vulkan.org/tutorial/latest/07_Depth_buffering.html), and [rendering and presentation](https://docs.vulkan.org/tutorial/latest/03_Drawing_a_triangle/03_Drawing/02_Rendering_and_presentation.html).

## License

Apache-2.0; see [LICENSE](LICENSE).
