#pragma once
#include "render_frame.hpp"
#include "scene.hpp"
#include <cstdint>
#include <memory>
#include <string>
#include <vector>
namespace swan {
struct FxEmitterStats {
    std::string effect,emitter,entity;   // Effect ID, emitter name (or index), followed entity.
    size_t particles=0;
    uint64_t spawned=0,dropped=0;        // Dropped: spawns refused by max_particles or the global cap.
    glm::vec3 minimum{},maximum{};       // World bounds of live particle centers (when particles > 0).
};
struct FxStats {
    size_t instances=0,particles=0;
    uint64_t spawned=0,dropped=0;
    glm::vec3 minimum{},maximum{};
    std::vector<FxEmitterStats> emitters;
};
// Runs a scene's particle effects and timeline against a runtime scene copy (never authored
// data). Deterministic: the same scene, seed, and update steps produce the same particles.
//
// Each update: advance time, apply timeline tracks at the new time (writing local transforms and
// material values into the scene), fire timeline events crossed by the step, then simulate every
// effect instance. Entities with an `effect` emit continuously at their world position and yaw;
// removing the entity (or changing its effect) stops emission and lets live particles finish.
class FxRuntime {
public:
    static constexpr size_t maxLiveParticles=200000;
    // Applies the timeline at time 0 and fires time-0 events; does not simulate yet.
    explicit FxRuntime(Scene& scene,uint32_t seed=0);
    ~FxRuntime();
    FxRuntime(FxRuntime&&) noexcept;
    FxRuntime& operator=(FxRuntime&&) noexcept;
    void update(Scene& scene,float dt);
    // Starts a one-shot instance of `effect` at `position` (world space), or following `entity`
    // with `position` as an offset. Throws for unknown effects.
    void play(const Scene& scene,const std::string& effect,glm::vec3 position,float yaw=0,const std::string& entity={},uint32_t seed=0);
    double elapsed() const;
    float timelineTime() const;
    size_t particleCount() const;
    FxStats stats() const;
    // Adds one batch per (blend, texture) pair of live particles.
    void appendTo(RenderFrame& frame,const Scene& scene) const;
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
};
// Writes a timeline's animated values at `time` into the scene (no effects are simulated).
void applyTimeline(Scene& scene,float time);
}
