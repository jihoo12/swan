#include "scene.hpp"
#include <stdexcept>
#include <cmath>
#include <charconv>
#include <algorithm>
namespace swan {
Transform composeTransform(const Transform& parent,const Transform& child) {
    if(parent.scale.x!=parent.scale.y || parent.scale.x!=parent.scale.z || parent.scale.x<=0)
        throw std::invalid_argument("Hierarchy parents require positive uniform scale");
    auto p=child.position*parent.scale.x;
    float c=std::cos(parent.yaw),s=std::sin(parent.yaw);
    return {parent.position+glm::vec3(c*p.x+s*p.z,p.y,-s*p.x+c*p.z),child.scale*parent.scale.x,child.yaw+parent.yaw};
}
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
std::optional<EntityId> Scene::parent(EntityId child) const {
    if(!get(child)) throw std::invalid_argument("Invalid hierarchy handle");
    return slots[child.index].parent;
}
Transform Scene::worldTransform(EntityId id) const {
    if(!get(id)) throw std::invalid_argument("Invalid world transform handle");
    Transform result=get(id)->transform;
    auto ancestor=parent(id);
    size_t depth=0;
    while(ancestor) {
        if(++depth>64 || !get(*ancestor)) throw std::invalid_argument("Invalid hierarchy depth/parent");
        const auto& entity=*get(*ancestor);
        result=composeTransform(entity.transform,result);
        if(get(id)->solid && entity.animation) throw std::invalid_argument("Solid collider cannot inherit animation");
        ancestor=parent(*ancestor);
    }
    for(int i=0;i<3;++i) if(!std::isfinite(result.position[i]) || !std::isfinite(result.scale[i]) || result.scale[i]<=0)
        throw std::invalid_argument("Invalid world transform");
    if(!std::isfinite(result.yaw)) throw std::invalid_argument("Invalid world rotation");
    return result;
}
void Scene::setParent(EntityId child,std::optional<EntityId> next) {
    if(!get(child) || (next && !get(*next))) throw std::invalid_argument("Invalid parent/child handle");
    auto ancestor=next; size_t depth=0;
    while(ancestor) {
        if(*ancestor==child) throw std::invalid_argument("Hierarchy cycle");
        if(++depth>64) throw std::invalid_argument("Hierarchy depth exceeds 64");
        ancestor=parent(*ancestor);
    }
    auto previous=slots[child.index].parent;
    slots[child.index].parent=next;
    try { for(auto id:entities()) worldTransform(id); }
    catch(...) {slots[child.index].parent=previous; throw;}
}
bool Scene::destroy(EntityId id) {
    if(!get(id)) return false;
    // Detach direct children in world space before deleting their parent.
    std::vector<std::pair<EntityId,Transform>> children;
    for(auto child:entities()) if(parent(child)==id) children.emplace_back(child,worldTransform(child));
    for(const auto& [child,transform]:children) {
        auto* entity=get(child);
        if(entity->animation) {
            float factor=transform.scale.x/entity->transform.scale.x;
            entity->animation->baseHeight=transform.position.y+(entity->animation->baseHeight-entity->transform.position.y)*factor;
            entity->animation->bob*=factor;
        }
        slots[child.index].parent.reset(); entity->transform=transform;
    }
    auto& slot=slots[id.index];
    slot.parent.reset(); slot.entity.reset(); ++slot.generation; --liveCount;
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
