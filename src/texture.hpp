#pragma once
#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>
namespace swan {
struct TextureData { uint32_t width=1,height=1; std::vector<uint8_t> rgba; };
using SharedTexture=std::shared_ptr<const TextureData>;
// Complete RGBA8 sRGB chain, including the original level.
std::vector<TextureData> buildMipChain(const TextureData& texture);
SharedTexture whiteTexture();
SharedTexture loadPngTexture(const std::filesystem::path& path);
}
