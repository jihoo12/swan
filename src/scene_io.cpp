#include "scene_io.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <unistd.h>
namespace swan {
namespace {
using Json=nlohmann::json;
constexpr size_t maxBytes=16*1024*1024,maxEntities=10000;
void keys(const Json& value,std::initializer_list<std::string_view> allowed) {
    if(!value.is_object()) throw std::invalid_argument("Expected a JSON object");
    for(auto it=value.begin();it!=value.end();++it)
        if(std::find(allowed.begin(),allowed.end(),it.key())==allowed.end())
            throw std::invalid_argument("Unknown scene field: "+it.key());
}
float number(const Json& value) {
    if(!value.is_number()) throw std::invalid_argument("Expected a numeric scene value");
    double n=value.get<double>();
    if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) throw std::invalid_argument("Scene value exceeds finite float range");
    return float(n);
}
glm::vec3 vector(const Json& value) {
    if(!value.is_array() || value.size()!=3) throw std::invalid_argument("Expected a three-component vector");
    return {number(value[0]),number(value[1]),number(value[2])};
}
Json vector(glm::vec3 value) { return Json::array({value.x,value.y,value.z}); }
}
SceneDocument parseScene(std::string_view text,const std::filesystem::path& baseDirectory) {
    if(text.size()>maxBytes) throw std::invalid_argument("Scene file exceeds 16 MiB");
    try {
        const auto root=Json::parse(text);
        keys(root,{"format","version","spawn","materials","meshes","textures","scripts","entities"});
        if(root.at("format")!="swan-scene" || !root.at("version").is_number_integer() || root.at("version")<1 || root.at("version")>6)
            throw std::invalid_argument("Unsupported scene format/version");
        SceneDocument document;
        document.spawn=vector(root.at("spawn"));
        const auto& materials=root.at("materials");
        if(!materials.is_object()) throw std::invalid_argument("Materials must be an object");
        for(auto it=materials.begin();it!=materials.end();++it) {
            keys(it.value(),{"color","emission","texture","uv_scale"});
            Material material{vector(it.value().at("color")),number(it.value().at("emission"))};
            if(it.value().contains("texture") || it.value().contains("uv_scale")) {
                if(root.at("version")<3) throw std::invalid_argument("Textured materials require scene version 3");
                material.textureId=it.value().value("texture",std::string("builtin:white"));
                if(it.value().contains("uv_scale")) {
                    const auto& uv=it.value().at("uv_scale");
                    if(!uv.is_array() || uv.size()!=2) throw std::invalid_argument("UV scale requires two components");
                    material.uvScale={number(uv[0]),number(uv[1])};
                }
            }
            document.scene.assets().set(it.key(),material);
        }
        if(root.contains("meshes")) {
            if(root.at("version")==1) throw std::invalid_argument("Mesh assets require scene version 2");
            const auto& meshes=root.at("meshes");
            if(!meshes.is_object()) throw std::invalid_argument("Meshes must be an object");
            auto base=baseDirectory.empty()?std::filesystem::current_path():baseDirectory;
            for(auto it=meshes.begin();it!=meshes.end();++it) {
                keys(it.value(),{"source","part"});
                uint32_t part=0;
                if(it.value().contains("part")) {
                    const auto& value=it.value().at("part");
                    if(root.at("version")<5 || !value.is_number_integer() || value.get<int64_t>()<0 || value.get<int64_t>()>255)
                        throw std::invalid_argument("Mesh part requires version 5 and integer 0..255");
                    part=value.get<uint32_t>();
                }
                auto source=it.value().at("source").get<std::string>();
                if(source.empty()) throw std::invalid_argument("Mesh source cannot be empty");
                auto path=base/std::filesystem::path(source);
                if(root.at("version")<5 && (path.extension()==".gltf" || path.extension()==".glb")) throw std::invalid_argument("glTF requires scene version 5");
                document.scene.meshes().load(it.key(),path,part);
            }
        }
        if(root.contains("textures")) {
            if(root.at("version")<3) throw std::invalid_argument("Texture assets require scene version 3");
            const auto& textures=root.at("textures");
            if(!textures.is_object() || textures.size()>256) throw std::invalid_argument("Textures must be an object with at most 256 entries");
            auto base=baseDirectory.empty()?std::filesystem::current_path():baseDirectory;
            for(auto it=textures.begin();it!=textures.end();++it) {
                keys(it.value(),{"source"}); auto source=it.value().at("source").get<std::string>();
                if(source.empty()) throw std::invalid_argument("Texture source cannot be empty");
                document.scene.textures().load(it.key(),base/std::filesystem::path(source));
            }
        }
        for(const auto& [id,material]:document.scene.assets().entries()) {
            (void)id; if(!document.scene.textures().contains(material.textureId)) throw std::invalid_argument("Unknown texture asset: "+material.textureId);
        }
        if(root.contains("scripts")) {
            if(root.at("version")<6) throw std::invalid_argument("Script assets require scene version 6");
            const auto& scripts=root.at("scripts");
            if(!scripts.is_object() || scripts.size()>ScriptAssets::maxScripts) throw std::invalid_argument("Scripts must be an object with at most 256 entries");
            auto base=baseDirectory.empty()?std::filesystem::current_path():baseDirectory;
            for(auto it=scripts.begin();it!=scripts.end();++it) {
                keys(it.value(),{"source"}); auto source=it.value().at("source").get<std::string>();
                if(source.empty()) throw std::invalid_argument("Script source cannot be empty");
                document.scene.scripts().load(it.key(),base/std::filesystem::path(source));
            }
        }
        const auto& entities=root.at("entities");
        if(!entities.is_array() || entities.size()>maxEntities) throw std::invalid_argument("Expected an entity array with at most 10000 entries");
        for(size_t i=0;i<entities.size();++i) {
            try {
                const auto& source=entities[i];
                keys(source,{"id","name","mesh","material","transform","solid","collectible","goal","animation","parent","script","properties"});
                Entity entity;
                entity.key=source.at("id").get<std::string>();
                if(entity.key.empty()) throw std::invalid_argument("Entity ID cannot be empty");
                entity.name=source.at("name").get<std::string>();
                entity.meshId=source.at("mesh").get<std::string>();
                entity.materialId=source.at("material").get<std::string>();
                const auto& transform=source.at("transform"); keys(transform,{"position","scale","yaw"});
                entity.transform={vector(transform.at("position")),vector(transform.at("scale")),number(transform.at("yaw"))};
                entity.solid=source.value("solid",false); entity.collectible=source.value("collectible",false); entity.goal=source.value("goal",false);
                if(source.contains("animation")) {
                    const auto& a=source.at("animation"); keys(a,{"base_height","phase","bob","speed"});
                    entity.animation=Animation{number(a.at("base_height")),number(a.at("phase")),number(a.at("bob")),number(a.at("speed"))};
                }
                if(source.contains("script") || source.contains("properties")) {
                    if(root.at("version")<6) throw std::invalid_argument("Entity scripts require scene version 6");
                    entity.scriptId=source.value("script",std::string());
                    if(source.contains("properties")) {
                        const auto& properties=source.at("properties");
                        if(!properties.is_object()) throw std::invalid_argument("Script properties must be an object");
                        for(auto it=properties.begin();it!=properties.end();++it) {
                            if(it.value().is_boolean()) entity.properties[it.key()]=it.value().get<bool>();
                            else if(it.value().is_number()) entity.properties[it.key()]=double(number(it.value()));
                            else if(it.value().is_string()) entity.properties[it.key()]=it.value().get<std::string>();
                            else throw std::invalid_argument("Script property must be a number, boolean, or string: "+it.key());
                        }
                    }
                }
                document.scene.create(std::move(entity));
            } catch(const std::exception& e) { throw std::invalid_argument("Entity "+std::to_string(i)+": "+e.what()); }
        }
        // Resolve after creating all entities, so file order does not matter.
        for(const auto& source:entities) if(source.contains("parent")) {
            if(root.at("version")<4) throw std::invalid_argument("Hierarchy requires scene version 4");
            auto key=source.at("parent").get<std::string>();
            auto parent=document.scene.find(key);
            if(!document.scene.get(parent)) throw std::invalid_argument("Unknown parent: "+key);
            document.scene.setParent(document.scene.find(source.at("id").get<std::string>()),parent);
        }
        return document;
    } catch(const std::exception& e) { throw std::invalid_argument(std::string("Invalid Swan scene: ")+e.what()); }
}
std::string serializeScene(const SceneDocument& document,const std::filesystem::path& baseDirectory) {
    Json root={{"format","swan-scene"},{"version",6},{"spawn",vector(document.spawn)},
               {"materials",Json::object()},{"meshes",Json::object()},{"textures",Json::object()},{"scripts",Json::object()},{"entities",Json::array()}};
    for(const auto& [id,material]:document.scene.assets().entries())
        root["materials"][id]={{"color",vector(material.color)},{"emission",material.emission},
            {"texture",material.textureId},{"uv_scale",Json::array({material.uvScale.x,material.uvScale.y})}};
    auto base=baseDirectory.empty()?std::filesystem::current_path():std::filesystem::absolute(baseDirectory);
    for(const auto& [id,mesh]:document.scene.meshes().entries()) if(!mesh.source.empty()) {
        std::error_code error;
        auto relative=std::filesystem::relative(mesh.source,base,error);
        root["meshes"][id]={{"source",(error?mesh.source:relative).generic_string()}};
        if(mesh.source.extension()==".gltf" || mesh.source.extension()==".glb") root["meshes"][id]["part"]=mesh.part;
    }
    for(const auto& [id,texture]:document.scene.textures().entries()) if(!texture.source.empty()) {
        std::error_code error; auto relative=std::filesystem::relative(texture.source,base,error);
        root["textures"][id]={{"source",(error?texture.source:relative).generic_string()}};
    }
    for(const auto& [id,script]:document.scene.scripts().entries()) {
        if(script.source.empty()) throw std::invalid_argument("Script "+id+" exists only in memory and cannot be saved");
        std::error_code error; auto relative=std::filesystem::relative(script.source,base,error);
        root["scripts"][id]={{"source",(error?script.source:relative).generic_string()}};
    }
    if(root["scripts"].empty()) root.erase("scripts");
    for(auto id:document.scene.entities()) {
        const auto& e=*document.scene.get(id);
        Json entity={{"id",e.key},{"name",e.name},{"mesh",e.meshId},{"material",e.materialId},
            {"transform",{{"position",vector(e.transform.position)},{"scale",vector(e.transform.scale)},{"yaw",e.transform.yaw}}},
            {"solid",e.solid},{"collectible",e.collectible},{"goal",e.goal}};
        if(auto parent=document.scene.parent(id)) entity["parent"]=document.scene.get(*parent)->key;
        if(!e.scriptId.empty()) entity["script"]=e.scriptId;
        if(!e.properties.empty()) {
            entity["properties"]=Json::object();
            for(const auto& [name,value]:e.properties) std::visit([&](const auto& v){entity["properties"][name]=v;},value);
        }
        if(e.animation) {
            const auto& a=*e.animation;
            entity["animation"]={{"base_height",a.baseHeight},{"phase",a.phase},{"bob",a.bob},{"speed",a.speed}};
        }
        root["entities"].push_back(std::move(entity));
    }
    // Scene entities are mutable at runtime. Revalidate before touching any file.
    auto text=root.dump(2)+"\n";
    parseScene(text,baseDirectory);
    return text;
}
SceneDocument loadScene(const std::filesystem::path& path) {
    try {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot open scene file");
        auto size=input.tellg();
        if(size<0 || size>static_cast<std::streamoff>(maxBytes)) throw std::runtime_error("Scene file exceeds 16 MiB or cannot be sized");
        std::string text(static_cast<size_t>(size),'\0'); input.seekg(0); input.read(text.data(),size);
        if(!input) throw std::runtime_error("Cannot read scene file");
        return parseScene(text,std::filesystem::absolute(path).parent_path());
    } catch(const std::exception& e) { throw std::runtime_error(path.string()+": "+e.what()); }
}
void saveScene(const std::filesystem::path& path,const SceneDocument& document) {
    auto text=serializeScene(document,std::filesystem::absolute(path).parent_path());
    std::string pattern=path.string()+".tmp-XXXXXX";
    std::vector<char> name(pattern.begin(),pattern.end()); name.push_back('\0');
    int descriptor=mkstemp(name.data());
    if(descriptor<0) throw std::runtime_error(path.string()+": Cannot create temporary scene file: "+std::strerror(errno));
    std::filesystem::path temporary=name.data();
    try {
        size_t offset=0;
        while(offset<text.size()) {
            auto count=::write(descriptor,text.data()+offset,text.size()-offset);
            if(count<0 && errno==EINTR) continue;
            if(count<=0) throw std::runtime_error("Cannot write scene file");
            offset+=static_cast<size_t>(count);
        }
        int result=::close(descriptor); descriptor=-1;
        if(result!=0) throw std::runtime_error("Cannot close scene file");
        std::filesystem::rename(temporary,path);
    } catch(const std::exception& e) {
        if(descriptor>=0) ::close(descriptor);
        std::error_code ec; std::filesystem::remove(temporary,ec);
        throw std::runtime_error(path.string()+": "+e.what());
    }
}
}
