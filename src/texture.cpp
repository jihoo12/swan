#include "texture.hpp"
#include <png.h>
#include <fstream>
#include <stdexcept>
namespace swan {
SharedTexture whiteTexture() {
    static const SharedTexture texture=std::make_shared<TextureData>(TextureData{1,1,{255,255,255,255}});
    return texture;
}
SharedTexture loadPngTexture(const std::filesystem::path& path) {
    png_image image{}; image.version=PNG_IMAGE_VERSION;
    try {
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot open PNG");
        auto size=input.tellg();
        if(size<0 || size>16*1024*1024) throw std::runtime_error("PNG exceeds 16 MiB");
        std::vector<uint8_t> encoded(static_cast<size_t>(size)); input.seekg(0);
        input.read(reinterpret_cast<char*>(encoded.data()),size);
        if(!input) throw std::runtime_error("Cannot read PNG");
        if(!png_image_begin_read_from_memory(&image,encoded.data(),encoded.size())) throw std::runtime_error(image.message);
        if(!image.width || !image.height || image.width>4096 || image.height>4096) throw std::runtime_error("PNG dimensions must be 1..4096");
        auto data=std::make_shared<TextureData>(); data->width=image.width; data->height=image.height;
        image.format=PNG_FORMAT_RGBA; data->rgba.resize(PNG_IMAGE_SIZE(image));
        if(!png_image_finish_read(&image,nullptr,data->rgba.data(),0,nullptr)) throw std::runtime_error(image.message);
        png_image_free(&image); return data;
    } catch(const std::exception& e) {
        png_image_free(&image); throw std::runtime_error(path.string()+": "+e.what());
    }
}
}
