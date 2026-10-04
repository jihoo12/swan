#pragma once
#include "camera.hpp"
#include "scene.hpp"
namespace swan {
// Coordinates use the full render surface, including the area beneath UI panels.
std::string pickEntity(const Scene& scene,const Camera& camera,glm::vec2 pixel,glm::vec2 display);
Entity editorCube(const Camera& camera);
}
