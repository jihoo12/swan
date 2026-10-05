#pragma once
#include "fx_player.hpp"
#include "texture.hpp"
#include <functional>
#include <optional>
#include <vector>
namespace swan {
// Draws a render snapshot into an RGBA8 sRGB image of `size` pixels. The swan executable supplies
// a headless Vulkan implementation; the SDK itself stays window- and GPU-free.
using ImageRenderer=std::function<TextureData(const RenderFrame& frame,glm::uvec2 size)>;
struct CaptureOptions {
    glm::uvec2 size{640,360};
    uint32_t supersample=2;          // Render at size*N and average down (1..4) for smooth edges.
    std::optional<Camera> camera;    // Default: FxPlayer::camera().
};
// Renders the player's current state.
TextureData captureFrame(const FxPlayer& player,const ImageRenderer& renderer,const CaptureOptions& options);
struct SheetOptions {
    CaptureOptions frame{{320,180},2,std::nullopt};
    double start=0,end=-1;           // end < 0: the timeline length, or 2 seconds without one.
    uint32_t count=8,columns=4;
    bool labels=true;                // "t=0.50s 120p": time and live particle count.
};
// Seeks to `count` evenly spaced times (start and end included) and tiles the frames.
TextureData captureSheet(FxPlayer& player,const ImageRenderer& renderer,const SheetOptions& options);
std::vector<double> sheetTimes(double start,double end,uint32_t count);
}
