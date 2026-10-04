#pragma once
#include "camera.hpp"
#include "scene.hpp"
#include <vector>
namespace swan {
// Immutable snapshot: the GPU layer never owns or modifies gameplay entities.
struct RenderObject { Transform transform; Material material; SharedMesh mesh=cubeMesh(); SharedTexture texture=whiteTexture(); };
// A nonzero targetSize renders the scene into an offscreen image (shown by the GUI) instead of the window.
struct RenderFrame { Camera camera; std::vector<RenderObject> objects; float time=0; glm::uvec2 targetSize{}; };
}
