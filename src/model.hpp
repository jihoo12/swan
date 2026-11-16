#pragma once
#include "mesh.hpp"
#include "texture.hpp"
#include <string>
namespace swan {
// A glTF material converted to Swan's unlit-plus-sun shading: base color factor and PNG
// base color texture, and emission as a multiplier of that surface color. Features that
// Swan cannot draw are listed in `unsupported` instead of failing the import.
struct ImportedMaterial {
    std::string name;
    glm::vec3 color{1}; float emission=0;
    SharedTexture texture;                // Null: no base color texture.
    std::vector<std::string> unsupported;
};
using SharedImportedMaterial=std::shared_ptr<const ImportedMaterial>;
// Engine-owned geometry and materials: no Assimp objects escape the import boundary.
// Parts follow depth-first node order, then each node's primitive order; `materials`
// holds each part's material (parts sharing a glTF material share one entry).
struct StaticModel { std::vector<SharedMesh> parts; std::vector<SharedImportedMaterial> materials; };
StaticModel loadStaticGltf(const std::filesystem::path& path);
}
