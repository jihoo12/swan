#pragma once
#include "input.hpp"
#include "render_frame.hpp"
#include <string>
namespace swan {
// Implement this interface to build another game with the same engine runtime.
class GameLayer {
public:
    virtual ~GameLayer() = default;
    virtual void drawGui() {}
    virtual void handleInput(const Input& input) = 0;
    virtual void fixedUpdate(float dt,const Input& input) = 0;
    virtual RenderFrame renderFrame(float interpolation) const = 0;
    virtual std::string status() const = 0;
};
}
