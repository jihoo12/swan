#pragma once
#include "camera.hpp"
#include "scene.hpp"
#include <vector>
namespace swan {
// Immutable snapshot: the GPU layer never owns or modifies gameplay entities.
struct RenderObject { Transform transform; Material material; };
struct RenderFrame { Camera camera; std::vector<RenderObject> objects; float time=0; };
}
