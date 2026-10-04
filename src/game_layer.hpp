#pragma once
#include "input.hpp"
#include "render_frame.hpp"
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
namespace swan {
// Per-frame data the runtime hands to GUI layers. sceneTexture is an ImTextureID for the
// renderer-owned image of the previous offscreen scene render (0 until one exists).
struct GuiFrame {
    uint64_t sceneTexture=0;
    glm::uvec2 sceneSize{};
    float deltaTime=0,fps=0,contentScale=1;
    uint64_t visibleObjects=0,culledObjects=0;
};
// Implement this interface to build another game with the same engine runtime.
class GameLayer {
public:
    virtual ~GameLayer() = default;
    // Called once after the GUI context exists and before the first GUI frame.
    virtual void initializeGui() {}
    virtual void drawGui(const GuiFrame&) {}
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
    // Log lines (e.g. from scripts) since the last call; the runtime prints them.
    virtual std::vector<std::string> takeMessages() { return {}; }
};
}
