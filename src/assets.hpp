#pragma once
#include "fx.hpp"
#include "mesh.hpp"
#include "model.hpp"
#include "texture.hpp"
#include <map>
#include <memory>
#include <string>
namespace swan {
struct MeshAsset { std::filesystem::path source; SharedMesh data; uint32_t part=0; };
class MeshAssets {
public:
    MeshAssets();
    void load(std::string id,const std::filesystem::path& path,uint32_t part=0);
    const MeshAsset& get(const std::string& id) const;
    bool contains(const std::string& id) const { return meshes.contains(id); }
    const std::map<std::string,MeshAsset>& entries() const { return meshes; }
private:
    std::map<std::string,MeshAsset> meshes;
    std::map<std::filesystem::path,StaticModel> models;
};
struct TextureAsset { std::filesystem::path source; SharedTexture data; };
class TextureAssets {
public:
    TextureAssets();
    void load(std::string id,const std::filesystem::path& path);
    const TextureAsset& get(const std::string& id) const;
    bool contains(const std::string& id) const { return textures.contains(id); }
    const std::map<std::string,TextureAsset>& entries() const { return textures; }
private:
    std::map<std::string,TextureAsset> textures;
};
// Lua source text, read when the scene loads; the runtime compiles it (core stays Lua-free).
struct ScriptAsset { std::filesystem::path source; std::shared_ptr<const std::string> code; };
class ScriptAssets {
public:
    static constexpr size_t maxBytes=1024*1024,maxScripts=256;
    void load(std::string id,const std::filesystem::path& path);
    // In-memory scripts (tests, generated content); `source` may be empty.
    void set(std::string id,std::string code,std::filesystem::path source={});
    // Re-read every file-backed script, e.g. before entering Play.
    void reloadSources();
    const ScriptAsset& get(const std::string& id) const;
    bool contains(const std::string& id) const { return scripts.contains(id); }
    const std::map<std::string,ScriptAsset>& entries() const { return scripts; }
private:
    std::map<std::string,ScriptAsset> scripts;
};
// Particle effect definitions (validated on insertion), shared between scene copies.
using SharedEffect=std::shared_ptr<const EffectDef>;
class EffectAssets {
public:
    void set(std::string id,EffectDef effect);
    bool erase(const std::string& id) { return effects.erase(id)>0; }
    const SharedEffect& get(const std::string& id) const;
    bool contains(const std::string& id) const { return effects.contains(id); }
    const std::map<std::string,SharedEffect>& entries() const { return effects; }
private:
    std::map<std::string,SharedEffect> effects;
};
struct Material {
    glm::vec3 color{1}; float emission=0;
    std::string textureId="builtin:white";
    glm::vec2 uvScale{1};
};
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
