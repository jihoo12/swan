#include "fx_capture.hpp"
#include "image.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>
namespace swan {
TextureData captureFrame(const FxPlayer& player,const ImageRenderer& renderer,const CaptureOptions& options) {
    if(!renderer) throw std::runtime_error("Rendering needs a GPU renderer; run inside the swan executable (swan script / swan render)");
    if(options.size.x<16 || options.size.y<16 || options.size.x>4096 || options.size.y>4096) throw std::invalid_argument("Capture size must be within 16..4096 pixels");
    if(options.supersample<1 || options.supersample>4) throw std::invalid_argument("Supersampling must be within 1..4");
    auto scaled=options.size*options.supersample;
    if(scaled.x>8192 || scaled.y>8192) throw std::invalid_argument("Supersampled capture exceeds 8192 pixels; lower the size or factor");
    auto image=renderer(player.frame(options.camera),scaled);
    if(image.width!=scaled.x || image.height!=scaled.y) throw std::runtime_error("Renderer returned an image of the wrong size");
    return downsample(image,options.supersample);
}
std::vector<double> sheetTimes(double start,double end,uint32_t count) {
    if(!std::isfinite(start) || !std::isfinite(end) || start<0 || end<start) throw std::invalid_argument("Sheet times need 0 <= start <= end");
    if(count<1 || count>64) throw std::invalid_argument("A sheet holds 1..64 frames");
    std::vector<double> times;
    for(uint32_t i=0;i<count;++i) times.push_back(count==1?start:start+(end-start)*double(i)/double(count-1));
    return times;
}
TextureData captureSheet(FxPlayer& player,const ImageRenderer& renderer,const SheetOptions& options) {
    double end=options.end>=0?options.end:(player.duration()>0?player.duration():2.0);
    std::vector<TextureData> frames;std::vector<std::string> labels;
    for(double time:sheetTimes(options.start,end,options.count)) {
        player.seek(time);
        frames.push_back(captureFrame(player,renderer,options.frame));
        char label[64];
        std::snprintf(label,sizeof label,"t=%.2fs %zup",player.time(),player.effects().particleCount());
        labels.push_back(options.labels?label:"");
    }
    return contactSheet(frames,labels,options.columns);
}
}
