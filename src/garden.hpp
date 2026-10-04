#pragma once
#include "scene.hpp"
namespace swan {
struct Garden { Scene scene; EntityId core; std::vector<EntityId> shards; glm::vec3 spawn{0,0.2f,14}; };
Garden gardenFromScene(Scene scene,glm::vec3 spawn);
Garden makeGarden();
}
