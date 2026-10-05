#include "script_runtime.hpp"
#include "script_engine.hpp"
#include "script_lua.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
namespace swan {
namespace {
constexpr const char* EntityType="Entity";
uint64_t handleKey(EntityId id) {return (uint64_t(id.index)<<32)|id.generation;}
void pushValue(lua_State* L,const ScriptValue& value) {
    std::visit([&](const auto& v){
        using T=std::decay_t<decltype(v)>;
        if constexpr(std::is_same_v<T,double>) lua_pushnumber(L,v);
        else if constexpr(std::is_same_v<T,bool>) lua_pushboolean(L,v);
        else lua::push(L,v);
    },value);
}
// Reads a {name = number|boolean|string} table without invoking metamethods.
ScriptProperties readProperties(lua_State* L,int table) {
    ScriptProperties result;
    if(lua_isnoneornil(L,table)) return result;
    if(lua_type(L,table)!=LUA_TTABLE) throw std::invalid_argument("properties must be a table");
    table=lua_absindex(L,table);
    lua_pushnil(L);
    while(lua_next(L,table)) {
        struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
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
}
struct ScriptRuntime::Impl {
    // The engine is declared first: registry references below die with the VM, never after it.
    ScriptEngine engine;
    Scene& scene;
    Log log;
    struct Instance { EntityId id; std::string script; int self=LUA_NOREF; int module=LUA_NOREF; bool failed=false; };
    std::map<std::string,int> modules; // Script ID -> registry reference, LUA_NOREF after a failure.
    std::vector<Instance> instances;
    std::set<uint64_t> known;
    int game=LUA_NOREF;
    std::string message;
    size_t failures=0;
    ScriptRuntime::EffectPlayer effects;
    Impl(Scene& scene,Log log):engine(ScriptEngine::Mode::Sandbox,[this](const std::string& line){report(line);}),scene(scene),log(std::move(log)) {}
    lua_State* L() const {return engine.state();}
    void report(const std::string& line) {if(log) log(line);}
    Entity& entity(EntityId id) {
        auto* value=scene.get(id);
        if(!value) throw std::runtime_error("entity no longer exists");
        return *value;
    }
    std::string label(EntityId id) {
        const auto* value=scene.get(id);
        return value?(value->name.empty()?value->key:value->name):"<destroyed>";
    }
    bool hasChildren(EntityId id) {
        for(auto other:scene.entities()) if(scene.parent(other)==id) return true;
        return false;
    }
    int module(const std::string& id) {
        if(auto found=modules.find(id);found!=modules.end()) return found->second;
        auto& slot=modules[id];slot=LUA_NOREF;
        const auto& asset=scene.scripts().get(id);
        auto chunkName="@"+(asset.source.empty()?id:asset.source.filename().string());
        auto* L=this->L();
        if(luaL_loadbufferx(L,asset.code->data(),asset.code->size(),chunkName.c_str(),"t")!=LUA_OK) {
            report("Script "+id+": "+lua::toString(L,-1));lua_pop(L,1);++failures;return slot;
        }
        ScriptBudget budget(engine);
        if(auto error=lua::callProtected(L,0,1)) {report("Script "+id+": "+*error);++failures;return slot;}
        if(lua_type(L,-1)!=LUA_TTABLE) {lua_pop(L,1);report("Script "+id+" must return a table of callbacks");++failures;return slot;}
        slot=luaL_ref(L,LUA_REGISTRYINDEX);
        return slot;
    }
    // Calls self:<name>(...) with `arguments` values already on the stack; a failure stops the instance.
    void call(Instance& instance,const char* name,int arguments=0) {
        auto* L=this->L();
        int first=lua_gettop(L)-arguments+1;
        if(instance.failed) {lua_settop(L,first-1);return;}
        // Raw lookups (instance, then module): no user metamethod runs outside lua_pcall.
        lua_rawgeti(L,LUA_REGISTRYINDEX,instance.self);
        if(lua::pushField(L,-1,name)==LUA_TNIL) {
            lua_pop(L,1);lua_rawgeti(L,LUA_REGISTRYINDEX,instance.module);lua::pushField(L,-1,name);lua_remove(L,-2);
        }
        if(!lua_isfunction(L,-1)) {lua_settop(L,first-1);return;}
        lua_insert(L,first);   // function, args..., self
        lua_insert(L,first+1); // function, self, args...
        ScriptBudget budget(engine);
        if(auto error=lua::callProtected(L,arguments+1,0)) {
            instance.failed=true;++failures;
            report("Script "+instance.script+" on "+label(instance.id)+" stopped: "+*error);
        }
    }
    void start(EntityId id);
    void refreshGame(const ScriptGameState& state);
};
void ScriptRuntime::Impl::start(EntityId id) {
    auto* L=this->L();
    const auto& value=*scene.get(id);
    Instance instance{id,value.scriptId};
    instance.module=module(value.scriptId);
    if(instance.module==LUA_NOREF) {instance.failed=true;instances.push_back(instance);return;}
    lua_newtable(L);
    int self=lua_gettop(L);
    lua_rawgeti(L,LUA_REGISTRYINDEX,instance.module);
    int behaviour=lua_gettop(L);
    // Defaults from the script's `properties` table, then this entity's overrides.
    if(lua::pushField(L,behaviour,"properties")==LUA_TTABLE) {
        lua_pushnil(L);
        while(lua_next(L,-2)) {lua_pushvalue(L,-2);lua_insert(L,-2);lua_rawset(L,self);}
    }
    lua_pop(L,1);
    for(const auto& [name,property]:value.properties) {lua::push(L,name);pushValue(L,property);lua_rawset(L,self);}
    lua::pushObject<EntityId>(L,EntityType,id);lua_setfield(L,self,"entity");
    lua_newtable(L);lua_pushvalue(L,behaviour);lua_setfield(L,-2,"__index");lua_setmetatable(L,self);
    lua_pop(L,1); // behaviour
    instance.self=luaL_ref(L,LUA_REGISTRYINDEX);
    instances.push_back(instance);
    call(instances.back(),"start");
}
void ScriptRuntime::Impl::refreshGame(const ScriptGameState& state) {
    auto* L=this->L();
    lua_rawgeti(L,LUA_REGISTRYINDEX,game);
    int table=lua_gettop(L);
    lua_pushnumber(L,state.time);lua_setfield(L,table,"time");
    lua_pushinteger(L,state.collected);lua_setfield(L,table,"collected");
    lua_pushinteger(L,state.total);lua_setfield(L,table,"total");
    lua_newtable(L);
    lua::push(L,state.player);lua_setfield(L,-2,"position");
    lua_pushboolean(L,state.grounded);lua_setfield(L,-2,"grounded");
    lua_pushboolean(L,state.flying);lua_setfield(L,-2,"flying");
    lua_setfield(L,table,"player");
    lua_newtable(L);
    lua::push(L,state.input.move);lua_setfield(L,-2,"move");
    lua_pushboolean(L,state.input.jump);lua_setfield(L,-2,"jump");
    lua_pushboolean(L,state.input.interact);lua_setfield(L,-2,"interact");
    lua_pushboolean(L,state.input.sprint);lua_setfield(L,-2,"sprint");
    lua_setfield(L,table,"input");
    lua_pop(L,1);
}
namespace {
ScriptRuntime::Impl* runtimeOf(lua_State* L) {
    lua_getfield(L,LUA_REGISTRYINDEX,"swan.runtime");
    auto* runtime=static_cast<ScriptRuntime::Impl*>(lua_touserdata(L,-1));
    lua_pop(L,1);
    return runtime;
}
EntityId& entityArg(lua_State* L,int index) {return lua::object<EntityId>(L,index,EntityType);}
Entity& liveEntity(lua_State* L,int index) {return runtimeOf(L)->entity(entityArg(L,index));}
void pushEntity(lua_State* L,EntityId id) {lua::pushObject<EntityId>(L,EntityType,id);}
int bindRuntime(lua_State* L) {
    using lua::protect;
    lua::defineClass(L,EntityType,{
        {"destroy",[](lua_State* L){return protect(L,[&]{runtimeOf(L)->scene.destroy(entityArg(L,1));return 0;});}},
    },{
        {"key",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).key);return 1;});},nullptr},
        {"alive",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,runtimeOf(L)->scene.get(entityArg(L,1))!=nullptr);return 1;});},nullptr},
        {"script",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).scriptId);return 1;});},nullptr},
        {"name",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).name);return 1;});},
                [](lua_State* L){return protect(L,[&]{liveEntity(L,1).name=lua::string(L,2,"name");return 0;});}},
        {"position",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).transform.position);return 1;});},
                    [](lua_State* L){return protect(L,[&]{liveEntity(L,1).transform.position=lua::vec3(L,2,"position");return 0;});}},
        {"scale",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).transform.scale);return 1;});},
                 [](lua_State* L){return protect(L,[&]{
                     auto scale=lua::vec3(L,2,"scale");
                     if(scale.x<=0 || scale.y<=0 || scale.z<=0) throw std::invalid_argument("scale must be positive");
                     // Parents need uniform scale (the yaw-only transform model has no shear).
                     if((scale.x!=scale.y || scale.x!=scale.z) && runtimeOf(L)->hasChildren(entityArg(L,1))) throw std::invalid_argument("entities with children need uniform scale");
                     liveEntity(L,1).transform.scale=scale;return 0;});}},
        {"yaw",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,liveEntity(L,1).transform.yaw);return 1;});},
               [](lua_State* L){return protect(L,[&]{liveEntity(L,1).transform.yaw=std::remainder(lua::finite(L,2,"yaw"),6.2831853f);return 0;});}},
        {"world_position",[](lua_State* L){return protect(L,[&]{
            auto* runtime=runtimeOf(L);auto id=entityArg(L,1);runtime->entity(id);
            lua::push(L,runtime->scene.worldTransform(id).position);return 1;});},nullptr},
        {"material",[](lua_State* L){return protect(L,[&]{lua::push(L,liveEntity(L,1).materialId);return 1;});},
                    [](lua_State* L){return protect(L,[&]{
                        auto id=lua::string(L,2,"material");
                        if(!runtimeOf(L)->scene.assets().contains(id)) throw std::invalid_argument("unknown material "+id);
                        liveEntity(L,1).materialId=id;return 0;});}},
        {"effect",[](lua_State* L){return protect(L,[&]{
            const auto& effect=liveEntity(L,1).effectId;
            if(effect.empty()) lua_pushnil(L); else lua::push(L,effect);
            return 1;});},
                  [](lua_State* L){return protect(L,[&]{
                      // nil or false detaches; live particles finish on their own.
                      std::string effect=lua_isnoneornil(L,2) || (lua_type(L,2)==LUA_TBOOLEAN && !lua_toboolean(L,2))?std::string():lua::string(L,2,"effect");
                      if(!effect.empty() && !runtimeOf(L)->scene.effects().contains(effect)) throw std::invalid_argument("unknown effect "+effect);
                      liveEntity(L,1).effectId=effect;return 0;});}},
        {"parent",[](lua_State* L){return protect(L,[&]{
            auto* runtime=runtimeOf(L);auto id=entityArg(L,1);runtime->entity(id);
            if(auto parent=runtime->scene.parent(id)) pushEntity(L,*parent); else lua_pushnil(L);
            return 1;});},nullptr},
    },{
        {"__eq",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,entityArg(L,1)==entityArg(L,2));return 1;});}},
        {"__tostring",[](lua_State* L){return protect(L,[&]{
            const auto* value=runtimeOf(L)->scene.get(entityArg(L,1));
            lua::push(L,value?"Entity("+value->key+")":std::string("Entity(<destroyed>)"));return 1;});}},
    },nullptr);
    lua_newtable(L);
    static constexpr luaL_Reg functions[]={
        {"message",[](lua_State* L){return protect(L,[&]{
            auto* runtime=runtimeOf(L);runtime->message=lua::string(L,1,"message");runtime->report(runtime->message);return 0;});}},
        {"find",[](lua_State* L){return protect(L,[&]{
            auto* runtime=runtimeOf(L);auto id=runtime->scene.find(lua::string(L,1,"key"));
            if(runtime->scene.get(id)) pushEntity(L,id); else lua_pushnil(L);
            return 1;});}},
        {"entities",[](lua_State* L){return protect(L,[&]{
            auto ids=runtimeOf(L)->scene.entities();
            lua_createtable(L,int(ids.size()),0);
            for(size_t i=0;i<ids.size();++i) {pushEntity(L,ids[i]);lua_rawseti(L,-2,lua_Integer(i+1));}
            return 1;});}},
        {"effect",[](lua_State* L){return protect(L,[&]{
            // game.effect(id, { position = vec3, entity = Entity|key, yaw = n, seed = n })
            auto* runtime=runtimeOf(L);
            auto effect=lua::string(L,1,"effect");
            if(!runtime->scene.effects().contains(effect)) throw std::invalid_argument("unknown effect "+effect);
            glm::vec3 position{};float yaw=0;std::string entity;uint32_t seed=0;
            if(!lua_isnoneornil(L,2)) {
                if(lua_type(L,2)!=LUA_TTABLE) throw std::invalid_argument("effect() options must be a table");
                if(auto v=lua::fieldVec3(L,2,"position")) position=*v;
                yaw=lua::fieldNumber(L,2,"yaw",0);
                seed=uint32_t(std::max(0.0f,lua::fieldNumber(L,2,"seed",0)));
                int type=lua::pushField(L,2,"entity");
                if(type==LUA_TSTRING) entity=lua_tostring(L,-1);
                else if(type!=LUA_TNIL) {
                    auto* id=static_cast<EntityId*>(luaL_testudata(L,-1,EntityType));
                    if(!id) {lua_pop(L,1);throw std::invalid_argument("entity must be an Entity or a key");}
                    entity=runtime->entity(*id).key;
                }
                lua_pop(L,1);
            }
            if(!entity.empty() && !runtime->scene.get(runtime->scene.find(entity))) throw std::invalid_argument("unknown entity "+entity);
            if(!runtime->effects) throw std::runtime_error("effects are not available in this runtime");
            runtime->effects(effect,position,yaw,entity,seed);
            return 0;});}},
        {"spawn",[](lua_State* L){return protect(L,[&]{
            if(lua_type(L,1)!=LUA_TTABLE) throw std::invalid_argument("spawn() takes a table");
            Entity value;
            value.name=lua::fieldString(L,1,"name","Spawned");
            value.meshId=lua::fieldString(L,1,"mesh","builtin:cube");
            value.materialId=lua::fieldString(L,1,"material","default");
            if(auto v=lua::fieldVec3(L,1,"position")) value.transform.position=*v;
            if(auto v=lua::fieldVec3(L,1,"scale")) value.transform.scale=*v;
            value.transform.yaw=lua::fieldNumber(L,1,"yaw",0);
            value.scriptId=lua::fieldString(L,1,"script","");
            value.effectId=lua::fieldString(L,1,"effect","");
            if(!value.effectId.empty() && !runtimeOf(L)->scene.effects().contains(value.effectId)) throw std::invalid_argument("unknown effect "+value.effectId);
            lua::pushField(L,1,"properties");
            {struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};value.properties=readProperties(L,-1);}
            pushEntity(L,runtimeOf(L)->scene.create(std::move(value)));
            return 1;});}},
    };
    for(const auto& function:functions) {lua_pushcfunction(L,function.func);lua_setfield(L,-2,function.name);}
    lua_pushvalue(L,-1);lua_setglobal(L,"game");
    return 1;
}
}
ScriptRuntime::ScriptRuntime(Scene& scene,Log log):impl(std::make_unique<Impl>(scene,std::move(log))) {
    auto* L=impl->L();
    lua_pushlightuserdata(L,impl.get());lua_setfield(L,LUA_REGISTRYINDEX,"swan.runtime");
    lua_pushcfunction(L,bindRuntime);
    if(auto error=lua::callProtected(L,0,1)) throw std::runtime_error("Cannot bind gameplay scripting: "+*error);
    impl->game=luaL_ref(L,LUA_REGISTRYINDEX);
}
ScriptRuntime::~ScriptRuntime()=default;
void ScriptRuntime::update(float dt,const ScriptGameState& state) {
    auto* L=impl->L();
    impl->refreshGame(state);
    std::erase_if(impl->instances,[&](const Impl::Instance& instance){
        if(impl->scene.get(instance.id)) return false;
        luaL_unref(L,LUA_REGISTRYINDEX,instance.self);
        return true;
    });
    for(auto id:impl->scene.entities()) {
        const auto* value=impl->scene.get(id);
        if(!value || value->scriptId.empty() || !impl->known.insert(handleKey(id)).second) continue;
        impl->start(id);
    }
    // Index loop: callbacks may spawn entities, which start on the next tick.
    for(size_t i=0;i<impl->instances.size();++i) {
        if(!impl->scene.get(impl->instances[i].id)) continue;
        lua_pushnumber(L,dt);
        impl->call(impl->instances[i],"update",1);
    }
}
void ScriptRuntime::collected(EntityId id) {
    for(size_t i=0;i<impl->instances.size();++i) if(impl->instances[i].id==id) impl->call(impl->instances[i],"collected");
}
void ScriptRuntime::setEffectPlayer(EffectPlayer player) {impl->effects=std::move(player);}
const std::string& ScriptRuntime::message() const {return impl->message;}
size_t ScriptRuntime::instanceCount() const {return impl->instances.size();}
size_t ScriptRuntime::failureCount() const {return impl->failures;}
}
