#pragma once
#include "scene.hpp"
namespace swan {
struct MotionResult { glm::vec3 feet{}; bool grounded=false,ceiling=false; };
// Upright capsule against static yaw-rotated boxes. Small movement substeps and
// iterative depenetration prevent normal player speeds from skipping thin walls.
MotionResult moveCapsule(const Scene& scene,glm::vec3 feet,glm::vec3 displacement,
                         float radius=0.3f,float height=1.8f);
}
