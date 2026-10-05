#pragma once
#include "texture.hpp"
#include <glm/glm.hpp>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>
namespace swan {
// CPU helpers for rendered RGBA8 sRGB images (TextureData): PNG output, supersample
// reduction, labels, and contact sheets. No GPU involved.
void savePng(const std::filesystem::path& path,const TextureData& image);
// Averages factor x factor blocks in linear light; dimensions must be multiples of factor.
TextureData downsample(const TextureData& image,uint32_t factor);
// Built-in 5x7 pixel font (ASCII letters, digits, and . : = - / ( ) _ + , %), scaled by `scale`.
void drawText(TextureData& image,int x,int y,std::string_view text,glm::u8vec4 color,int scale=1);
// Equal-size frames in a grid, each with an optional label in its top-left corner.
TextureData contactSheet(const std::vector<TextureData>& frames,const std::vector<std::string>& labels,uint32_t columns);
}
