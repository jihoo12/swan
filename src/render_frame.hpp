#pragma once
#include "camera.hpp"
#include "scene.hpp"
#include <vector>
namespace swan {
// Immutable snapshot: the GPU layer never owns or modifies gameplay entities.
struct RenderObject { Transform transform; Material material; SharedMesh mesh=cubeMesh(); SharedTexture texture=whiteTexture(); };
// One camera-facing particle quad, laid out for the GPU instance buffer. `color` is linear RGB
// (already multiplied by intensity) with alpha; `sprite` is a ParticleSprite value.
struct ParticleVertex {
    glm::vec3 position{}; float size=0;
    glm::vec4 color{1};
    glm::vec3 velocity{}; float rotation=0;
    float stretch=0; uint32_t sprite=0,seed=0; float padding=0;
};
static_assert(sizeof(ParticleVertex)==64);
// Particles sharing a blend mode and texture. Alpha-blended particles are sorted far to near
// by the renderer; additive ones are order-independent.
struct ParticleBatch { ParticleBlend blend=ParticleBlend::Additive; SharedTexture texture=whiteTexture(); std::vector<ParticleVertex> particles; };
// A nonzero targetSize renders the scene into an offscreen image (shown by the GUI) instead of the window.
struct RenderFrame {
    Camera camera;
    std::vector<RenderObject> objects;
    std::vector<ParticleBatch> particles;
    Environment environment;
    float time=0;
    glm::uvec2 targetSize{};
};
}
