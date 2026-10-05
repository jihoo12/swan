#pragma once
#include "assets.hpp"
#include "timeline.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <map>
#include <string>
#include <variant>
#include <vector>
namespace swan {
struct Transform { glm::vec3 position{},scale{1}; float yaw=0; };
// Compose a child with a uniform-scale, yaw-only parent.
Transform composeTransform(const Transform& parent,const Transform& child);
// Inverse of composeTransform: the child-local transform that produces `world` under `parent`.
Transform relativeTransform(const Transform& parent,const Transform& world);
struct Animation { float baseHeight=0,phase=0,bob=0.2f,speed=0.5f; };
// Per-entity script parameters (number, flag, or text), overriding the script's defaults.
using ScriptValue=std::variant<double,bool,std::string>;
using ScriptProperties=std::map<std::string,ScriptValue>;
// Scene-wide look: background/fog color (linear), lighting, and the HDR post-process.
enum class ToneMap : uint8_t { Reinhard, Aces };
struct Environment {
    glm::vec3 background{0.035f,0.065f,0.095f};  // Clear color and fog color.
    float fog=0.00065f;                          // Density over squared distance; 0 disables.
    float ambient=0.24f,sun=0.9f,localLight=1;   // localLight: the cyan light above the origin.
    float exposure=1;
    float bloom=0.35f,bloomThreshold=1;          // Glow of HDR values above the threshold.
    ToneMap toneMap=ToneMap::Reinhard;
    bool operator==(const Environment&) const=default;
};
struct EntityId {
    uint32_t index=std::numeric_limits<uint32_t>::max(),generation=0;
    bool operator==(const EntityId&) const = default;
};
struct Entity {
    std::string name;
    Transform transform;
    std::string key;
    std::string materialId="default";
    std::string meshId="builtin:cube";
    bool goal=false;
    bool solid=false,collectible=false;
    std::optional<Animation> animation;
    std::string scriptId;          // Empty: no behaviour script.
    ScriptProperties properties;   // At most 32; keys 1..64 characters.
    std::string effectId;          // Empty: no attached particle effect.
};
// Generation checks prevent a deleted object's handle from naming its replacement.
// Handles are scoped to one scene; use entity keys for identity across reloads.
// References are valid only until the next create/destroy; retain EntityId instead.
class Scene {
public:
    EntityId create(Entity entity);
    // Validate mutable entities and hierarchy without reopening source assets.
    void validate() const;
    EntityId find(const std::string& key) const;
    MaterialAssets& assets() { return materialAssets; }
    const MaterialAssets& assets() const { return materialAssets; }
    MeshAssets& meshes() { return meshAssets; }
    const MeshAssets& meshes() const { return meshAssets; }
    TextureAssets& textures() { return textureAssets; }
    const TextureAssets& textures() const { return textureAssets; }
    ScriptAssets& scripts() { return scriptAssets; }
    const ScriptAssets& scripts() const { return scriptAssets; }
    EffectAssets& effects() { return effectAssets; }
    const EffectAssets& effects() const { return effectAssets; }
    const Environment& environment() const { return sceneEnvironment; }
    // Validates ranges; the runtime timeline may also animate exposure, bloom, and background.
    void setEnvironment(Environment value);
    Timeline& timeline() { return sceneTimeline; }
    const Timeline& timeline() const { return sceneTimeline; }
    // Effect textures and timeline references to entities, materials, and effects.
    void validateReferences() const;
    // Local transforms are retained on reparenting; parents require uniform scale.
    void setParent(EntityId child, std::optional<EntityId> parent);
    std::optional<EntityId> parent(EntityId child) const;
    Transform worldTransform(EntityId id) const;
    bool destroy(EntityId id);
    Entity* get(EntityId id);
    const Entity* get(EntityId id) const;
    std::vector<EntityId> entities() const;
    size_t size() const { return liveCount; }
private:
    struct Slot { uint32_t generation=0; std::optional<Entity> entity; std::optional<EntityId> parent; };
    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlots;
    size_t liveCount=0;
    uint64_t serial=0;
    MaterialAssets materialAssets;
    MeshAssets meshAssets;
    TextureAssets textureAssets;
    ScriptAssets scriptAssets;
    EffectAssets effectAssets;
    Timeline sceneTimeline;
    Environment sceneEnvironment;
};
}
