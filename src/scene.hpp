#pragma once
#include "assets.hpp"
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>
namespace swan {
struct Transform { glm::vec3 position{},scale{1}; float yaw=0; };
// Compose a child with a uniform-scale, yaw-only parent.
Transform composeTransform(const Transform& parent,const Transform& child);
// Inverse of composeTransform: the child-local transform that produces `world` under `parent`.
Transform relativeTransform(const Transform& parent,const Transform& world);
struct Animation { float baseHeight=0,phase=0,bob=0.2f,speed=0.5f; };
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
};
}
