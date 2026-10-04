#include "scene.hpp"
#include <stdexcept>
#include <cmath>
namespace swan {
EntityId Scene::create(Entity entity) {
    const auto& t=entity.transform;
    for(int i=0;i<3;++i) {
        if(!std::isfinite(t.position[i]) || !std::isfinite(t.scale[i]) || t.scale[i]<=0 || !std::isfinite(entity.material.color[i]))
            throw std::invalid_argument("Entity transform/material must be finite and scale positive");
    }
    if(!std::isfinite(t.yaw) || !std::isfinite(entity.material.emission) || entity.material.emission<0)
        throw std::invalid_argument("Invalid entity rotation/emission");
    uint32_t index;
    if(freeSlots.empty()) { index=uint32_t(slots.size()); slots.emplace_back(); }
    else { index=freeSlots.back(); freeSlots.pop_back(); }
    slots[index].entity=std::move(entity);
    ++liveCount;
    return {index,slots[index].generation};
}
bool Scene::destroy(EntityId id) {
    if(!get(id)) return false;
    auto& slot=slots[id.index];
    slot.entity.reset(); ++slot.generation; --liveCount;
    // Never recycle a slot after its generation wraps around.
    if(slot.generation!=0) freeSlots.push_back(id.index);
    return true;
}
Entity* Scene::get(EntityId id) {
    if(id.index>=slots.size()) return nullptr;
    auto& slot=slots[id.index];
    return slot.generation==id.generation && slot.entity?&*slot.entity:nullptr;
}
const Entity* Scene::get(EntityId id) const {
    if(id.index>=slots.size()) return nullptr;
    const auto& slot=slots[id.index];
    return slot.generation==id.generation && slot.entity?&*slot.entity:nullptr;
}
std::vector<EntityId> Scene::entities() const {
    std::vector<EntityId> result; result.reserve(liveCount);
    for(uint32_t i=0;i<slots.size();++i) if(slots[i].entity) result.push_back({i,slots[i].generation});
    return result;
}
}
