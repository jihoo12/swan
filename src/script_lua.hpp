#pragma once
// Internal helpers for binding C++ to the Lua 5.4 C API.
//
// Lua reports errors with longjmp, which skips C++ destructors. Therefore:
// - Binding functions run their body inside protect(): argument checks throw C++ exceptions,
//   and lua_error() is raised only after every C++ object in the body has been destroyed.
// - Calls into script code always go through lua_pcall (see callProtected()).
// - Reads from script-provided tables use raw access, so no metamethod can run (and fail) there.
#include <lua.hpp>
#include <glm/glm.hpp>
#include <cstdio>
#include <initializer_list>
#include <new>
#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
namespace swan::lua {
template<class Body> int protect(lua_State* L,Body&& body) {
    char message[1024];
    try {return body();}
    catch(const std::exception& error) {std::snprintf(message,sizeof message,"%s",error.what());}
    catch(...) {std::snprintf(message,sizeof message,"unknown C++ exception");}
    lua_pushstring(L,message);
    return lua_error(L);
}
// Argument readers; they throw std::invalid_argument naming `what` on a type mismatch.
double number(lua_State* L,int index,const char* what);
float finite(lua_State* L,int index,const char* what);
bool boolean(lua_State* L,int index,const char* what);
std::string string(lua_State* L,int index,const char* what);
glm::vec3 vec3(lua_State* L,int index,const char* what);   // vec3 userdata or {x,y,z} / {1,2,3}
glm::vec2 vec2(lua_State* L,int index,const char* what);
bool none(lua_State* L,int index);
// Raw field access on the table at `table`. present() pushes nothing and reports non-nil.
bool present(lua_State* L,int table,const char* name);
std::string fieldString(lua_State* L,int table,const char* name,const std::string& fallback);
float fieldNumber(lua_State* L,int table,const char* name,float fallback);
bool fieldBool(lua_State* L,int table,const char* name,bool fallback);
std::optional<glm::vec3> fieldVec3(lua_State* L,int table,const char* name);
std::optional<glm::vec2> fieldVec2(lua_State* L,int table,const char* name);
// Pushes the raw field (possibly nil) and returns its type.
int pushField(lua_State* L,int table,const char* name);
void push(lua_State* L,glm::vec3 value);
void push(lua_State* L,glm::vec2 value);
inline void push(lua_State* L,const std::string& value) {lua_pushlstring(L,value.data(),value.size());}
// Sets table[name] = value-on-top (pops it); the table must be one this code created.
inline void set(lua_State* L,int table,const char* name) {lua_setfield(L,table,name);}
// tostring() of a value without risking an unprotected __tostring error.
std::string toString(lua_State* L,int index);
// Calls the function below `arguments` values with lua_pcall; on failure returns the message and
// leaves the stack as it was before the function was pushed.
std::optional<std::string> callProtected(lua_State* L,int arguments,int results);

// Userdata classes: a metatable with getters, setters, methods, and extra metamethods.
struct Field { const char* name; lua_CFunction get; lua_CFunction set; };
void defineClass(lua_State* L,const char* type,std::initializer_list<luaL_Reg> methods,std::initializer_list<Field> fields,
                 std::initializer_list<luaL_Reg> metamethods,lua_CFunction destroy);
template<class T> int destroy(lua_State* L) {static_cast<T*>(lua_touserdata(L,1))->~T();return 0;}
template<class T,class... Args> T& pushObject(lua_State* L,const char* type,Args&&... args) {
    void* memory=lua_newuserdatauv(L,sizeof(T),0);
    T* object=new(memory) T(std::forward<Args>(args)...);
    luaL_setmetatable(L,type);
    return *object;
}
template<class T> T& object(lua_State* L,int index,const char* type) {
    void* memory=luaL_testudata(L,index,type);
    if(!memory) throw std::invalid_argument(std::string("expected a ")+type+(index==1?" (call methods with ':', e.g. object:method())":""));
    return *static_cast<T*>(memory);
}
// Registers vec2/vec3 classes and the callable globals vec2(...) / vec3(...).
void bindMath(lua_State* L);
}
