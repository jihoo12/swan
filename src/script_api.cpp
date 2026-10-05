#include "script_api.hpp"
#include "fx_capture.hpp"
#include "fx_json.hpp"
#include "image.hpp"
#include "script_lua.hpp"
#include "simulation.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
namespace swan {
namespace {
using lua::protect;
constexpr const char* DocumentType="Document";
constexpr const char* SimulationType="Simulation";
constexpr const char* PreviewType="Preview";
constexpr const char* RendererType="swan.ImageRenderer";
constexpr const char* RendererKey="swan.renderer"; // Registry slot holding the renderer object.
using Json=nlohmann::json;
struct DocumentRef {
    std::shared_ptr<EditorDocument> owned; // Empty for borrowed documents (e.g. the editor's).
    EditorDocument* document=nullptr;
    std::filesystem::path path;
};
struct SimulationRef { std::shared_ptr<Simulation> simulation; };
struct PreviewRef { std::shared_ptr<FxPlayer> player; };
DocumentRef& documentRef(lua_State* L) {return lua::object<DocumentRef>(L,1,DocumentType);}
EditorDocument& document(lua_State* L) {return *documentRef(L).document;}
Simulation& simulation(lua_State* L) {return *lua::object<SimulationRef>(L,1,SimulationType).simulation;}
FxPlayer& preview(lua_State* L) {return *lua::object<PreviewRef>(L,1,PreviewType).player;}
struct Pop { lua_State* L; int count=1; ~Pop() {lua_pop(L,count);} };
void pushValue(lua_State* L,const ScriptValue& value) {
    std::visit([&](const auto& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,double>) lua_pushnumber(L,v);
        else if constexpr(std::is_same_v<T,bool>) lua_pushboolean(L,v);
        else lua::push(L,v);
    },value);
}
ScriptProperties readProperties(lua_State* L,int table) {
    ScriptProperties result;
    if(lua_isnoneornil(L,table)) return result;
    if(lua_type(L,table)!=LUA_TTABLE) throw std::invalid_argument("properties must be a table");
    table=lua_absindex(L,table);
    lua_pushnil(L);
    while(lua_next(L,table)) {
        Pop pop{L};
        if(lua_type(L,-2)!=LUA_TSTRING) throw std::invalid_argument("property names must be strings");
        std::string name=lua_tostring(L,-2);
        switch(lua_type(L,-1)) {
        case LUA_TNUMBER: result[name]=lua_tonumber(L,-1);break;
        case LUA_TBOOLEAN: result[name]=bool(lua_toboolean(L,-1));break;
        case LUA_TSTRING: result[name]=lua::string(L,-1,name.c_str());break;
        default: throw std::invalid_argument("property "+name+" must be a number, boolean, or string");
        }
    }
    return result;
}
ScriptProperties fieldProperties(lua_State* L,int table,const char* name) {
    lua::pushField(L,table,name);Pop pop{L};
    return readProperties(L,-1);
}
// Lua tables <-> JSON, so effects and timelines share the scene file's schema and validation.
// Sequences (keys 1..n) become arrays, string-keyed tables objects, vec2/vec3 arrays.
Json toJson(lua_State* L,int index,int depth=0) {
    if(depth>32) throw std::invalid_argument("table nesting exceeds 32 levels");
    index=lua_absindex(L,index);
    switch(lua_type(L,index)) {
    case LUA_TNIL: return nullptr;
    case LUA_TBOOLEAN: return bool(lua_toboolean(L,index));
    case LUA_TNUMBER: {
        if(lua_isinteger(L,index)) return lua_tointeger(L,index);
        double value=lua_tonumber(L,index);
        if(!std::isfinite(value)) throw std::invalid_argument("numbers must be finite");
        return value;
    }
    case LUA_TSTRING: return lua::string(L,index,"string");
    case LUA_TUSERDATA: {
        if(luaL_testudata(L,index,"vec3")) {auto v=lua::vec3(L,index,"vec3");return Json::array({v.x,v.y,v.z});}
        if(luaL_testudata(L,index,"vec2")) {auto v=lua::vec2(L,index,"vec2");return Json::array({v.x,v.y});}
        throw std::invalid_argument("cannot convert this userdata to data");
    }
    case LUA_TTABLE: {
        size_t entries=0;bool sequence=true;
        lua_pushnil(L);
        while(lua_next(L,index)) {
            ++entries;
            if(!lua_isinteger(L,-2) || lua_tointeger(L,-2)<1) sequence=false;
            lua_pop(L,1);
        }
        if(sequence) {
            for(size_t i=1;i<=entries;++i) {
                bool present=lua_rawgeti(L,index,lua_Integer(i))!=LUA_TNIL;lua_pop(L,1);
                if(!present) throw std::invalid_argument("arrays cannot have holes");
            }
            Json array=Json::array();
            for(size_t i=1;i<=entries;++i) {lua_rawgeti(L,index,lua_Integer(i));Pop pop{L};array.push_back(toJson(L,-1,depth+1));}
            return array;
        }
        Json object=Json::object();
        lua_pushnil(L);
        while(lua_next(L,index)) {
            Pop pop{L};
            if(lua_type(L,-2)!=LUA_TSTRING) throw std::invalid_argument("tables must be arrays or have string keys");
            std::string key=lua_tostring(L,-2);
            object[key]=toJson(L,-1,depth+1);
        }
        return object;
    }
    default: throw std::invalid_argument(std::string("cannot convert a ")+lua_typename(L,lua_type(L,index))+" to data");
    }
}
void pushJson(lua_State* L,const Json& value) {
    switch(value.type()) {
    case Json::value_t::object:
        lua_createtable(L,0,int(value.size()));
        for(auto it=value.begin();it!=value.end();++it) {pushJson(L,it.value());lua_setfield(L,-2,it.key().c_str());}
        break;
    case Json::value_t::array:
        lua_createtable(L,int(value.size()),0);
        for(size_t i=0;i<value.size();++i) {pushJson(L,value[i]);lua_rawseti(L,-2,lua_Integer(i+1));}
        break;
    case Json::value_t::boolean: lua_pushboolean(L,value.get<bool>());break;
    case Json::value_t::number_integer: case Json::value_t::number_unsigned: lua_pushinteger(L,value.get<lua_Integer>());break;
    case Json::value_t::number_float: lua_pushnumber(L,value.get<double>());break;
    case Json::value_t::string: lua::push(L,value.get<std::string>());break;
    default: lua_pushnil(L);
    }
}
EffectDef readEffect(lua_State* L,int index) {
    if(lua_type(L,index)!=LUA_TTABLE) throw std::invalid_argument("an effect is a table: { emitters = { {...}, ... } }");
    return effectFromJson(toJson(L,index));
}
void pushEntity(lua_State* L,const Scene& scene,EntityId id) {
    const auto& e=*scene.get(id);
    lua_createtable(L,0,16);
    int t=lua_gettop(L);
    lua::push(L,e.key);lua::set(L,t,"key");lua::push(L,e.name);lua::set(L,t,"name");
    lua::push(L,e.meshId);lua::set(L,t,"mesh");lua::push(L,e.materialId);lua::set(L,t,"material");
    lua::push(L,e.transform.position);lua::set(L,t,"position");lua::push(L,e.transform.scale);lua::set(L,t,"scale");
    lua_pushnumber(L,e.transform.yaw);lua::set(L,t,"yaw");
    lua::push(L,scene.worldTransform(id).position);lua::set(L,t,"world_position");
    lua_pushboolean(L,e.solid);lua::set(L,t,"solid");lua_pushboolean(L,e.collectible);lua::set(L,t,"collectible");lua_pushboolean(L,e.goal);lua::set(L,t,"goal");
    if(auto parent=scene.parent(id)) {lua::push(L,scene.get(*parent)->key);lua::set(L,t,"parent");}
    if(!e.scriptId.empty()) {lua::push(L,e.scriptId);lua::set(L,t,"script");}
    if(!e.effectId.empty()) {lua::push(L,e.effectId);lua::set(L,t,"effect");}
    lua_newtable(L);
    for(const auto& [name,value]:e.properties) {pushValue(L,value);lua_setfield(L,-2,name.c_str());}
    lua::set(L,t,"properties");
}
void pushEntities(lua_State* L,const Scene& scene) {
    auto ids=scene.entities();
    lua_createtable(L,int(ids.size()),0);
    for(size_t i=0;i<ids.size();++i) {pushEntity(L,scene,ids[i]);lua_rawseti(L,-2,lua_Integer(i+1));}
}
void pushStrings(lua_State* L,const std::vector<std::string>& values) {
    lua_createtable(L,int(values.size()),0);
    for(size_t i=0;i<values.size();++i) {lua::push(L,values[i]);lua_rawseti(L,-2,lua_Integer(i+1));}
}
const Entity& requireEntity(const Scene& scene,const std::string& key) {
    const auto* entity=scene.get(scene.find(key));
    if(!entity) throw std::invalid_argument("Unknown entity: "+key);
    return *entity;
}
Input readInput(lua_State* L,int index) {
    Input input;
    if(lua_isnoneornil(L,index)) return input;
    if(lua_type(L,index)!=LUA_TTABLE) throw std::invalid_argument("input must be a table");
    if(auto v=lua::fieldVec2(L,index,"move")) input.move=*v;
    if(auto v=lua::fieldVec2(L,index,"look")) input.look=*v;
    input.vertical=lua::fieldNumber(L,index,"vertical",0);
    input.sprint=lua::fieldBool(L,index,"sprint",false);input.jump=lua::fieldBool(L,index,"jump",false);
    input.interact=lua::fieldBool(L,index,"interact",false);input.pause=lua::fieldBool(L,index,"pause",false);
    input.reset=lua::fieldBool(L,index,"reset",false);input.toggleFlight=lua::fieldBool(L,index,"toggle_flight",false);
    input.toggleCamera=lua::fieldBool(L,index,"toggle_camera",false);
    return input;
}
void pushMaterial(lua_State* L,const Material& m) {
    lua_createtable(L,0,4);
    lua::push(L,m.color);lua_setfield(L,-2,"color");lua_pushnumber(L,m.emission);lua_setfield(L,-2,"emission");
    lua::push(L,m.textureId);lua_setfield(L,-2,"texture");lua::push(L,m.uvScale);lua_setfield(L,-2,"uv_scale");
}
Material readMaterial(lua_State* L,int table,Material m) {
    if(auto v=lua::fieldVec3(L,table,"color")) m.color=*v;
    m.emission=lua::fieldNumber(L,table,"emission",m.emission);
    m.textureId=lua::fieldString(L,table,"texture",m.textureId);
    if(auto v=lua::fieldVec2(L,table,"uv_scale")) m.uvScale=*v;
    return m;
}
const ImageRenderer* rendererOf(lua_State* L) {
    lua_getfield(L,LUA_REGISTRYINDEX,RendererKey);
    auto* renderer=static_cast<ImageRenderer*>(luaL_testudata(L,-1,RendererType));
    lua_pop(L,1);
    return renderer && *renderer?renderer:nullptr;
}
const ImageRenderer& requireRenderer(lua_State* L) {
    if(const auto* renderer=rendererOf(L)) return *renderer;
    throw std::runtime_error("rendering needs the GPU renderer: run this script with `swan script` (not available in this host)");
}
CaptureOptions readCapture(lua_State* L,int table,CaptureOptions options) {
    if(lua_isnoneornil(L,table)) return options;
    if(lua_type(L,table)!=LUA_TTABLE) throw std::invalid_argument("render options must be a table");
    options.size.x=uint32_t(lua::fieldNumber(L,table,"width",float(options.size.x)));
    options.size.y=uint32_t(lua::fieldNumber(L,table,"height",float(options.size.y)));
    options.supersample=uint32_t(lua::fieldNumber(L,table,"supersample",float(options.supersample)));
    if(lua::pushField(L,table,"camera")==LUA_TTABLE) {
        Pop pop{L};
        auto position=lua::fieldVec3(L,-1,"position");
        if(!position) throw std::invalid_argument("camera needs a position");
        float fov=lua::fieldNumber(L,-1,"fov",55);
        if(fov<1 || fov>170) throw std::invalid_argument("camera fov must be within 1..170 degrees");
        if(auto target=lua::fieldVec3(L,-1,"target")) options.camera=lookAt(*position,*target,fov);
        else {
            Camera camera;camera.position=*position;camera.fov=fov;
            camera.yaw=lua::fieldNumber(L,-1,"yaw",camera.yaw);camera.pitch=lua::fieldNumber(L,-1,"pitch",camera.pitch);
            options.camera=camera;
        }
    } else lua_pop(L,1);
    return options;
}
void pushCamera(lua_State* L,const Camera& camera) {
    lua_createtable(L,0,5);
    lua::push(L,camera.position);lua_setfield(L,-2,"position");
    lua::push(L,camera.position+forward(camera.yaw,camera.pitch));lua_setfield(L,-2,"target");
    lua_pushnumber(L,camera.fov);lua_setfield(L,-2,"fov");
    lua_pushnumber(L,camera.yaw);lua_setfield(L,-2,"yaw");
    lua_pushnumber(L,camera.pitch);lua_setfield(L,-2,"pitch");
}
void pushStats(lua_State* L,const FxPlayer& player) {
    auto stats=player.stats();
    lua_createtable(L,0,8);
    lua_pushnumber(L,player.time());lua_setfield(L,-2,"time");
    lua_pushinteger(L,lua_Integer(stats.particles));lua_setfield(L,-2,"particles");
    lua_pushinteger(L,lua_Integer(stats.instances));lua_setfield(L,-2,"instances");
    lua_pushinteger(L,lua_Integer(stats.spawned));lua_setfield(L,-2,"spawned");
    lua_pushinteger(L,lua_Integer(stats.dropped));lua_setfield(L,-2,"dropped");
    if(stats.particles) {
        lua_createtable(L,0,2);
        lua::push(L,stats.minimum);lua_setfield(L,-2,"min");lua::push(L,stats.maximum);lua_setfield(L,-2,"max");
        lua_setfield(L,-2,"bounds");
    }
    lua_createtable(L,int(stats.emitters.size()),0);
    for(size_t i=0;i<stats.emitters.size();++i) {
        const auto& e=stats.emitters[i];
        lua_createtable(L,0,8);
        lua::push(L,e.effect);lua_setfield(L,-2,"effect");lua::push(L,e.emitter);lua_setfield(L,-2,"emitter");
        if(!e.entity.empty()) {lua::push(L,e.entity);lua_setfield(L,-2,"entity");}
        lua_pushinteger(L,lua_Integer(e.particles));lua_setfield(L,-2,"particles");
        lua_pushinteger(L,lua_Integer(e.spawned));lua_setfield(L,-2,"spawned");
        lua_pushinteger(L,lua_Integer(e.dropped));lua_setfield(L,-2,"dropped");
        if(e.particles) {lua::push(L,e.minimum);lua_setfield(L,-2,"min");lua::push(L,e.maximum);lua_setfield(L,-2,"max");}
        lua_rawseti(L,-2,lua_Integer(i+1));
    }
    lua_setfield(L,-2,"emitters");
}
std::string label(const Entity& e) {return e.name.empty()?e.key:e.name;}
int bindTypes(lua_State* L) {
    if(luaL_getmetatable(L,DocumentType)!=LUA_TNIL) {lua_pop(L,1);return 0;}
    lua_pop(L,1);
    lua::defineClass(L,DocumentType,{
        {"select",[](lua_State* L){return protect(L,[&]{document(L).select(lua::none(L,2)?std::string():lua::string(L,2,"key"));return 0;});}},
        {"entities",[](lua_State* L){return protect(L,[&]{pushEntities(L,document(L).document().scene);return 1;});}},
        {"entity",[](lua_State* L){return protect(L,[&]{
            const auto& scene=document(L).document().scene;auto id=scene.find(lua::string(L,2,"key"));
            if(scene.get(id)) pushEntity(L,scene,id); else lua_pushnil(L);
            return 1;});}},
        {"create",[](lua_State* L){return protect(L,[&]{
            Entity e;e.name="Entity";std::optional<std::string> parent;
            if(!lua::none(L,2)) {
                if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("create() takes a table");
                e.key=lua::fieldString(L,2,"key","");e.name=lua::fieldString(L,2,"name","Entity");
                e.meshId=lua::fieldString(L,2,"mesh","builtin:cube");e.materialId=lua::fieldString(L,2,"material","default");
                if(auto v=lua::fieldVec3(L,2,"position")) e.transform.position=*v;
                if(auto v=lua::fieldVec3(L,2,"scale")) e.transform.scale=*v;
                e.transform.yaw=lua::fieldNumber(L,2,"yaw",0);
                e.solid=lua::fieldBool(L,2,"solid",false);e.collectible=lua::fieldBool(L,2,"collectible",false);e.goal=lua::fieldBool(L,2,"goal",false);
                e.scriptId=lua::fieldString(L,2,"script","");e.properties=fieldProperties(L,2,"properties");
                e.effectId=lua::fieldString(L,2,"effect","");
                if(lua::present(L,2,"parent")) parent=lua::fieldString(L,2,"parent","");
            }
            auto& doc=document(L);
            doc.apply(CreateEntity{e,parent});
            lua::push(L,doc.selection());return 1;});}},
        {"set",[](lua_State* L){return protect(L,[&]{
            auto& doc=document(L);auto key=lua::string(L,2,"key");
            if(lua_type(L,3)!=LUA_TTABLE) throw std::invalid_argument("set() takes a table of changes");
            auto e=requireEntity(doc.document().scene,key);
            std::vector<SceneEdit> edits;
            auto t=e.transform;
            if(auto v=lua::fieldVec3(L,3,"position")) t.position=*v;
            if(auto v=lua::fieldVec3(L,3,"scale")) t.scale=*v;
            t.yaw=lua::fieldNumber(L,3,"yaw",t.yaw);
            if(lua::present(L,3,"position") || lua::present(L,3,"scale") || lua::present(L,3,"yaw")) edits.push_back(SetTransform{key,t});
            bool props=false;
            for(const char* field:{"name","mesh","material","solid","collectible","goal"}) props|=lua::present(L,3,field);
            if(props) {
                e.name=lua::fieldString(L,3,"name",e.name);e.meshId=lua::fieldString(L,3,"mesh",e.meshId);e.materialId=lua::fieldString(L,3,"material",e.materialId);
                e.solid=lua::fieldBool(L,3,"solid",e.solid);e.collectible=lua::fieldBool(L,3,"collectible",e.collectible);e.goal=lua::fieldBool(L,3,"goal",e.goal);
                edits.push_back(SetEntityProperties{key,e.name,e.meshId,e.materialId,e.solid,e.collectible,e.goal});
            }
            if(lua::present(L,3,"script") || lua::present(L,3,"properties")) {
                // script = false removes the behaviour.
                std::string script=e.scriptId;
                if(lua::pushField(L,3,"script")==LUA_TBOOLEAN) {if(lua_toboolean(L,-1)) {lua_pop(L,1);throw std::invalid_argument("script must be an ID or false");} script.clear();}
                else if(lua_type(L,-1)==LUA_TSTRING) script=lua_tostring(L,-1);
                lua_pop(L,1);
                auto properties=lua::present(L,3,"properties")?fieldProperties(L,3,"properties"):e.properties;
                edits.push_back(SetEntityScript{key,script,properties});
            }
            if(lua::present(L,3,"effect")) {
                // effect = false detaches the effect.
                std::string effect;
                if(lua::pushField(L,3,"effect")==LUA_TBOOLEAN) {bool on=lua_toboolean(L,-1);lua_pop(L,1);if(on) throw std::invalid_argument("effect must be an ID or false");}
                else {struct P { lua_State* L; ~P() {lua_pop(L,1);} } pop{L};effect=lua::string(L,-1,"effect");}
                edits.push_back(SetEntityEffect{key,effect});
            }
            if(lua::present(L,3,"parent")) {
                std::optional<std::string> parent;
                if(lua::pushField(L,3,"parent")==LUA_TSTRING) parent=lua_tostring(L,-1);
                lua_pop(L,1);
                edits.push_back(SetParent{key,parent});
            }
            if(!edits.empty()) doc.apply(edits,"Set "+label(e));
            return 0;});}},
        {"reparent",[](lua_State* L){return protect(L,[&]{
            // Keeps the world placement.
            auto& doc=document(L);auto key=lua::string(L,2,"key");
            std::optional<std::string> parent;if(!lua::none(L,3)) parent=lua::string(L,3,"parent");
            const auto& scene=doc.document().scene;
            requireEntity(scene,key);
            auto world=scene.worldTransform(scene.find(key));
            if(parent) requireEntity(scene,*parent);
            auto local=parent?relativeTransform(scene.worldTransform(scene.find(*parent)),world):world;
            std::vector<SceneEdit> edits{SetParent{key,parent},SetTransform{key,local}};
            doc.apply(edits,"Reparent "+key);return 0;});}},
        {"delete",[](lua_State* L){return protect(L,[&]{document(L).apply(DeleteEntity{lua::string(L,2,"key")});return 0;});}},
        {"materials",[](lua_State* L){return protect(L,[&]{
            lua_newtable(L);
            for(const auto& [id,m]:document(L).document().scene.assets().entries()) {pushMaterial(L,m);lua_setfield(L,-2,id.c_str());}
            return 1;});}},
        {"set_material",[](lua_State* L){return protect(L,[&]{
            auto& doc=document(L);auto id=lua::string(L,2,"id");
            if(lua_type(L,3)!=LUA_TTABLE) throw std::invalid_argument("set_material() takes a table of changes");
            doc.apply(SetMaterial{id,readMaterial(L,3,doc.document().scene.assets().get(id))});return 0;});}},
        {"create_material",[](lua_State* L){return protect(L,[&]{
            auto& doc=document(L);auto id=lua::string(L,2,"id");Material m;
            if(lua_type(L,3)==LUA_TTABLE) {
                if(lua::present(L,3,"base")) m=doc.document().scene.assets().get(lua::fieldString(L,3,"base",""));
                m=readMaterial(L,3,m);
            }
            doc.apply(CreateMaterial{id,m});return 0;});}},
        {"scripts",[](lua_State* L){return protect(L,[&]{
            lua_newtable(L);
            for(const auto& [id,script]:document(L).document().scene.scripts().entries()) {lua::push(L,script.source.string());lua_setfield(L,-2,id.c_str());}
            return 1;});}},
        {"add_script",[](lua_State* L){return protect(L,[&]{document(L).apply(AddScript{lua::string(L,2,"id"),lua::string(L,3,"path")});return 0;});}},
        {"effects",[](lua_State* L){return protect(L,[&]{
            lua_newtable(L);
            for(const auto& [id,effect]:document(L).document().scene.effects().entries()) {pushJson(L,effectToJson(*effect));lua_setfield(L,-2,id.c_str());}
            return 1;});}},
        {"effect",[](lua_State* L){return protect(L,[&]{
            const auto& effects=document(L).document().scene.effects();auto id=lua::string(L,2,"id");
            if(effects.contains(id)) pushJson(L,effectToJson(*effects.get(id))); else lua_pushnil(L);
            return 1;});}},
        {"set_effect",[](lua_State* L){return protect(L,[&]{
            auto id=lua::string(L,2,"id");
            auto effect=readEffect(L,3);
            document(L).apply(SetEffect{id,std::move(effect)});return 0;});}},
        {"delete_effect",[](lua_State* L){return protect(L,[&]{document(L).apply(SetEffect{lua::string(L,2,"id"),std::nullopt});return 0;});}},
        {"timeline",[](lua_State* L){return protect(L,[&]{
            // Always a table with tracks/events arrays and the effective duration, for easy scripting.
            const auto& timeline=document(L).document().scene.timeline();
            auto json=timelineToJson(timeline);
            json["duration"]=timeline.length();json["loop"]=timeline.loop;
            for(const char* list:{"tracks","events"}) if(!json.contains(list)) json[list]=Json::array();
            pushJson(L,json);
            return 1;});}},
        {"set_timeline",[](lua_State* L){return protect(L,[&]{
            Timeline timeline;
            if(!lua::none(L,2)) {
                if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("set_timeline() takes a table or nil");
                timeline=timelineFromJson(toJson(L,2));
            }
            document(L).apply(SetTimeline{std::move(timeline)});return 0;});}},
        {"set_track",[](lua_State* L){return protect(L,[&]{
            // Replaces the track for (entity|material, property); keys = nil or {} removes it.
            if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("set_track() takes { entity|material = id, property = name, keys = {...} }");
            auto spec=toJson(L,2);
            if(!spec.is_object()) throw std::invalid_argument("set_track() takes a table with named fields");
            auto& doc=document(L);
            auto json=timelineToJson(doc.document().scene.timeline());
            Json tracks=json.contains("tracks")?json["tracks"]:Json::array();
            auto matches=[&](const Json& track){
                for(const char* kind:{"entity","material"})
                    if(spec.contains(kind)!=track.contains(kind) || (spec.contains(kind) && spec[kind]!=track[kind])) return false;
                return spec.value("property",Json())==track.value("property",Json());
            };
            Json kept=Json::array();
            for(const auto& track:tracks) if(!matches(track)) kept.push_back(track);
            bool remove=!spec.contains("keys") || (spec["keys"].is_array() && spec["keys"].empty());
            if(!remove) kept.push_back(spec);
            json["tracks"]=kept;
            doc.apply(SetTimeline{timelineFromJson(json)},(remove?"Remove track ":"Animate ")+spec.value("entity",spec.value("material",std::string("?")))+" "+spec.value("property",std::string()));
            return 0;});}},
        {"add_event",[](lua_State* L){return protect(L,[&]{
            if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("add_event() takes { time = t, effect = id, entity = key?, position = vec3? }");
            auto& doc=document(L);
            auto json=timelineToJson(doc.document().scene.timeline());
            if(!json.contains("events")) json["events"]=Json::array();
            auto event=toJson(L,2);
            json["events"].push_back(event);
            doc.apply(SetTimeline{timelineFromJson(json)},"Add event "+event.value("effect",std::string()));
            return 0;});}},
        {"environment",[](lua_State* L){return protect(L,[&]{pushJson(L,environmentToJson(document(L).document().scene.environment()));return 1;});}},
        {"set_environment",[](lua_State* L){return protect(L,[&]{
            // Changes merge into the current environment: doc:set_environment{ bloom = 0.8 }.
            if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("set_environment() takes a table of changes");
            auto& doc=document(L);
            doc.apply(SetEnvironment{environmentFromJson(toJson(L,2),doc.document().scene.environment())});return 0;});}},
        {"set_camera",[](lua_State* L){return protect(L,[&]{
            // { position = keys, target = keys, fov = keys } (each a curve or a constant); nil clears.
            auto& doc=document(L);
            auto json=timelineToJson(doc.document().scene.timeline());
            json.erase("camera");
            if(!lua::none(L,2)) json["camera"]=toJson(L,2);
            doc.apply(SetTimeline{timelineFromJson(json)},"Edit camera");
            return 0;});}},
        {"undo",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,document(L).undo());return 1;});}},
        {"redo",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,document(L).redo());return 1;});}},
        {"history",[](lua_State* L){return protect(L,[&]{
            auto& doc=document(L);lua_createtable(L,0,2);
            pushStrings(L,doc.undoHistory());lua_setfield(L,-2,"undo");pushStrings(L,doc.redoHistory());lua_setfield(L,-2,"redo");
            return 1;});}},
        {"transaction",[](lua_State* L){return protect(L,[&]{
            // Every edit inside becomes one undo step; an error rolls them back and propagates.
            auto& doc=document(L);auto name=lua::string(L,2,"label");
            if(lua_type(L,3)!=LUA_TFUNCTION) throw std::invalid_argument("transaction() needs a function");
            auto before=doc.revision();
            doc.beginGroup(name);
            lua_pushvalue(L,3);
            auto error=lua::callProtected(L,0,0);
            doc.endGroup();
            if(error) {
                if(doc.revision()!=before) doc.undo();
                throw std::runtime_error(*error);
            }
            return 0;});}},
        {"save",[](lua_State* L){return protect(L,[&]{
            auto& ref=documentRef(L);
            std::filesystem::path target=lua::none(L,2)?ref.path:std::filesystem::path(lua::string(L,2,"path"));
            if(target.empty()) throw std::invalid_argument("save() needs a path for a new document");
            ref.document->save(target);ref.path=target;return 0;});}},
    },{
        {"path",[](lua_State* L){return protect(L,[&]{lua::push(L,documentRef(L).path.string());return 1;});},nullptr},
        {"modified",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,document(L).modified());return 1;});},nullptr},
        {"selection",[](lua_State* L){return protect(L,[&]{
            const auto& key=document(L).selection();
            if(key.empty()) lua_pushnil(L); else lua::push(L,key);
            return 1;});},nullptr},
    },{
        {"__tostring",[](lua_State* L){return protect(L,[&]{
            auto& ref=documentRef(L);
            lua::push(L,"Document("+(ref.path.empty()?std::string("untitled"):ref.path.filename().string())+", "+std::to_string(ref.document->document().scene.size())+" entities)");
            return 1;});}},
    },lua::destroy<DocumentRef>);
    lua::defineClass(L,SimulationType,{
        {"step",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,lua_Integer(simulation(L).step(lua::number(L,2,"seconds"),readInput(L,3))));return 1;});}},
        {"tick",[](lua_State* L){return protect(L,[&]{simulation(L).tick(readInput(L,2));return 0;});}},
        {"player",[](lua_State* L){return protect(L,[&]{
            auto& sim=simulation(L);lua_createtable(L,0,3);
            lua::push(L,sim.playerPosition());lua_setfield(L,-2,"position");
            lua_pushboolean(L,sim.onGround());lua_setfield(L,-2,"grounded");
            lua_pushboolean(L,sim.gameplay().flying());lua_setfield(L,-2,"flying");
            return 1;});}},
        {"entity",[](lua_State* L){return protect(L,[&]{
            const auto& scene=simulation(L).scene();auto id=scene.find(lua::string(L,2,"key"));
            if(scene.get(id)) pushEntity(L,scene,id); else lua_pushnil(L);
            return 1;});}},
        {"entities",[](lua_State* L){return protect(L,[&]{pushEntities(L,simulation(L).scene());return 1;});}},
        {"messages",[](lua_State* L){return protect(L,[&]{pushStrings(L,simulation(L).takeMessages());return 1;});}},
    },{
        {"time",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,simulation(L).time());return 1;});},nullptr},
        {"ticks",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,lua_Integer(simulation(L).ticks()));return 1;});},nullptr},
        {"collected",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,simulation(L).collected());return 1;});},nullptr},
        {"total",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,simulation(L).collectibleCount());return 1;});},nullptr},
        {"restored",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,simulation(L).restored());return 1;});},nullptr},
        {"status",[](lua_State* L){return protect(L,[&]{lua::push(L,simulation(L).status());return 1;});},nullptr},
    },{},lua::destroy<SimulationRef>);
    lua::defineClass(L,PreviewType,{
        {"step",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,lua_Integer(preview(L).step(lua::number(L,2,"seconds"))));return 1;});}},
        {"seek",[](lua_State* L){return protect(L,[&]{preview(L).seek(lua::number(L,2,"seconds"));return 0;});}},
        {"restart",[](lua_State* L){return protect(L,[&]{preview(L).restart();return 0;});}},
        {"stats",[](lua_State* L){return protect(L,[&]{pushStats(L,preview(L));return 1;});}},
        {"entity",[](lua_State* L){return protect(L,[&]{
            const auto& scene=preview(L).scene();auto id=scene.find(lua::string(L,2,"key"));
            if(scene.get(id)) pushEntity(L,scene,id); else lua_pushnil(L);
            return 1;});}},
        {"entities",[](lua_State* L){return protect(L,[&]{pushEntities(L,preview(L).scene());return 1;});}},
        {"material",[](lua_State* L){return protect(L,[&]{
            const auto& assets=preview(L).scene().assets();auto id=lua::string(L,2,"id");
            if(assets.contains(id)) pushMaterial(L,assets.get(id)); else lua_pushnil(L);
            return 1;});}},
        {"particles",[](lua_State* L){return protect(L,[&]{
            // The rendered particle snapshot: position, size, color {r,g,b,a}, velocity, blend.
            auto limit=size_t(lua::none(L,2)?1000:std::max(0.0,lua::number(L,2,"limit")));
            auto frame=preview(L).frame();
            lua_newtable(L);
            lua_Integer n=0;
            for(const auto& batch:frame.particles) for(const auto& p:batch.particles) {
                if(size_t(n)>=limit) break;
                lua_createtable(L,0,5);
                lua::push(L,p.position);lua_setfield(L,-2,"position");
                lua_pushnumber(L,p.size);lua_setfield(L,-2,"size");
                lua_createtable(L,4,0);
                for(int c=0;c<4;++c) {lua_pushnumber(L,p.color[c]);lua_rawseti(L,-2,c+1);}
                lua_setfield(L,-2,"color");
                lua::push(L,p.velocity);lua_setfield(L,-2,"velocity");
                lua_pushstring(L,blendName(batch.blend));lua_setfield(L,-2,"blend");
                lua_rawseti(L,-2,++n);
            }
            return 1;});}},
        {"camera",[](lua_State* L){return protect(L,[&]{pushCamera(L,preview(L).camera());return 1;});}},
        {"play",[](lua_State* L){return protect(L,[&]{
            auto effect=lua::string(L,2,"effect");
            glm::vec3 position{};float yaw=0;std::string entity;uint32_t seed=0;
            if(!lua::none(L,3)) {
                if(lua_type(L,3)!=LUA_TTABLE) throw std::invalid_argument("play() options must be a table");
                if(auto v=lua::fieldVec3(L,3,"position")) position=*v;
                yaw=lua::fieldNumber(L,3,"yaw",0);entity=lua::fieldString(L,3,"entity","");
                seed=uint32_t(std::max(0.0f,lua::fieldNumber(L,3,"seed",0)));
            }
            preview(L).play(effect,position,yaw,entity,seed);return 0;});}},
        {"messages",[](lua_State* L){return protect(L,[&]{pushStrings(L,preview(L).takeMessages());return 1;});}},
        {"render",[](lua_State* L){return protect(L,[&]{
            // render(path, { width, height, supersample, camera = { position, target, fov } })
            auto path=lua::string(L,2,"path");
            auto image=captureFrame(preview(L),requireRenderer(L),readCapture(L,3,{}));
            savePng(path,image);
            lua::push(L,path);return 1;});}},
        {"render_sheet",[](lua_State* L){return protect(L,[&]{
            // render_sheet(path, { from, to, count, columns, width, height, supersample, labels, camera })
            auto path=lua::string(L,2,"path");
            SheetOptions options;
            if(!lua::none(L,3)) {
                if(lua_type(L,3)!=LUA_TTABLE) throw std::invalid_argument("render_sheet() options must be a table");
                options.frame=readCapture(L,3,options.frame);
                options.start=lua::fieldNumber(L,3,"from",0);options.end=lua::fieldNumber(L,3,"to",-1);
                options.count=uint32_t(lua::fieldNumber(L,3,"count",8));options.columns=uint32_t(lua::fieldNumber(L,3,"columns",4));
                options.labels=lua::fieldBool(L,3,"labels",true);
            }
            savePng(path,captureSheet(preview(L),requireRenderer(L),options));
            lua::push(L,path);return 1;});}},
    },{
        {"time",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,preview(L).time());return 1;});},nullptr},
        {"ticks",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,lua_Integer(preview(L).ticks()));return 1;});},nullptr},
        {"duration",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,preview(L).duration());return 1;});},nullptr},
        {"timeline_time",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,preview(L).timelineTime());return 1;});},nullptr},
        {"particle_count",[](lua_State* L){return protect(L,[&]{lua_pushinteger(L,lua_Integer(preview(L).effects().particleCount()));return 1;});},nullptr},
        {"can_render",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,rendererOf(L)!=nullptr);return 1;});},nullptr},
    },{},lua::destroy<PreviewRef>);
    return 0;
}
// Upvalue 1 of the swan table setup: the argument list.
int bindSwan(lua_State* L) {
    auto* args=static_cast<const std::vector<std::string>*>(lua_touserdata(L,1));
    lua_newtable(L);
    lua_pushstring(L,SWAN_VERSION);lua_setfield(L,-2,"version");
    pushStrings(L,*args);lua_setfield(L,-2,"args");
    static constexpr luaL_Reg functions[]={
        {"open",[](lua_State* L){return protect(L,[&]{
            auto path=lua::string(L,1,"path");
            auto owned=std::make_shared<EditorDocument>(loadScene(path));
            lua::pushObject<DocumentRef>(L,DocumentType,DocumentRef{owned,owned.get(),path});
            return 1;});}},
        {"new",[](lua_State* L){return protect(L,[&]{
            auto owned=std::make_shared<EditorDocument>(SceneDocument{});
            lua::pushObject<DocumentRef>(L,DocumentType,DocumentRef{owned,owned.get(),{}});
            return 1;});}},
        {"validate",[](lua_State* L){return protect(L,[&]{
            auto path=lua::string(L,1,"path");
            std::string error;
            try {loadScene(path);} catch(const std::exception& failure) {error=failure.what();}
            lua_pushboolean(L,error.empty());
            if(error.empty()) lua_pushnil(L); else lua::push(L,error);
            return 2;});}},
        {"preview",[](lua_State* L){return protect(L,[&]{
            FxPlayerOptions options;
            if(lua_type(L,2)==LUA_TTABLE) {
                options.seed=uint32_t(std::max(0.0f,lua::fieldNumber(L,2,"seed",0)));
                options.scripts=lua::fieldBool(L,2,"scripts",true);
            }
            std::shared_ptr<FxPlayer> created;
            if(auto* ref=static_cast<DocumentRef*>(luaL_testudata(L,1,DocumentType))) created=std::make_shared<FxPlayer>(ref->document->document(),options);
            else if(lua_type(L,1)==LUA_TSTRING) created=std::make_shared<FxPlayer>(FxPlayer::load(lua::string(L,1,"path"),options));
            else throw std::invalid_argument("preview() needs a Document or a scene path");
            lua::pushObject<PreviewRef>(L,PreviewType,PreviewRef{created});
            return 1;});}},
        {"simulate",[](lua_State* L){return protect(L,[&]{
            SimulationOptions options;
            if(lua_type(L,2)==LUA_TTABLE) {options.thirdPerson=lua::fieldBool(L,2,"third_person",false);options.flying=lua::fieldBool(L,2,"flying",false);}
            std::shared_ptr<Simulation> created;
            if(auto* ref=static_cast<DocumentRef*>(luaL_testudata(L,1,DocumentType))) created=std::make_shared<Simulation>(ref->document->document(),options);
            else if(lua_type(L,1)==LUA_TSTRING) created=std::make_shared<Simulation>(Simulation::load(lua::string(L,1,"path"),options));
            else throw std::invalid_argument("simulate() needs a Document or a scene path");
            lua::pushObject<SimulationRef>(L,SimulationType,SimulationRef{created});
            return 1;});}},
    };
    for(const auto& function:functions) {lua_pushcfunction(L,function.func);lua_setfield(L,-2,function.name);}
    lua_setglobal(L,"swan");
    return 0;
}
// Upvalues: document pointer, path pointer, global name.
int bindBorrowed(lua_State* L) {
    auto* doc=static_cast<EditorDocument*>(lua_touserdata(L,1));
    auto* path=static_cast<const std::filesystem::path*>(lua_touserdata(L,2));
    lua::pushObject<DocumentRef>(L,DocumentType,DocumentRef{nullptr,doc,*path});
    lua_setglobal(L,lua_tostring(L,3));
    return 0;
}
// Upvalue 1: the ImageRenderer to keep in the registry.
int bindRenderer(lua_State* L) {
    auto* renderer=static_cast<const ImageRenderer*>(lua_touserdata(L,1));
    if(luaL_newmetatable(L,RendererType)) {lua_pushcfunction(L,lua::destroy<ImageRenderer>);lua_setfield(L,-2,"__gc");}
    lua_pop(L,1);
    lua::pushObject<ImageRenderer>(L,RendererType,*renderer);
    lua_setfield(L,LUA_REGISTRYINDEX,RendererKey);
    return 0;
}
void protectedSetup(lua_State* L,lua_CFunction setup,std::initializer_list<void*> pointers,const char* text=nullptr) {
    lua_pushcfunction(L,setup);
    for(auto* pointer:pointers) lua_pushlightuserdata(L,pointer);
    if(text) lua_pushstring(L,text);
    if(auto error=lua::callProtected(L,int(pointers.size())+(text?1:0),0)) throw std::runtime_error("Lua binding failed: "+*error);
}
}
void bindScriptApi(ScriptEngine& engine,std::vector<std::string> args,ImageRenderer renderer) {
    protectedSetup(engine.state(),bindTypes,{});
    protectedSetup(engine.state(),bindSwan,{&args});
    if(renderer) protectedSetup(engine.state(),bindRenderer,{&renderer});
}
void bindDocument(ScriptEngine& engine,const std::string& global,EditorDocument& document,std::filesystem::path path) {
    protectedSetup(engine.state(),bindTypes,{});
    protectedSetup(engine.state(),bindBorrowed,{&document,&path},global.c_str());
}
int runScriptFile(const std::filesystem::path& file,std::vector<std::string> args,ImageRenderer renderer) {
    try {
        std::ifstream input(file,std::ios::binary);
        if(!input) throw std::runtime_error("Cannot open script: "+file.string());
        std::stringstream code;code<<input.rdbuf();
        // Trusted automation: full standard library, no instruction or memory cap.
        ScriptEngine engine(ScriptEngine::Mode::Tool,[](const std::string& line){std::cout<<line<<'\n';},{0,0});
        bindScriptApi(engine,std::move(args),std::move(renderer));
        auto result=engine.run(code.str(),"@"+file.string());
        if(!result.ok) {std::cerr<<"swan script: "<<result.error<<'\n';return 1;}
        if(!result.values.empty()) {
            try {return std::stoi(result.values.front());} catch(const std::exception&) {}
        }
        return 0;
    } catch(const std::exception& error) {std::cerr<<"swan script: "<<error.what()<<'\n';return 1;}
}
}
