#pragma once
#include "camera.hpp"
#include "render_frame.hpp"
#include <span>
namespace swan {
// Orthographic sun shadow projection for one frame (Vulkan clip space, depth 0..1).
struct ShadowProjection {
    glm::mat4 viewProjection{1};
    float texelWorld=0;    // World-space size of one shadow-map texel (used for normal-offset bias).
};
// Fits the light volume to a bounding sphere of the camera frustum out to
// environment.shadowDistance, snapped to whole texels so a moving camera does not make
// shadow edges shimmer. The depth range extends toward the sun to include every object
// whose bounds overlap the light volume, so off-screen casters still cast into view.
ShadowProjection fitSunShadow(const Camera& camera,float aspect,const Environment& environment,
                              std::span<const RenderObject> objects,uint32_t resolution);
}
