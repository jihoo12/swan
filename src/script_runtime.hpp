#pragma once
#include "input.hpp"
#include "scene.hpp"
#include <functional>
#include <memory>
#include <string>
namespace swan {
// What behaviour scripts can read about the game each tick (as the global `game` table).
struct ScriptGameState {
    float time=0;
    glm::vec3 player{};
    bool grounded=false,flying=false;
    int collected=0,total=0;
    Input input;
};
// Runs per-entity Lua behaviours (Scene::scripts() assets) against a live runtime scene, never
// authored data. A failing behaviour is disabled and reported; the game and other scripts go on.
class ScriptRuntime {
public:
    using Log=std::function<void(const std::string&)>;
    ScriptRuntime(Scene& scene,Log log);
    ~ScriptRuntime();
    ScriptRuntime(const ScriptRuntime&)=delete;
    ScriptRuntime& operator=(const ScriptRuntime&)=delete;
    // Starts behaviours of newly scripted entities, then calls update(self, dt) on each.
    void update(float dt,const ScriptGameState& state);
    // Receives game.effect(id, {...}) calls: a world position, or an entity key plus offset.
    using EffectPlayer=std::function<void(const std::string& effect,glm::vec3 position,float yaw,const std::string& entity,uint32_t seed)>;
    void setEffectPlayer(EffectPlayer player);
    // Calls collected(self) on the entity's behaviour; call before destroying the entity.
    void collected(EntityId id);
    const std::string& message() const;   // Latest game.message() text.
    size_t instanceCount() const;
    size_t failureCount() const;
    struct Impl; // Opaque; public so binding helpers can name it.
private:
    std::unique_ptr<Impl> impl;
};
}
