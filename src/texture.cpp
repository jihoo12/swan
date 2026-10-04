#include "texture.hpp"
#include <png.h>
#include <algorithm>
#include <cmath>
#include <fstream>
#include <stdexcept>
namespace swan {
std::vector<TextureData> buildMipChain(const TextureData& texture) {
    if (!texture.width || !texture.height || texture.width>4096 || texture.height>4096 ||
        texture.rgba.size()!=size_t(texture.width)*texture.height*4)
        throw std::invalid_argument("Invalid mipmap source");
    auto linear=[](uint8_t value) {
        double s=value/255.0;
        return s<=0.04045 ? s/12.92 : std::pow((s+0.055)/1.055,2.4);
    };
    auto encoded=[](double value) {
        double s=value<=0.0031308 ? 12.92*value : 1.055*std::pow(value,1.0/2.4)-0.055;
        return static_cast<uint8_t>(std::clamp(std::lround(s*255),0l,255l));
    };
    std::vector<TextureData> levels{texture};
    while (levels.back().width>1 || levels.back().height>1) {
        const auto& source=levels.back();
        TextureData next{std::max(1u,source.width/2),std::max(1u,source.height/2),{}};
        next.rgba.resize(size_t(next.width)*next.height*4);
        // Area filtering includes the last row/column of odd-sized images.
        for (uint32_t y=0;y<next.height;++y) for (uint32_t x=0;x<next.width;++x) {
            double left=double(x)*source.width/next.width, right=double(x+1)*source.width/next.width;
            double top=double(y)*source.height/next.height, bottom=double(y+1)*source.height/next.height;
            double sum[4]{}, area=(right-left)*(bottom-top);
            for (uint32_t sy=uint32_t(top);sy<std::ceil(bottom);++sy)
                for (uint32_t sx=uint32_t(left);sx<std::ceil(right);++sx) {
                    double weight=(std::min(right,double(sx+1))-std::max(left,double(sx))) *
                                  (std::min(bottom,double(sy+1))-std::max(top,double(sy)));
                    auto offset=(size_t(sy)*source.width+sx)*4;
                    for (int c=0;c<3;++c) sum[c]+=linear(source.rgba[offset+c])*weight;
                    sum[3]+=source.rgba[offset+3]*weight;
                }
            auto offset=(size_t(y)*next.width+x)*4;
            for (int c=0;c<3;++c) next.rgba[offset+c]=encoded(sum[c]/area);
            next.rgba[offset+3]=static_cast<uint8_t>(std::lround(sum[3]/area));
        }
        levels.push_back(std::move(next));
    }
    return levels;
}
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
