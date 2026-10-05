#pragma once
#include "input.hpp"
#include "render_frame.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace swan {
// Implement this interface to build another game with the same engine runtime.
class GameLayer {
public:
    virtual ~GameLayer() = default;
    virtual void handleInput(const Input& input) = 0;
    virtual void fixedUpdate(float dt,const Input& input) = 0;
    virtual RenderFrame renderFrame(float interpolation) const = 0;
    virtual std::string status() const = 0;
    // Return false to veto a window-close request (e.g. to ask about unsaved changes).
    virtual bool allowClose() { return true; }
    // Return false to end the run loop from inside the layer.
    virtual bool running() const { return true; }
    // nullopt keeps the renderer's click-to-capture behavior; a value takes explicit control.
    virtual std::optional<bool> cursorCapture() const { return std::nullopt; }
    // Frames per second the runtime should not exceed (0: unlimited, presentation/vsync only).
    virtual int frameRateLimit() const { return 0; }
    // Log lines (e.g. from scripts) since the last call; the runtime prints them.
    virtual std::vector<std::string> takeMessages() { return {}; }
};
}
