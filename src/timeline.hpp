#pragma once
#include "camera.hpp"
#include "fx.hpp"
#include <optional>
#include <string>
#include <vector>
namespace swan {
// Animated properties. Entity tracks write local transforms (yaw in radians) or scale the
// entity's attached effect emission (`emit`); material tracks write shared material values;
// environment tracks write the scene's exposure, bloom strength, or background/fog color.
enum class TrackTarget : uint8_t { Entity, Material, Environment };
enum class TrackProperty : uint8_t { Position, Scale, Yaw, Emit, Color, Emission, Exposure, Bloom, Background };
struct TimelineTrack {
    TrackTarget kind=TrackTarget::Entity;
    std::string target;    // Entity key or material ID; empty for the environment.
    TrackProperty property=TrackProperty::Position;
    Vec3Curve vector;      // Position, scale, color, background.
    FloatCurve scalar;     // Yaw, emit, emission, exposure, bloom.
    bool isVector() const {
        return property==TrackProperty::Position || property==TrackProperty::Scale || property==TrackProperty::Color || property==TrackProperty::Background;
    }
};
// The target kind a property belongs to.
TrackTarget propertyTarget(TrackProperty property);
// Plays a one-shot effect instance at `time`, at an entity (plus offset) or a world position.
struct TimelineEvent {
    float time=0;
    std::string effect;
    std::string entity;    // Empty: use `position`.
    glm::vec3 position{};  // World position, or offset from the entity.
    float yaw=0;
    uint32_t seed=0;
};
// Shot camera; tracks are optional individually (a missing track keeps the fallback value).
struct CameraTrack {
    Vec3Curve position,target;
    FloatCurve fov;
    bool empty() const { return position.empty() && target.empty() && fov.empty(); }
};
// Scene-level keyframe animation in seconds. `duration` 0 means "until the last key/event".
struct Timeline {
    float duration=0;
    bool loop=false;
    std::vector<TimelineTrack> tracks;
    std::vector<TimelineEvent> events;   // Sorted by time.
    CameraTrack camera;
    bool empty() const { return tracks.empty() && events.empty() && camera.empty() && duration==0 && !loop; }
    // The explicit duration, else the latest key or event time.
    float length() const;
    // Maps elapsed seconds to timeline time (wrapping when looping, holding at the end otherwise).
    float localTime(double elapsed) const;
    // The shot camera at `time`, built on `fallback`; nullopt without camera tracks.
    std::optional<Camera> cameraAt(float time,const Camera& fallback={}) const;
    // Drop tracks and events that reference an entity or effect (used by cascading deletes).
    void removeEntity(const std::string& key);
    void removeEffect(const std::string& id);
};
constexpr size_t maxTimelineTracks=512,maxTimelineEvents=1024;
// Structural checks only; Scene::validate() checks entity, material, and effect references.
void validateTimeline(const Timeline& timeline);
const char* propertyName(TrackProperty property);
// Camera looking from `position` toward `target` (yaw/pitch; no roll).
Camera lookAt(glm::vec3 position,glm::vec3 target,float fov=65);
}
