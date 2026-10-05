#pragma once
#include "fx_runtime.hpp"
#include "scene_io.hpp"
#include <cstdint>
#include <filesystem>
#include <memory>
#include <optional>
#include <string>
#include <vector>
namespace swan {
struct FxPlayerOptions {
    uint32_t seed=0;      // Varies every random particle; the same seed reproduces a run exactly.
    bool scripts=true;    // Run entity behaviour scripts (they may call game.effect()).
};
// Headless preview of a scene's effects and timeline, without gameplay or a player: entity
// bob/spin animation, behaviour scripts, timeline tracks/events, and particles advance at a fixed
// 120 Hz tick. No window or GPU; frame() returns the snapshot a renderer would draw.
class FxPlayer {
public:
    static constexpr double tickSeconds=1.0/120.0;
    explicit FxPlayer(const SceneDocument& document,FxPlayerOptions options={});
    static FxPlayer load(const std::filesystem::path& scene,FxPlayerOptions options={});
    ~FxPlayer();
    FxPlayer(FxPlayer&&) noexcept;
    FxPlayer& operator=(FxPlayer&&) noexcept;
    // Advances by whole ticks covering `seconds` (0 is allowed); returns the tick count.
    uint64_t step(double seconds);
    // Moves to `seconds` from the start, restarting the run when seeking backwards.
    void seek(double seconds);
    void restart();
    double time() const { return double(tickCount)*tickSeconds; }
    uint64_t ticks() const { return tickCount; }
    float timelineTime() const;
    // Timeline length in seconds, or 0 without a timeline.
    float duration() const;
    const Scene& scene() const;
    const FxRuntime& effects() const;
    FxStats stats() const;
    // Starts a one-shot effect now (see FxRuntime::play).
    void play(const std::string& effect,glm::vec3 position,float yaw=0,const std::string& entity={},uint32_t seed=0);
    // The timeline shot camera at the current time, else the automatic framing.
    Camera camera() const;
    // Frames the entities that carry effects, timeline events, and animated entities (or, without
    // any, every entity) from the front and slightly above. Computed once at the start.
    Camera autoCamera() const;
    RenderFrame frame(std::optional<Camera> camera={}) const;
    // Behaviour script output and errors since the last call.
    std::vector<std::string> takeMessages();
    struct Impl;
private:
    std::unique_ptr<Impl> impl;
    uint64_t tickCount=0;
};
}
