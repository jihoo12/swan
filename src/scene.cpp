#include "scene.hpp"
#include <stdexcept>
#include <cmath>
#include <charconv>
#include <algorithm>
namespace swan {
EntityId Scene::create(Entity entity) {
    const auto& t=entity.transform;
    for(int i=0;i<3;++i) {
        if(!std::isfinite(t.position[i]) || !std::isfinite(t.scale[i]) || t.scale[i]<=0)
            throw std::invalid_argument("Entity transform must be finite and scale positive");
    }
    if(!std::isfinite(t.yaw))
        throw std::invalid_argument("Invalid entity rotation");
    if(!meshAssets.contains(entity.meshId)) throw std::invalid_argument("Unsupported mesh asset: "+entity.meshId);
    if(!materialAssets.contains(entity.materialId)) throw std::invalid_argument("Unknown material asset: "+entity.materialId);
    if(!textureAssets.contains(materialAssets.get(entity.materialId).textureId)) throw std::invalid_argument("Unknown material texture asset");
    if(entity.animation) {
        const auto& a=*entity.animation;
        if(!std::isfinite(a.baseHeight) || !std::isfinite(a.phase) || !std::isfinite(a.bob) || !std::isfinite(a.speed) || a.bob<0)
            throw std::invalid_argument("Invalid entity animation");
        if(entity.solid) throw std::invalid_argument("Animated solid colliders are not supported");
    }
    if(entity.goal && entity.collectible) throw std::invalid_argument("A goal cannot also be collectible");
    if(entity.key.empty()) {
        do { entity.key="entity-"+std::to_string(serial++); } while(get(find(entity.key)));
    }
    if(entity.key.size()>256 || get(find(entity.key))) throw std::invalid_argument("Invalid or duplicate entity key: "+entity.key);
    // Imported generated keys must reserve the serial range too.
    if(entity.key.starts_with("entity-")) {
        uint64_t value=0;
        const auto* first=entity.key.data()+7;
        const auto* last=entity.key.data()+entity.key.size();
        auto [end,error]=std::from_chars(first,last,value);
        if(error==std::errc{} && end==last && value<std::numeric_limits<uint64_t>::max()) serial=std::max(serial,value+1);
    }
    uint32_t index;
    if(freeSlots.empty()) { index=uint32_t(slots.size()); slots.emplace_back(); }
    else { index=freeSlots.back(); freeSlots.pop_back(); }
    slots[index].entity=std::move(entity);
    ++liveCount;
    return {index,slots[index].generation};
}
EntityId Scene::find(const std::string& key) const {
    for(uint32_t i=0;i<slots.size();++i)
        if(slots[i].entity && slots[i].entity->key==key) return {i,slots[i].generation};
    return {};
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
