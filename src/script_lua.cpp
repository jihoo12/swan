#include "script_lua.hpp"
#include <cmath>
namespace swan::lua {
namespace {
constexpr const char* Vec3="vec3";
constexpr const char* Vec2="vec2";
const char* typeName(lua_State* L,int index) {return lua_typename(L,lua_type(L,index));}
float component(lua_State* L,int table,const char* name,int position,const char* what) {
    lua_pushstring(L,name);lua_rawget(L,table);
    if(lua_isnil(L,-1)) {lua_pop(L,1);lua_rawgeti(L,table,position);}
    bool ok=lua_type(L,-1)==LUA_TNUMBER;
    double value=ok?lua_tonumber(L,-1):0;
    lua_pop(L,1);
    if(!ok) throw std::invalid_argument(std::string(what)+" needs numeric components");
    if(!std::isfinite(value)) throw std::invalid_argument(std::string(what)+" must be finite");
    return float(value);
}
// __index: getters first (called with self), then methods. __newindex: setters only.
int index(lua_State* L) {
    lua_pushvalue(L,2);lua_rawget(L,lua_upvalueindex(1));
    if(lua_isfunction(L,-1)) {lua_pushvalue(L,1);lua_call(L,1,1);return 1;}
    lua_pop(L,1);
    lua_pushvalue(L,2);lua_rawget(L,lua_upvalueindex(2));
    return 1;
}
int newIndex(lua_State* L) {
    lua_pushvalue(L,2);lua_rawget(L,lua_upvalueindex(1));
    if(!lua_isfunction(L,-1)) return luaL_error(L,"cannot assign field '%s'",lua_tostring(L,2)?lua_tostring(L,2):"?");
    lua_pushvalue(L,1);lua_pushvalue(L,3);lua_call(L,2,0);
    return 0;
}
int toStringValue(lua_State* L) {luaL_tolstring(L,1,nullptr);return 1;}
template<class V> V& self(lua_State* L,const char* type) {return object<V>(L,1,type);}
// Calling vec3{...}/vec3(x, y, z): the global is a table whose metatable has __call.
int newVec3(lua_State* L) {
    return protect(L,[&]{
        int arguments=lua_gettop(L)-1;
        glm::vec3 value{};
        if(arguments==1 && lua_type(L,2)==LUA_TNUMBER) value=glm::vec3(finite(L,2,"vec3"));
        else if(arguments==1) value=vec3(L,2,"vec3");
        else if(arguments>=3) value={finite(L,2,"x"),finite(L,3,"y"),finite(L,4,"z")};
        else if(arguments!=0) throw std::invalid_argument("vec3 takes 0, 1, or 3 arguments");
        push(L,value);return 1;});
}
int newVec2(lua_State* L) {
    return protect(L,[&]{
        int arguments=lua_gettop(L)-1;
        glm::vec2 value{};
        if(arguments==1 && lua_type(L,2)==LUA_TNUMBER) value=glm::vec2(finite(L,2,"vec2"));
        else if(arguments==1) value=vec2(L,2,"vec2");
        else if(arguments>=2) value={finite(L,2,"x"),finite(L,3,"y")};
        else if(arguments!=0) throw std::invalid_argument("vec2 takes 0, 1, or 2 arguments");
        push(L,value);return 1;});
}
void callable(lua_State* L,const char* name,lua_CFunction constructor) {
    lua_newtable(L);
    lua_newtable(L);lua_pushcfunction(L,constructor);lua_setfield(L,-2,"__call");
    lua_setmetatable(L,-2);
    lua_setglobal(L,name);
}
}
double number(lua_State* L,int index,const char* what) {
    if(lua_type(L,index)!=LUA_TNUMBER) throw std::invalid_argument(std::string(what)+" must be a number, not "+typeName(L,index));
    return lua_tonumber(L,index);
}
float finite(lua_State* L,int index,const char* what) {
    double value=number(L,index,what);
    if(!std::isfinite(value)) throw std::invalid_argument(std::string(what)+" must be finite");
    return float(value);
}
bool boolean(lua_State* L,int index,const char* what) {
    if(lua_type(L,index)!=LUA_TBOOLEAN) throw std::invalid_argument(std::string(what)+" must be a boolean, not "+typeName(L,index));
    return lua_toboolean(L,index);
}
std::string string(lua_State* L,int index,const char* what) {
    if(lua_type(L,index)!=LUA_TSTRING) throw std::invalid_argument(std::string(what)+" must be a string, not "+typeName(L,index));
    size_t length=0;const char* text=lua_tolstring(L,index,&length);
    return std::string(text,length);
}
glm::vec3 vec3(lua_State* L,int index,const char* what) {
    index=lua_absindex(L,index);
    if(auto* value=static_cast<glm::vec3*>(luaL_testudata(L,index,Vec3))) return *value;
    if(lua_type(L,index)==LUA_TTABLE) return {component(L,index,"x",1,what),component(L,index,"y",2,what),component(L,index,"z",3,what)};
    throw std::invalid_argument(std::string(what)+" must be a vec3 or {x, y, z}");
}
glm::vec2 vec2(lua_State* L,int index,const char* what) {
    index=lua_absindex(L,index);
    if(auto* value=static_cast<glm::vec2*>(luaL_testudata(L,index,Vec2))) return *value;
    if(lua_type(L,index)==LUA_TTABLE) return {component(L,index,"x",1,what),component(L,index,"y",2,what)};
    throw std::invalid_argument(std::string(what)+" must be a vec2 or {x, y}");
}
bool none(lua_State* L,int index) {return lua_isnoneornil(L,index);}
int pushField(lua_State* L,int table,const char* name) {
    table=lua_absindex(L,table);
    lua_pushstring(L,name);
    return lua_rawget(L,table);
}
bool present(lua_State* L,int table,const char* name) {
    bool result=pushField(L,table,name)!=LUA_TNIL;
    lua_pop(L,1);
    return result;
}
std::string fieldString(lua_State* L,int table,const char* name,const std::string& fallback) {
    if(pushField(L,table,name)==LUA_TNIL) {lua_pop(L,1);return fallback;}
    struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
    return string(L,-1,name);
}
float fieldNumber(lua_State* L,int table,const char* name,float fallback) {
    if(pushField(L,table,name)==LUA_TNIL) {lua_pop(L,1);return fallback;}
    struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
    return finite(L,-1,name);
}
bool fieldBool(lua_State* L,int table,const char* name,bool fallback) {
    if(pushField(L,table,name)==LUA_TNIL) {lua_pop(L,1);return fallback;}
    struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
    return boolean(L,-1,name);
}
std::optional<glm::vec3> fieldVec3(lua_State* L,int table,const char* name) {
    if(pushField(L,table,name)==LUA_TNIL) {lua_pop(L,1);return std::nullopt;}
    struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
    return vec3(L,-1,name);
}
std::optional<glm::vec2> fieldVec2(lua_State* L,int table,const char* name) {
    if(pushField(L,table,name)==LUA_TNIL) {lua_pop(L,1);return std::nullopt;}
    struct Pop { lua_State* L; ~Pop() {lua_pop(L,1);} } pop{L};
    return vec2(L,-1,name);
}
void push(lua_State* L,glm::vec3 value) {pushObject<glm::vec3>(L,Vec3,value);}
void push(lua_State* L,glm::vec2 value) {pushObject<glm::vec2>(L,Vec2,value);}
std::string toString(lua_State* L,int index) {
    index=lua_absindex(L,index);
    lua_pushcfunction(L,toStringValue);lua_pushvalue(L,index);
    if(lua_pcall(L,1,1,0)!=LUA_OK) {lua_pop(L,1);return "<error converting value>";}
    size_t length=0;const char* text=lua_tolstring(L,-1,&length);
    std::string result(text?text:"",text?length:0);
    lua_pop(L,1);
    return result;
}
std::optional<std::string> callProtected(lua_State* L,int arguments,int results) {
    if(lua_pcall(L,arguments,results,0)==LUA_OK) return std::nullopt;
    auto message=lua_type(L,-1)==LUA_TSTRING?std::string(lua_tostring(L,-1)):toString(L,-1);
    lua_pop(L,1);
    return message;
}
void defineClass(lua_State* L,const char* type,std::initializer_list<luaL_Reg> methods,std::initializer_list<Field> fields,
                 std::initializer_list<luaL_Reg> metamethods,lua_CFunction destroyFunction) {
    luaL_newmetatable(L,type);
    int metatable=lua_gettop(L);
    lua_newtable(L);
    for(const auto& field:fields) if(field.get) {lua_pushcfunction(L,field.get);lua_setfield(L,-2,field.name);}
    lua_newtable(L);
    for(const auto& method:methods) {lua_pushcfunction(L,method.func);lua_setfield(L,-2,method.name);}
    lua_pushcclosure(L,index,2);lua_setfield(L,metatable,"__index");
    lua_newtable(L);
    for(const auto& field:fields) if(field.set) {lua_pushcfunction(L,field.set);lua_setfield(L,-2,field.name);}
    lua_pushcclosure(L,newIndex,1);lua_setfield(L,metatable,"__newindex");
    for(const auto& method:metamethods) {lua_pushcfunction(L,method.func);lua_setfield(L,metatable,method.name);}
    if(destroyFunction) {lua_pushcfunction(L,destroyFunction);lua_setfield(L,metatable,"__gc");}
    lua_pop(L,1);
}
void bindMath(lua_State* L) {
    auto component3=[](int axis)->lua_CFunction{
        switch(axis) {
        case 0: return [](lua_State* L){return protect(L,[&]{lua_pushnumber(L,self<glm::vec3>(L,Vec3).x);return 1;});};
        case 1: return [](lua_State* L){return protect(L,[&]{lua_pushnumber(L,self<glm::vec3>(L,Vec3).y);return 1;});};
        default: return [](lua_State* L){return protect(L,[&]{lua_pushnumber(L,self<glm::vec3>(L,Vec3).z);return 1;});};
        }
    };
    defineClass(L,Vec3,{
        {"length",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,glm::length(vec3(L,1,"self")));return 1;});}},
        {"normalized",[](lua_State* L){return protect(L,[&]{auto v=vec3(L,1,"self");float l=glm::length(v);push(L,l>0?v/l:v);return 1;});}},
        {"dot",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,glm::dot(vec3(L,1,"self"),vec3(L,2,"other")));return 1;});}},
        {"cross",[](lua_State* L){return protect(L,[&]{push(L,glm::cross(vec3(L,1,"self"),vec3(L,2,"other")));return 1;});}},
        {"distance",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,glm::distance(vec3(L,1,"self"),vec3(L,2,"other")));return 1;});}},
        {"lerp",[](lua_State* L){return protect(L,[&]{push(L,glm::mix(vec3(L,1,"self"),vec3(L,2,"other"),finite(L,3,"t")));return 1;});}},
    },{
        {"x",component3(0),[](lua_State* L){return protect(L,[&]{self<glm::vec3>(L,Vec3).x=finite(L,2,"x");return 0;});}},
        {"y",component3(1),[](lua_State* L){return protect(L,[&]{self<glm::vec3>(L,Vec3).y=finite(L,2,"y");return 0;});}},
        {"z",component3(2),[](lua_State* L){return protect(L,[&]{self<glm::vec3>(L,Vec3).z=finite(L,2,"z");return 0;});}},
    },{
        {"__add",[](lua_State* L){return protect(L,[&]{push(L,vec3(L,1,"left")+vec3(L,2,"right"));return 1;});}},
        {"__sub",[](lua_State* L){return protect(L,[&]{push(L,vec3(L,1,"left")-vec3(L,2,"right"));return 1;});}},
        {"__mul",[](lua_State* L){return protect(L,[&]{
            if(lua_type(L,1)==LUA_TNUMBER) push(L,float(lua_tonumber(L,1))*vec3(L,2,"right"));
            else if(lua_type(L,2)==LUA_TNUMBER) push(L,vec3(L,1,"left")*float(lua_tonumber(L,2)));
            else push(L,vec3(L,1,"left")*vec3(L,2,"right"));
            return 1;});}},
        {"__div",[](lua_State* L){return protect(L,[&]{push(L,vec3(L,1,"left")/finite(L,2,"divisor"));return 1;});}},
        {"__unm",[](lua_State* L){return protect(L,[&]{push(L,-vec3(L,1,"value"));return 1;});}},
        {"__eq",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,vec3(L,1,"left")==vec3(L,2,"right"));return 1;});}},
        {"__tostring",[](lua_State* L){return protect(L,[&]{
            auto v=vec3(L,1,"value");char text[96];std::snprintf(text,sizeof text,"vec3(%g, %g, %g)",v.x,v.y,v.z);
            lua_pushstring(L,text);return 1;});}},
    },nullptr);
    defineClass(L,Vec2,{
        {"length",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,glm::length(vec2(L,1,"self")));return 1;});}},
    },{
        {"x",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,self<glm::vec2>(L,Vec2).x);return 1;});},[](lua_State* L){return protect(L,[&]{self<glm::vec2>(L,Vec2).x=finite(L,2,"x");return 0;});}},
        {"y",[](lua_State* L){return protect(L,[&]{lua_pushnumber(L,self<glm::vec2>(L,Vec2).y);return 1;});},[](lua_State* L){return protect(L,[&]{self<glm::vec2>(L,Vec2).y=finite(L,2,"y");return 0;});}},
    },{
        {"__add",[](lua_State* L){return protect(L,[&]{push(L,vec2(L,1,"left")+vec2(L,2,"right"));return 1;});}},
        {"__sub",[](lua_State* L){return protect(L,[&]{push(L,vec2(L,1,"left")-vec2(L,2,"right"));return 1;});}},
        {"__mul",[](lua_State* L){return protect(L,[&]{
            if(lua_type(L,1)==LUA_TNUMBER) push(L,float(lua_tonumber(L,1))*vec2(L,2,"right"));
            else push(L,vec2(L,1,"left")*finite(L,2,"factor"));
            return 1;});}},
        {"__unm",[](lua_State* L){return protect(L,[&]{push(L,-vec2(L,1,"value"));return 1;});}},
        {"__eq",[](lua_State* L){return protect(L,[&]{lua_pushboolean(L,vec2(L,1,"left")==vec2(L,2,"right"));return 1;});}},
        {"__tostring",[](lua_State* L){return protect(L,[&]{
            auto v=vec2(L,1,"value");char text[64];std::snprintf(text,sizeof text,"vec2(%g, %g)",v.x,v.y);
            lua_pushstring(L,text);return 1;});}},
    },nullptr);
    callable(L,"vec3",newVec3);
    callable(L,"vec2",newVec2);
}
}
