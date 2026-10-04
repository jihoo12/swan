#pragma once
#include "scene.hpp"
namespace swan {
// Transform-only history; GPU assets and gameplay state remain in the live scene.
// A pose belongs to one scene lifetime. Capture anew after replacing/reloading it.
class RenderPose {
public:
    void capture(const Scene& scene);
    Transform worldTransform(const Scene& scene,EntityId id,float interpolation) const;
private:
    struct Entry { EntityId id; Transform local; std::optional<EntityId> parent; };
    std::vector<std::optional<Entry>> entries;
};
}
