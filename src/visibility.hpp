#pragma once
#include "scene.hpp"
#include <array>
namespace swan {
// Vulkan clip volume: -w <= x,y <= w and 0 <= z <= w.
// Conservative plane tests retain boxes crossing the camera or a clip boundary.
class Frustum {
public:
    explicit Frustum(const glm::mat4& viewProjection);
    bool intersects(const MeshData& mesh,const Transform& world) const;
private:
    std::array<glm::dvec4,6> planes;
};
}
