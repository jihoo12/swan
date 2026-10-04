#include "render_pose.hpp"
#include <algorithm>
#include <array>
#include <cmath>
namespace swan {
void RenderPose::capture(const Scene& scene) {
    auto ids=scene.entities();
    size_t count=0;
    for(auto id:ids) count=std::max(count,size_t(id.index)+1);
    std::vector<std::optional<Entry>> next(count);
    for(auto id:ids) next[id.index]=Entry{id,scene.get(id)->transform,scene.parent(id)};
    entries=std::move(next);
}
Transform RenderPose::worldTransform(const Scene& scene,EntityId id,float interpolation) const {
    // Validate current hierarchy and keep discontinuities on the current state.
    auto current=scene.worldTransform(id);
    float alpha=std::isfinite(interpolation)?std::clamp(interpolation,0.0f,1.0f):1.0f;
    if(alpha==1) return current;
    std::array<EntityId,65> chain;
    size_t count=0;
    std::optional<EntityId> node=id;
    while(node) {
        if(count==chain.size() || node->index>=entries.size()) return current;
        const auto& previous=entries[node->index];
        if(!previous || previous->id!=*node || previous->parent!=scene.parent(*node)) return current;
        chain[count++]=*node;node=scene.parent(*node);
    }
    auto local=[&](EntityId entity) {
        const auto& before=entries[entity.index]->local;
        const auto& after=scene.get(entity)->transform;
        if(alpha==0) return before;
        Transform result;
        result.position=glm::mix(before.position,after.position,alpha);
        result.scale=glm::mix(before.scale,after.scale,alpha);
        constexpr float tau=6.28318530718f;
        result.yaw=before.yaw+std::remainder(after.yaw-before.yaw,tau)*alpha;
        return result;
    };
    Transform result=local(chain[0]);
    // Interpolate local poses before composing to preserve a child's orbit.
    for(size_t i=1;i<count;++i) result=composeTransform(local(chain[i]),result);
    return result;
}
}
