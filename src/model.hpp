#pragma once
#include "mesh.hpp"
namespace swan {
// Engine-owned geometry only: no Assimp objects escape the import boundary.
// Parts follow depth-first node order, then each node's primitive order.
struct StaticModel { std::vector<SharedMesh> parts; };
StaticModel loadStaticGltf(const std::filesystem::path& path);
}
