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
SceneDocument parseScene(std::string_view text) {
    if(text.size()>maxBytes) throw std::invalid_argument("Scene file exceeds 16 MiB");
    try {
        const auto root=Json::parse(text);
        keys(root,{"format","version","spawn","materials","entities"});
        if(root.at("format")!="swan-scene" || !root.at("version").is_number_integer() || root.at("version")!=1)
            throw std::invalid_argument("Unsupported scene format/version");
        SceneDocument document;
        document.spawn=vector(root.at("spawn"));
        const auto& materials=root.at("materials");
        if(!materials.is_object()) throw std::invalid_argument("Materials must be an object");
        for(auto it=materials.begin();it!=materials.end();++it) {
            keys(it.value(),{"color","emission"});
            document.scene.assets().set(it.key(),{vector(it.value().at("color")),number(it.value().at("emission"))});
        }
        const auto& entities=root.at("entities");
        if(!entities.is_array() || entities.size()>maxEntities) throw std::invalid_argument("Expected an entity array with at most 10000 entries");
        for(size_t i=0;i<entities.size();++i) {
            try {
                const auto& source=entities[i];
                keys(source,{"id","name","mesh","material","transform","solid","collectible","goal","animation"});
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
                document.scene.create(std::move(entity));
            } catch(const std::exception& e) { throw std::invalid_argument("Entity "+std::to_string(i)+": "+e.what()); }
        }
        return document;
    } catch(const std::exception& e) { throw std::invalid_argument(std::string("Invalid Swan scene: ")+e.what()); }
}
std::string serializeScene(const SceneDocument& document) {
    Json root={{"format","swan-scene"},{"version",1},{"spawn",vector(document.spawn)},
               {"materials",Json::object()},{"entities",Json::array()}};
    for(const auto& [id,material]:document.scene.assets().entries())
        root["materials"][id]={{"color",vector(material.color)},{"emission",material.emission}};
    for(auto id:document.scene.entities()) {
        const auto& e=*document.scene.get(id);
        Json entity={{"id",e.key},{"name",e.name},{"mesh",e.meshId},{"material",e.materialId},
            {"transform",{{"position",vector(e.transform.position)},{"scale",vector(e.transform.scale)},{"yaw",e.transform.yaw}}},
            {"solid",e.solid},{"collectible",e.collectible},{"goal",e.goal}};
        if(e.animation) {
            const auto& a=*e.animation;
            entity["animation"]={{"base_height",a.baseHeight},{"phase",a.phase},{"bob",a.bob},{"speed",a.speed}};
        }
        root["entities"].push_back(std::move(entity));
    }
    // Scene entities are mutable at runtime. Revalidate before touching any file.
    auto text=root.dump(2)+"\n";
    parseScene(text);
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
        return parseScene(text);
    } catch(const std::exception& e) { throw std::runtime_error(path.string()+": "+e.what()); }
}
void saveScene(const std::filesystem::path& path,const SceneDocument& document) {
    auto text=serializeScene(document);
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
