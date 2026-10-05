#include "assets.hpp"
#include <cmath>
#include <fstream>
#include <sstream>
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
namespace {
std::string readScript(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary|std::ios::ate);
    if(!input) throw std::invalid_argument("Cannot open script: "+path.string());
    auto size=input.tellg();
    if(size<0 || size>std::streamoff(ScriptAssets::maxBytes)) throw std::invalid_argument("Script exceeds 1 MiB: "+path.string());
    std::string code(size_t(size),'\0');input.seekg(0);input.read(code.data(),size);
    if(!input) throw std::invalid_argument("Cannot read script: "+path.string());
    return code;
}
void checkScriptId(const std::string& id) {
    if(id.empty() || id.size()>256 || id.starts_with("builtin:")) throw std::invalid_argument("Invalid script asset ID: "+id);
}
}
void ScriptAssets::load(std::string id,const std::filesystem::path& path) {
    auto source=std::filesystem::canonical(path);
    set(std::move(id),readScript(source),source);
}
void ScriptAssets::set(std::string id,std::string code,std::filesystem::path source) {
    checkScriptId(id);
    if(code.size()>maxBytes) throw std::invalid_argument("Script exceeds 1 MiB: "+id);
    if(!scripts.contains(id) && scripts.size()>=maxScripts) throw std::invalid_argument("At most 256 scripts are supported");
    scripts.insert_or_assign(std::move(id),ScriptAsset{std::move(source),std::make_shared<const std::string>(std::move(code))});
}
void ScriptAssets::reloadSources() {
    // Read everything first so a missing file leaves all scripts unchanged.
    std::map<std::string,std::string> fresh;
    for(const auto& [id,script]:scripts) if(!script.source.empty()) fresh[id]=readScript(script.source);
    for(auto& [id,code]:fresh) scripts.at(id).code=std::make_shared<const std::string>(std::move(code));
}
const ScriptAsset& ScriptAssets::get(const std::string& id) const {
    auto found=scripts.find(id);
    if(found==scripts.end()) throw std::invalid_argument("Unknown script asset: "+id);
    return found->second;
}
void EffectAssets::set(std::string id,EffectDef effect) {
    if(id.empty() || id.size()>256 || id.starts_with("builtin:")) throw std::invalid_argument("Invalid effect asset ID: "+id);
    if(!effects.contains(id) && effects.size()>=maxEffects) throw std::invalid_argument("At most 256 effects are supported");
    try {validateEffect(effect);} catch(const std::exception& e) {throw std::invalid_argument("Effect "+id+": "+e.what());}
    effects.insert_or_assign(std::move(id),std::make_shared<const EffectDef>(std::move(effect)));
}
const SharedEffect& EffectAssets::get(const std::string& id) const {
    auto found=effects.find(id);
    if(found==effects.end()) throw std::invalid_argument("Unknown effect asset: "+id);
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
