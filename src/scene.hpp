#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>
namespace swan {
struct Transform { glm::vec3 position{},scale{1}; float yaw=0; };
struct Material { glm::vec3 color{1}; float emission=0; };
struct Animation { float baseHeight=0,phase=0,bob=0.2f,speed=0.5f; };
struct EntityId {
    uint32_t index=std::numeric_limits<uint32_t>::max(),generation=0;
    bool operator==(const EntityId&) const = default;
};
struct Entity {
    std::string name;
    Transform transform;
    Material material;
    bool solid=false,collectible=false;
    std::optional<Animation> animation;
};
// Generation checks prevent a deleted object's handle from naming its replacement.
// References are valid only until the next create/destroy; retain EntityId instead.
class Scene {
public:
    EntityId create(Entity entity);
    bool destroy(EntityId id);
    Entity* get(EntityId id);
    const Entity* get(EntityId id) const;
    std::vector<EntityId> entities() const;
    size_t size() const { return liveCount; }
private:
    struct Slot { uint32_t generation=0; std::optional<Entity> entity; };
    std::vector<Slot> slots;
    std::vector<uint32_t> freeSlots;
    size_t liveCount=0;
};
}
