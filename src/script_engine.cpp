#include "script_engine.hpp"
#include "script_lua.hpp"
#include <cstdlib>
#include <stdexcept>
namespace swan {
struct ScriptEngine::Impl {
    lua_State* state=nullptr;
    Mode mode;
    Sink print;
    ScriptLimits limits;
    size_t used=0;
    uint64_t counted=0;
    int budgetDepth=0;
};
namespace {
constexpr int hookInterval=1000;
void* allocate(void* data,void* block,size_t oldSize,size_t newSize) {
    auto* impl=static_cast<ScriptEngine::Impl*>(data);
    size_t previous=block?oldSize:0; // For new blocks, oldSize encodes the object type.
    if(newSize==0) {std::free(block);impl->used-=previous;return nullptr;}
    if(impl->limits.memoryBytes && impl->used-previous+newSize>impl->limits.memoryBytes) return nullptr;
    void* result=std::realloc(block,newSize);
    if(result) impl->used=impl->used-previous+newSize;
    return result;
}
ScriptEngine::Impl* owner(lua_State* L) {return *static_cast<ScriptEngine::Impl**>(lua_getextraspace(L));}
void countInstructions(lua_State* L,lua_Debug*) {
    auto* impl=owner(L);
    impl->counted+=hookInterval;
    if(impl->budgetDepth>0 && impl->limits.instructions && impl->counted>impl->limits.instructions)
        luaL_error(L,"instruction budget of %d exceeded (infinite loop?)",int(impl->limits.instructions));
}
int panic(lua_State* L) {
    // Reached only by an error outside any protected call, which this code never makes.
    const char* message=lua_tostring(L,-1);
    std::fprintf(stderr,"Lua panic: %s\n",message?message:"unknown error");
    std::abort();
}
// print(...): tab-separated tostring() values, delivered to the engine's sink.
int print(lua_State* L) {
    int count=lua_gettop(L);
    luaL_Buffer buffer;luaL_buffinit(L,&buffer);
    for(int i=1;i<=count;++i) {
        if(i>1) luaL_addchar(&buffer,'\t');
        luaL_tolstring(L,i,nullptr);luaL_addvalue(&buffer);
    }
    luaL_pushresult(&buffer);
    size_t length=0;const char* text=lua_tolstring(L,-1,&length);
    auto* impl=owner(L);
    return lua::protect(L,[&]{if(impl->print) impl->print(std::string(text,length));return 0;});
}
// Library setup runs under lua_pcall, so even allocation failures stay recoverable.
int openSandbox(lua_State* L) {
    static constexpr luaL_Reg libraries[]={{LUA_GNAME,luaopen_base},{LUA_COLIBNAME,luaopen_coroutine},{LUA_MATHLIBNAME,luaopen_math},
        {LUA_STRLIBNAME,luaopen_string},{LUA_TABLIBNAME,luaopen_table},{LUA_UTF8LIBNAME,luaopen_utf8}};
    for(const auto& library:libraries) {luaL_requiref(L,library.name,library.func,1);lua_pop(L,1);}
    // Text chunks only: precompiled bytecode can bypass the VM's safety checks.
    static constexpr const char* restrict=R"(
        local load = load
        _G.load = function(chunk, name, _, env) return load(chunk, name, "t", env) end
        dofile, loadfile, string.dump = nil, nil, nil
    )";
    if(luaL_dostring(L,restrict)!=LUA_OK) return lua_error(L);
    return 0;
}
int openAll(lua_State* L) {luaL_openlibs(L);return 0;}
int bindBasics(lua_State* L) {
    lua_pushcfunction(L,print);lua_setglobal(L,"print");
    lua::bindMath(L);
    return 0;
}
}
ScriptEngine::ScriptEngine(Mode mode,Sink sink,ScriptLimits limits):impl(std::make_unique<Impl>()) {
    impl->mode=mode;impl->print=std::move(sink);impl->limits=limits;
    impl->state=lua_newstate(allocate,impl.get());
    if(!impl->state) throw std::runtime_error("Cannot create Lua state");
    auto* L=impl->state;
    *static_cast<Impl**>(lua_getextraspace(L))=impl.get();
    lua_atpanic(L,panic);
    try {
        lua_pushcfunction(L,mode==Mode::Tool?openAll:openSandbox);
        if(auto error=lua::callProtected(L,0,0)) throw std::runtime_error("Cannot open Lua libraries: "+*error);
        lua_pushcfunction(L,bindBasics);
        if(auto error=lua::callProtected(L,0,0)) throw std::runtime_error("Cannot bind Lua basics: "+*error);
    } catch(...) {lua_close(L);impl->state=nullptr;throw;}
    lua_sethook(L,countInstructions,LUA_MASKCOUNT,hookInterval);
}
ScriptEngine::~ScriptEngine() {if(impl->state) lua_close(impl->state);}
lua_State* ScriptEngine::state() const {return impl->state;}
ScriptEngine::Mode ScriptEngine::mode() const {return impl->mode;}
size_t ScriptEngine::memoryUsed() const {return impl->used;}
const ScriptEngine::Sink& ScriptEngine::printer() const {return impl->print;}
void ScriptEngine::beginBudget() {if(impl->budgetDepth++==0) impl->counted=0;}
void ScriptEngine::endBudget() {if(impl->budgetDepth>0) --impl->budgetDepth;}
ScriptResult ScriptEngine::run(std::string_view code,const std::string& chunkName,bool echo) {
    auto* L=impl->state;
    int base=lua_gettop(L);
    ScriptResult result;
    bool loaded=false;
    if(echo) {
        auto expression="return "+std::string(code);
        loaded=luaL_loadbufferx(L,expression.data(),expression.size(),chunkName.c_str(),"t")==LUA_OK;
        if(!loaded) lua_pop(L,1);
    }
    if(!loaded && luaL_loadbufferx(L,code.data(),code.size(),chunkName.c_str(),"t")!=LUA_OK) {
        result.ok=false;result.error=lua::toString(L,-1);lua_settop(L,base);return result;
    }
    ScriptBudget budget(*this);
    if(auto error=lua::callProtected(L,0,LUA_MULTRET)) {result.ok=false;result.error=*error;lua_settop(L,base);return result;}
    for(int i=base+1;i<=lua_gettop(L);++i) result.values.push_back(lua::toString(L,i));
    lua_settop(L,base);
    return result;
}
}
