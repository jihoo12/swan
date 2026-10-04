#pragma once
#include "mesh.hpp"
#include <map>
#include <string>
namespace swan {
struct MeshAsset { std::filesystem::path source; SharedMesh data; };
class MeshAssets {
public:
    MeshAssets();
    void load(std::string id,const std::filesystem::path& path);
    const MeshAsset& get(const std::string& id) const;
    bool contains(const std::string& id) const { return meshes.contains(id); }
    const std::map<std::string,MeshAsset>& entries() const { return meshes; }
private:
    std::map<std::string,MeshAsset> meshes;
};
struct Material { glm::vec3 color{1}; float emission=0; };
// Stable material names are shared by scene entities and serialized files.
class MaterialAssets {
public:
    MaterialAssets();
    void set(std::string id,Material material);
    const Material& get(const std::string& id) const;
    bool contains(const std::string& id) const { return materials.contains(id); }
    const std::map<std::string,Material>& entries() const { return materials; }
private:
    std::map<std::string,Material> materials;
};
}
