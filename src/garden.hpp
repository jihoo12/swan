#pragma once
#include "scene.hpp"
namespace swan {
struct Garden { Scene scene; EntityId core; std::vector<EntityId> shards; };
Garden makeGarden();
}
