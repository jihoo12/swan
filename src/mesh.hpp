#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
namespace swan {
struct Vertex { glm::vec3 position{},normal{}; glm::vec2 uv{}; };
struct MeshData {
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;
    glm::vec3 minimum{},maximum{};
};
using SharedMesh=std::shared_ptr<const MeshData>;
SharedMesh cubeMesh();
SharedMesh loadObjMesh(const std::filesystem::path& path);
}
