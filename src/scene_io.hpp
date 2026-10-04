#pragma once
#include "scene.hpp"
#include <filesystem>
#include <string_view>
namespace swan {
struct SceneDocument { Scene scene; glm::vec3 spawn{0,0.2f,14}; };
SceneDocument parseScene(std::string_view json);
std::string serializeScene(const SceneDocument& document);
SceneDocument loadScene(const std::filesystem::path& path);
// Writes a sibling temporary file and renames it after a complete write.
void saveScene(const std::filesystem::path& path,const SceneDocument& document);
}
