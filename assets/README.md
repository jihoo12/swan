# Swan scene assets

`scenes/playground.swan.json` (version 1) is a small hand-editable world with a floor, two pillars, a crystal goal, and three collectibles. Load it with:

```sh
./build/swan --scene assets/scenes/playground.swan.json
```

A scene file contains `format: "swan-scene"`, integer `version: 1` or `2`, a `spawn` feet position, named `materials`, and `entities`. All vectors use three numeric components. Positions/scales use world units; `yaw` is in radians. Material colors are nonnegative linear RGB values; `emission` is a nonnegative multiplier.

Each entity requires a unique stable `id`, a `name`, a `mesh` asset ID (`builtin:cube` is always available), a named `material`, and a `transform` containing `position`, positive `scale`, and `yaw`. Runtime generation handles are rebuilt when loading; stable file IDs remain unchanged.

Optional flags are `solid`, `collectible`, and `goal`. An optional `animation` contains `base_height`, `phase`, nonnegative `bob`, and `speed`; bobbing uses `sin(time * 1.3 + phase) * bob`, and rotation advances by `speed` radians per second. Animated solid colliders are currently rejected.

The engine scene format allows general worlds. The sample garden game additionally requires exactly one `goal` and at least one `collectible`, and an entity cannot have both flags. Use the `GameLayer` interface for games with different rules.

Materials are shared by stable name. Change one definition to recolor all referencing entities on reload. Version 2 supports imported triangle OBJ meshes; unknown mesh/material IDs, duplicate entity IDs, unknown fields, invalid vectors, unsupported versions, and nonfinite or out-of-range numeric values are rejected. Files are limited to 16 MiB and 10,000 entities.

## Imported meshes

```sh
./build/swan --scene assets/scenes/mesh-garden.swan.json
```

![Imported crystal meshes rendered by Swan](../docs/mesh-garden.png)

The version 2 example uses the locally authored `meshes/crystal.obj` for the goal and three collectibles. A version 2 scene can declare meshes alongside materials:

```json
"meshes": {
  "crystal-mesh": { "source": "../meshes/crystal.obj" }
}
```

Entities reference `"mesh": "crystal-mesh"`. Source paths are resolved relative to the **scene file's directory**, independently of the working directory. Multiple IDs resolving to the same file share one CPU mesh and one GPU upload. Reload builds a new catalog so file edits become visible; malformed or missing OBJ data prevents the entire world replacement. `builtin:` names cannot be overridden.

The OBJ subset accepts triangular faces, positive/negative position and normal indices, and optional normals. Supplied normals are normalized; missing normals are generated per face for flat shading. Triangulate meshes in your authoring tool before export. Non-triangle faces, line/point primitives, invalid position/normal indices, degenerate triangles, and invalid normals are rejected. Files are limited to 32 MiB and 1,000,000 triangle corners. UVs, vertex colors, smoothing groups, and MTL files are not used; appearance comes from scene materials. Bounds are computed from referenced vertices; solid meshes collide using a rotated box proxy, not exact triangle collision.

Export writes version 2 and rebases mesh sources relative to the output directory. It preserves references rather than copying OBJ files. Keep referenced files available when moving or sharing a scene. Existing version 1 scenes still load; version 1 does not allow a `meshes` table.

GPU resources use a shared buffer containing vertices and 32-bit indices. Buffers are uploaded once per resource and old ones are freed after the frame fence. `--reload-test --frames 90` triggers F5-style reloads after 10 and 30 frames for validation; use it with `--scene` to exercise mesh replacement.

## Editing workflow

Export the full procedural garden without opening a window:

```sh
./build/swan --export-scene /tmp/garden.swan.json
./build/swan --validate-scene /tmp/garden.swan.json
./build/swan --scene /tmp/garden.swan.json --save-scene /tmp/garden-copy.swan.json
```

Edit the input file in your text editor and press F5 in the running game. Successful reload resets gameplay and respawns using the file's `spawn`. If reading, parsing, or game validation fails, the old world stays active; the console reports the cause. Without `--scene`, F5 restarts the original procedural definition.

F6 exports the **loaded authored definition** to the explicit `--save-scene` path. It does not save transient animation, the player's position, removed collectibles, or progress. Use a different output path to keep edits to the input file from being replaced by its currently loaded definition. Save writes a unique temporary file beside the destination and renames it after a complete write; invalid data or write failures do not truncate the destination. This provides atomic replacement on Linux, not guaranteed persistence across power loss.

Scene paths are explicit and relative to the working directory when not absolute. The Nix package also installs these examples under `share/swan/assets/` next to `bin/`.
