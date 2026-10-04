#include "assets.hpp"
#include <cmath>
#include <stdexcept>
namespace swan {
MeshAssets::MeshAssets() { meshes.emplace("builtin:cube",MeshAsset{{},cubeMesh()}); }
void MeshAssets::load(std::string id,const std::filesystem::path& path,uint32_t part) {
    if(id.empty() || id.size()>256 || id.starts_with("builtin:")) throw std::invalid_argument("Invalid mesh asset ID: "+id);
    auto source=std::filesystem::canonical(path);
    // Deduplicate by resolved path within a catalog; reload builds a fresh catalog.
    SharedMesh data;
    for(const auto& [name,existing]:meshes) {
        (void)name;
        if(existing.source==source && existing.part==part) { data=existing.data; break; }
    }
    if(!data) {
        auto extension=source.extension().string();
        if(extension==".gltf" || extension==".glb") {
            auto found=models.find(source);
            if(found==models.end()) found=models.emplace(source,loadStaticGltf(source)).first;
            if(part>=found->second.parts.size()) throw std::invalid_argument("glTF part index out of range");
            data=found->second.parts[part];
        } else {
            if(part!=0) throw std::invalid_argument("OBJ does not support part selection");
            data=loadObjMesh(source);
        }
    }
    meshes.insert_or_assign(std::move(id),MeshAsset{std::move(source),std::move(data),part});
}
const MeshAsset& MeshAssets::get(const std::string& id) const {
    auto found=meshes.find(id);
    if(found==meshes.end()) throw std::invalid_argument("Unknown mesh asset: "+id);
    return found->second;
}
TextureAssets::TextureAssets() { textures.emplace("builtin:white",TextureAsset{{},whiteTexture()}); }
void TextureAssets::load(std::string id,const std::filesystem::path& path) {
    if(id.empty() || id.size()>256 || id.starts_with("builtin:")) throw std::invalid_argument("Invalid texture asset ID: "+id);
    if(!textures.contains(id) && textures.size()>=257) throw std::invalid_argument("At most 256 imported textures are supported");
    auto source=std::filesystem::canonical(path); SharedTexture data;
    for(const auto& [name,existing]:textures) { (void)name; if(existing.source==source) {data=existing.data; break;} }
    if(!data) data=loadPngTexture(source);
    textures.insert_or_assign(std::move(id),TextureAsset{std::move(source),std::move(data)});
}
const TextureAsset& TextureAssets::get(const std::string& id) const {
    auto found=textures.find(id);
    if(found==textures.end()) throw std::invalid_argument("Unknown texture asset: "+id);
    return found->second;
}
MaterialAssets::MaterialAssets() { set("default",{}); }
void MaterialAssets::set(std::string id,Material material) {
    if(id.empty() || id.size()>256) throw std::invalid_argument("Material ID must contain 1..256 characters");
    for(int i=0;i<3;++i) if(!std::isfinite(material.color[i]) || material.color[i]<0)
        throw std::invalid_argument("Material colors must be finite and nonnegative");
    if(!std::isfinite(material.emission) || material.emission<0)
        throw std::invalid_argument("Material emission must be finite and nonnegative");
    if(material.textureId.empty()) throw std::invalid_argument("Material texture ID cannot be empty");
    for(int i=0;i<2;++i) if(!std::isfinite(material.uvScale[i]) || material.uvScale[i]<=0)
        throw std::invalid_argument("Material UV scale must be positive and finite");
    materials.insert_or_assign(std::move(id),material);
}
const Material& MaterialAssets::get(const std::string& id) const {
    auto found=materials.find(id);
    if(found==materials.end()) throw std::invalid_argument("Unknown material asset: "+id);
    return found->second;
}
}
