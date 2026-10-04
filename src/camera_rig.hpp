#pragma once
#include "camera.hpp"
#include "scene.hpp"
namespace swan {
// Stateless presentation rig; controller orientation and player motion stay separate.
struct ThirdPersonRig {
    float distance=4.0f,clearance=0.2f;
    Camera camera(const Scene& scene,const Camera& aim,glm::vec3 target) const;
};
}
