#pragma once
#include <glm/glm.hpp>
#include <map>
#include <string>
namespace swan {
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
