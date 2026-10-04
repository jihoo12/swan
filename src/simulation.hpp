#pragma once
#include "game.hpp"
#include "scene_io.hpp"
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>
namespace swan {
struct SimulationOptions {
    bool thirdPerson=false;
    bool flying=false; // Start in free flight (the `--overview` camera).
};
// Headless gameplay: run a scene with its behaviour scripts at the engine's fixed 120 Hz tick,
// drive it with Input, and inspect the result. No window, GPU, or GLFW is involved, so it suits
// tests, servers, bots, and batch tools. Build scenes with EditorDocument or scene_io.
class Simulation {
public:
    static constexpr double tickSeconds=1.0/120.0;
    explicit Simulation(const SceneDocument& document,SimulationOptions options={});
    static Simulation load(const std::filesystem::path& scene,SimulationOptions options={});
    Simulation(Simulation&&)=default;
    // Advance by whole ticks covering `seconds` (at least one). One-shot actions in `input`
    // (jump, interact, toggles) apply on the first tick only; held values apply throughout.
    uint64_t step(double seconds,const Input& input={});
    void tick(const Input& input={});
    uint64_t ticks() const { return tickCount; }
    double time() const { return double(tickCount)*tickSeconds; }
    const Scene& scene() const { return game->scene(); }
    Game& gameplay() { return *game; }
    const Game& gameplay() const { return *game; }
    glm::vec3 playerPosition() const { return game->playerFeet(); }
    bool onGround() const { return game->onGround(); }
    int collected() const { return game->collected(); }
    int collectibleCount() const { return game->collectibleCount(); }
    bool restored() const { return game->restored(); }
    std::string status() const { return game->status(); }
    // Script output and errors since the last call.
    std::vector<std::string> takeMessages() { return game->takeMessages(); }
    // The render snapshot a windowed run would draw this tick (no GPU needed).
    RenderFrame frame() const { return game->renderFrame(1); }
private:
    std::unique_ptr<Game> game;
    uint64_t tickCount=0;
};
}
