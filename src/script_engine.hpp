#pragma once
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>
struct lua_State;
namespace swan {
struct ScriptResult { bool ok=true; std::string error; std::vector<std::string> values; };
struct ScriptLimits { size_t memoryBytes=64u<<20; uint64_t instructions=5'000'000; }; // 0 disables a limit.
// One Lua 5.4 VM with a memory cap, a per-call instruction budget, and a print() sink.
// Sandbox mode opens only base/coroutine/math/string/table/utf8 and loads text chunks only;
// Tool mode opens the full standard library for trusted automation scripts.
class ScriptEngine {
public:
    enum class Mode { Sandbox, Tool };
    using Sink=std::function<void(const std::string&)>;
    ScriptEngine(Mode mode,Sink print,ScriptLimits limits={});
    ~ScriptEngine();
    ScriptEngine(const ScriptEngine&)=delete;
    ScriptEngine& operator=(const ScriptEngine&)=delete;
    lua_State* state() const;
    Mode mode() const;
    // Compile and run a chunk under the budget. With echo, "return <code>" is tried first so
    // REPL expressions print their value; returned values are converted with tostring().
    ScriptResult run(std::string_view code,const std::string& chunkName,bool echo=false);
    // Guards bracket every call into script code (the counter restarts for each guard).
    void beginBudget();
    void endBudget();
    size_t memoryUsed() const;
    const Sink& printer() const;
    struct Impl; // Opaque; public so the allocator and hook callbacks can name it.
private:
    std::unique_ptr<Impl> impl;
};
class ScriptBudget {
public:
    explicit ScriptBudget(ScriptEngine& engine):engine(engine) {engine.beginBudget();}
    ~ScriptBudget() {engine.endBudget();}
    ScriptBudget(const ScriptBudget&)=delete;
    ScriptBudget& operator=(const ScriptBudget&)=delete;
private:
    ScriptEngine& engine;
};
}
