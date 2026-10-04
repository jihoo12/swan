#pragma once
#include "camera.hpp"
#include "scene.hpp"
#include <array>
#include <optional>
namespace swan {
// Coordinates are pixels within the rendered scene image (viewport), origin at its top-left.
std::string pickEntity(const Scene& scene,const Camera& camera,glm::vec2 pixel,glm::vec2 display);
Entity editorCube(const Camera& camera);
// translate * rotateY(yaw) * scale, matching the scene shader and composeTransform().
glm::mat4 transformMatrix(const Transform& transform);
// Inverse of transformMatrix() for yaw-only matrices, e.g. after a Y-rotation/scale/translate gizmo.
Transform transformFromMatrix(const glm::mat4& matrix);
// World-space corners of an entity's mesh bounds (for selection outlines and framing).
std::array<glm::vec3,8> worldBounds(const Scene& scene,EntityId id);
// Where a viewport ray meets the y=0 ground plane, if in front of the camera and within maxDistance.
std::optional<glm::vec3> groundPoint(const Camera& camera,glm::vec2 pixel,glm::vec2 display,float maxDistance=60);
// "base", "base-2", "base-3", ... — the first ID not already used by a material.
std::string uniqueMaterialId(const MaterialAssets& materials,const std::string& base);
}
