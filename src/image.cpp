#include "image.hpp"
#include <png.h>
#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <stdexcept>
namespace swan {
namespace {
void checkImage(const TextureData& image) {
    if(!image.width || !image.height || image.rgba.size()!=size_t(image.width)*image.height*4) throw std::invalid_argument("Invalid image dimensions/pixels");
}
float linear(uint8_t value) {
    float s=value/255.0f;
    return s<=0.04045f?s/12.92f:std::pow((s+0.055f)/1.055f,2.4f);
}
uint8_t encoded(float value) {
    float s=value<=0.0031308f?12.92f*value:1.055f*std::pow(value,1.0f/2.4f)-0.055f;
    return uint8_t(std::clamp(std::lround(s*255),0l,255l));
}
const std::array<uint8_t,7>* glyph(char c) {
    static const std::array<uint8_t,7> digits[10]={
        {0x0E,0x11,0x13,0x15,0x19,0x11,0x0E},{0x04,0x0C,0x04,0x04,0x04,0x04,0x0E},{0x0E,0x11,0x01,0x02,0x04,0x08,0x1F},
        {0x1F,0x02,0x04,0x02,0x01,0x11,0x0E},{0x02,0x06,0x0A,0x12,0x1F,0x02,0x02},{0x1F,0x10,0x1E,0x01,0x01,0x11,0x0E},
        {0x06,0x08,0x10,0x1E,0x11,0x11,0x0E},{0x1F,0x01,0x02,0x04,0x08,0x08,0x08},{0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E},
        {0x0E,0x11,0x11,0x0F,0x01,0x02,0x0C}};
    static const std::array<uint8_t,7> letters[26]={
        {0x0E,0x11,0x11,0x1F,0x11,0x11,0x11},{0x1E,0x11,0x11,0x1E,0x11,0x11,0x1E},{0x0E,0x11,0x10,0x10,0x10,0x11,0x0E},
        {0x1C,0x12,0x11,0x11,0x11,0x12,0x1C},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F},{0x1F,0x10,0x10,0x1E,0x10,0x10,0x10},
        {0x0E,0x11,0x10,0x17,0x11,0x11,0x0F},{0x11,0x11,0x11,0x1F,0x11,0x11,0x11},{0x0E,0x04,0x04,0x04,0x04,0x04,0x0E},
        {0x07,0x02,0x02,0x02,0x02,0x12,0x0C},{0x11,0x12,0x14,0x18,0x14,0x12,0x11},{0x10,0x10,0x10,0x10,0x10,0x10,0x1F},
        {0x11,0x1B,0x15,0x15,0x11,0x11,0x11},{0x11,0x11,0x19,0x15,0x13,0x11,0x11},{0x0E,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x1E,0x11,0x11,0x1E,0x10,0x10,0x10},{0x0E,0x11,0x11,0x11,0x15,0x12,0x0D},{0x1E,0x11,0x11,0x1E,0x14,0x12,0x11},
        {0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E},{0x1F,0x04,0x04,0x04,0x04,0x04,0x04},{0x11,0x11,0x11,0x11,0x11,0x11,0x0E},
        {0x11,0x11,0x11,0x11,0x11,0x0A,0x04},{0x11,0x11,0x11,0x15,0x15,0x15,0x0A},{0x11,0x11,0x0A,0x04,0x0A,0x11,0x11},
        {0x11,0x11,0x11,0x0A,0x04,0x04,0x04},{0x1F,0x01,0x02,0x04,0x08,0x10,0x1F}};
    static const std::pair<char,std::array<uint8_t,7>> symbols[]={
        {'.',{0,0,0,0,0,0x0C,0x0C}},{':',{0,0x0C,0x0C,0,0x0C,0x0C,0}},{'=',{0,0,0x1F,0,0x1F,0,0}},{'-',{0,0,0,0x1F,0,0,0}},
        {'/',{0,0x01,0x02,0x04,0x08,0x10,0}},{'(',{0x02,0x04,0x08,0x08,0x08,0x04,0x02}},{')',{0x08,0x04,0x02,0x02,0x02,0x04,0x08}},
        {'_',{0,0,0,0,0,0,0x1F}},{'+',{0,0x04,0x04,0x1F,0x04,0x04,0}},{',',{0,0,0,0,0x0C,0x04,0x08}},{'%',{0x18,0x19,0x02,0x04,0x08,0x13,0x03}}};
    if(c>='0' && c<='9') return &digits[c-'0'];
    if(std::isalpha(static_cast<unsigned char>(c))) return &letters[std::toupper(static_cast<unsigned char>(c))-'A'];
    for(const auto& [symbol,bits]:symbols) if(symbol==c) return &bits;
    return nullptr;
}
void fill(TextureData& image,int x,int y,int w,int h,glm::u8vec4 color) {
    for(int py=std::max(0,y);py<std::min(int(image.height),y+h);++py)
        for(int px=std::max(0,x);px<std::min(int(image.width),x+w);++px) {
            auto* p=&image.rgba[(size_t(py)*image.width+size_t(px))*4];
            float a=color.a/255.0f;
            for(int c=0;c<3;++c) p[c]=uint8_t(std::lround(p[c]*(1-a)+color[c]*a));
            p[3]=255;
        }
}
}
void savePng(const std::filesystem::path& path,const TextureData& image) {
    checkImage(image);
    png_image png{};png.version=PNG_IMAGE_VERSION;png.width=image.width;png.height=image.height;png.format=PNG_FORMAT_RGBA;
    if(!png_image_write_to_file(&png,path.c_str(),0,image.rgba.data(),0,nullptr)) {
        std::string message=png.message;png_image_free(&png);
        throw std::runtime_error(path.string()+": cannot write PNG: "+message);
    }
}
TextureData downsample(const TextureData& image,uint32_t factor) {
    checkImage(image);
    if(factor<=1) return image;
    if(image.width%factor || image.height%factor) throw std::invalid_argument("Image size must be a multiple of the downsample factor");
    TextureData result{image.width/factor,image.height/factor,{}};
    result.rgba.resize(size_t(result.width)*result.height*4);
    float area=float(factor*factor);
    for(uint32_t y=0;y<result.height;++y) for(uint32_t x=0;x<result.width;++x) {
        float sum[4]{};
        for(uint32_t sy=0;sy<factor;++sy) for(uint32_t sx=0;sx<factor;++sx) {
            const auto* p=&image.rgba[(size_t(y*factor+sy)*image.width+x*factor+sx)*4];
            for(int c=0;c<3;++c) sum[c]+=linear(p[c]);
            sum[3]+=p[3];
        }
        auto* out=&result.rgba[(size_t(y)*result.width+x)*4];
        for(int c=0;c<3;++c) out[c]=encoded(sum[c]/area);
        out[3]=uint8_t(std::lround(sum[3]/area));
    }
    return result;
}
void drawText(TextureData& image,int x,int y,std::string_view text,glm::u8vec4 color,int scale) {
    checkImage(image);
    scale=std::max(1,scale);
    int cursor=x;
    for(char c:text) {
        if(const auto* bits=glyph(c))
            for(int row=0;row<7;++row) for(int column=0;column<5;++column)
                if((*bits)[row]&(0x10>>column)) fill(image,cursor+column*scale,y+row*scale,scale,scale,color);
        cursor+=6*scale;
    }
}
TextureData contactSheet(const std::vector<TextureData>& frames,const std::vector<std::string>& labels,uint32_t columns) {
    if(frames.empty()) throw std::invalid_argument("A contact sheet needs at least one frame");
    columns=std::clamp<uint32_t>(columns,1,uint32_t(frames.size()));
    auto width=frames.front().width,height=frames.front().height;
    for(const auto& frame:frames) {
        checkImage(frame);
        if(frame.width!=width || frame.height!=height) throw std::invalid_argument("Contact sheet frames must share one size");
    }
    constexpr uint32_t gap=4;
    auto rows=uint32_t((frames.size()+columns-1)/columns);
    TextureData sheet{columns*width+(columns+1)*gap,rows*height+(rows+1)*gap,{}};
    if(sheet.width>16384 || sheet.height>16384) throw std::invalid_argument("Contact sheet exceeds 16384 pixels; use smaller frames or fewer columns/rows");
    sheet.rgba.assign(size_t(sheet.width)*sheet.height*4,0);
    for(size_t i=0;i<sheet.rgba.size();i+=4) {sheet.rgba[i]=24;sheet.rgba[i+1]=24;sheet.rgba[i+2]=28;sheet.rgba[i+3]=255;}
    int scale=height>=360?2:1;
    for(size_t i=0;i<frames.size();++i) {
        uint32_t left=gap+uint32_t(i%columns)*(width+gap),top=gap+uint32_t(i/columns)*(height+gap);
        for(uint32_t y=0;y<height;++y)
            std::copy_n(&frames[i].rgba[size_t(y)*width*4],size_t(width)*4,&sheet.rgba[(size_t(top+y)*sheet.width+left)*4]);
        if(i<labels.size() && !labels[i].empty()) {
            int w=int(labels[i].size())*6*scale+4*scale;
            fill(sheet,int(left),int(top),w,9*scale+2*scale,{0,0,0,160});
            drawText(sheet,int(left)+2*scale,int(top)+2*scale,labels[i],{255,255,255,255},scale);
        }
    }
    return sheet;
}
}
