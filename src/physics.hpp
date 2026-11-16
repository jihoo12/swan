#pragma once
#include "scene.hpp"
#include <optional>
#include <vector>
namespace swan {
// An entity's collision box: its mesh bounds under the world transform, as a yaw-rotated box
// whose `scale` is the full size. Collision, raycasts, overlaps, and triggers all use it.
Transform collisionBox(const Scene& scene,EntityId id);
// Which entities a query considers. Triggers are invisible volumes and skipped by default.
struct QueryFilter { bool solidOnly=false,triggers=false; std::optional<EntityId> ignore; };
struct RayHit { EntityId entity; glm::vec3 point{},normal{}; float distance=0; };
// Nearest entity box along the ray within maxDistance (direction need not be normalized).
// Boxes containing the origin are skipped, so a ray cast from inside an object sees past it.
std::optional<RayHit> raycast(const Scene& scene,glm::vec3 origin,glm::vec3 direction,float maxDistance,const QueryFilter& filter={});
// Entities whose box intersects the sphere, in scene order.
std::vector<EntityId> overlapSphere(const Scene& scene,glm::vec3 center,float radius,const QueryFilter& filter={});
// Whether the player capsule (feet position) overlaps the entity's box.
bool capsuleTouches(const Scene& scene,EntityId id,glm::vec3 feet,float radius=0.3f,float height=1.8f);
struct MotionResult { glm::vec3 feet{}; bool grounded=false,ceiling=false; };
// Upright capsule against static yaw-rotated boxes. Small movement substeps and
// iterative depenetration prevent normal player speeds from skipping thin walls.
MotionResult moveCapsule(const Scene& scene,glm::vec3 feet,glm::vec3 displacement,
                         float radius=0.3f,float height=1.8f);
}
