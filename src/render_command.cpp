#include "render_command.hpp"
#include "image.hpp"
#include "renderer.hpp"
#include <charconv>
#include <cmath>
#include <optional>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <sstream>
namespace swan {
LazyRenderer::LazyRenderer(bool validation):validation(validation),instance(std::make_shared<std::unique_ptr<VulkanRenderer>>()) {}
LazyRenderer::~LazyRenderer()=default;
ImageRenderer LazyRenderer::renderer() {
    return [state=instance,validation=validation](const RenderFrame& frame,glm::uvec2 size) {
        if(!*state) {
            Options options;options.headless=true;options.validation=validation;
            *state=std::make_unique<VulkanRenderer>(options);
        }
        return (*state)->renderImage(frame,size);
    };
}
void LazyRenderer::finish() {if(*instance) (*instance)->finish();}
namespace {
constexpr const char* usage=R"(Usage: swan render SCENE [OPTIONS]
Render a scene's effects and timeline without a window (headless Vulkan).
  -o, --output PATH       Output PNG (default render.png, or sheet.png with --sheet)
  --time T                Single frame at T seconds (default: half the timeline, or 1)
  --sheet                 Contact sheet of evenly spaced frames, labeled with time and particle count
  --sequence DIR          Numbered frames DIR/frame_0000.png ... at --fps (default 24)
  --from A --to B         Time range for --sheet/--sequence (default 0 .. timeline length or 2)
  --count N --columns C   Sheet frames (default 8) and columns (default 4)
  --size WxH              Frame size (default 640x360; 320x180 per sheet frame)
  --supersample N         Render N times larger and average down, 1..4 (default 2)
  --camera X,Y,Z,TX,TY,TZ[,FOV]  Override the timeline/automatic camera
  --seed N                Particle random seed (default 0)
  --no-scripts            Do not run entity behaviour scripts
  --stats                 Print one JSON line of particle statistics per rendered time to stdout
  --stats-only            Print statistics only; no GPU required
  --validation            Enable Vulkan validation (fails on errors)
)";
double parseNumber(const std::string& text,const char* what) {
    double value=0;
    auto [end,error]=std::from_chars(text.data(),text.data()+text.size(),value);
    if(error!=std::errc{} || end!=text.data()+text.size()) throw std::invalid_argument(std::string(what)+" needs a number, got '"+text+"'");
    return value;
}
std::vector<double> numbers(const std::string& text,const char* what) {
    std::vector<double> values;std::stringstream stream(text);std::string part;
    while(std::getline(stream,part,',')) values.push_back(parseNumber(part,what));
    return values;
}
std::string json(const FxPlayer& player) {
    auto stats=player.stats();
    char head[256];
    std::snprintf(head,sizeof head,R"({"time":%.4f,"timeline_time":%.4f,"particles":%zu,"instances":%zu,"spawned":%llu,"dropped":%llu)",
                  player.time(),double(player.timelineTime()),stats.particles,stats.instances,(unsigned long long)stats.spawned,(unsigned long long)stats.dropped);
    std::string out=head;
    if(stats.particles) {
        char bounds[192];
        std::snprintf(bounds,sizeof bounds,R"(,"bounds":{"min":[%.3f,%.3f,%.3f],"max":[%.3f,%.3f,%.3f]})",
                      stats.minimum.x,stats.minimum.y,stats.minimum.z,stats.maximum.x,stats.maximum.y,stats.maximum.z);
        out+=bounds;
    }
    out+=R"(,"emitters":[)";
    for(size_t i=0;i<stats.emitters.size();++i) {
        const auto& e=stats.emitters[i];
        auto quote=[](const std::string& s){std::string q="\"";for(char c:s) {if(c=='"' || c=='\\') q+='\\';q+=c;}return q+"\"";};
        char counts[96];
        std::snprintf(counts,sizeof counts,R"("particles":%zu,"spawned":%llu,"dropped":%llu)",e.particles,(unsigned long long)e.spawned,(unsigned long long)e.dropped);
        out+=std::string(i?",":"")+"{\"effect\":"+quote(e.effect)+",\"emitter\":"+quote(e.emitter)+(e.entity.empty()?"":",\"entity\":"+quote(e.entity))+","+counts+"}";
    }
    return out+"]}";
}
}
int runRenderCommand(const std::vector<std::string>& args) {
    try {
        if(args.empty() || args.front()=="--help" || args.front()=="-h") {std::cout<<usage;return args.empty()?1:0;}
        std::filesystem::path scene=args.front(),output,sequence;
        std::optional<double> time,from,to;
        bool sheet=false,stats=false,statsOnly=false,validation=false;
        double fps=24;uint32_t count=8,columns=4;
        CaptureOptions capture;bool sized=false;
        FxPlayerOptions playerOptions;
        for(size_t i=1;i<args.size();++i) {
            const auto& arg=args[i];
            auto next=[&]()->const std::string&{
                if(i+1>=args.size()) throw std::invalid_argument(arg+" needs a value");
                return args[++i];
            };
            if(arg=="-o" || arg=="--output") output=next();
            else if(arg=="--time") time=parseNumber(next(),"--time");
            else if(arg=="--sheet") sheet=true;
            else if(arg=="--sequence") sequence=next();
            else if(arg=="--fps") fps=parseNumber(next(),"--fps");
            else if(arg=="--from") from=parseNumber(next(),"--from");
            else if(arg=="--to") to=parseNumber(next(),"--to");
            else if(arg=="--count") count=uint32_t(parseNumber(next(),"--count"));
            else if(arg=="--columns") columns=uint32_t(parseNumber(next(),"--columns"));
            else if(arg=="--size") {
                auto text=next();auto x=text.find('x');
                if(x==std::string::npos) throw std::invalid_argument("--size needs WIDTHxHEIGHT");
                capture.size={uint32_t(parseNumber(text.substr(0,x),"--size")),uint32_t(parseNumber(text.substr(x+1),"--size"))};
                sized=true;
            } else if(arg=="--supersample") capture.supersample=uint32_t(parseNumber(next(),"--supersample"));
            else if(arg=="--camera") {
                auto v=numbers(next(),"--camera");
                if(v.size()!=6 && v.size()!=7) throw std::invalid_argument("--camera needs X,Y,Z,TX,TY,TZ[,FOV]");
                capture.camera=lookAt({v[0],v[1],v[2]},{v[3],v[4],v[5]},v.size()==7?float(v[6]):55.0f);
            } else if(arg=="--seed") playerOptions.seed=uint32_t(parseNumber(next(),"--seed"));
            else if(arg=="--no-scripts") playerOptions.scripts=false;
            else if(arg=="--stats") stats=true;
            else if(arg=="--stats-only") stats=statsOnly=true;
            else if(arg=="--validation") validation=true;
            else throw std::invalid_argument("Unknown render option: "+arg+" (see swan render --help)");
        }
        if(int(sheet)+int(!sequence.empty())+int(time.has_value())>1) throw std::invalid_argument("Choose one of --time, --sheet, or --sequence");
        if(!(fps>0 && fps<=240)) throw std::invalid_argument("--fps must be within 0..240");
        auto player=FxPlayer::load(scene,playerOptions);
        for(const auto& line:player.takeMessages()) std::cerr<<line<<'\n';
        double length=player.duration()>0?player.duration():2.0;
        double start=from.value_or(0),end=to.value_or(length);
        LazyRenderer gpu(validation);
        auto render=gpu.renderer();
        auto report=[&]{if(stats) std::cout<<json(player)<<'\n';for(const auto& line:player.takeMessages()) std::cerr<<line<<'\n';};
        if(sheet) {
            SheetOptions options;options.start=start;options.end=end;options.count=count;options.columns=columns;
            options.frame=capture;if(!sized) options.frame.size={320,180};
            if(output.empty()) output="sheet.png";
            if(statsOnly) {for(double t:sheetTimes(start,end,count)) {player.seek(t);report();}return 0;}
            auto image=captureSheet(player,render,options);
            savePng(output,image);
            if(stats) for(double t:sheetTimes(start,end,count)) {player.seek(t);report();}
            std::cerr<<"Wrote "<<output.string()<<" ("<<image.width<<"x"<<image.height<<", "<<count<<" frames)\n";
        } else if(!sequence.empty()) {
            if(end<start) throw std::invalid_argument("--to must not precede --from");
            auto frames=uint64_t(std::floor((end-start)*fps+1e-6))+1;
            if(frames>10000) throw std::invalid_argument("A sequence is limited to 10000 frames");
            if(!statsOnly) std::filesystem::create_directories(sequence);
            for(uint64_t f=0;f<frames;++f) {
                player.seek(start+double(f)/fps);
                if(!statsOnly) {
                    char name[32];std::snprintf(name,sizeof name,"frame_%04llu.png",(unsigned long long)f);
                    savePng(sequence/name,captureFrame(player,render,capture));
                }
                report();
            }
            if(!statsOnly) std::cerr<<"Wrote "<<frames<<" frames to "<<sequence.string()<<" at "<<fps<<" fps\n";
        } else {
            player.seek(time.value_or(player.duration()>0?player.duration()/2:1.0));
            if(output.empty()) output="render.png";
            if(!statsOnly) {
                savePng(output,captureFrame(player,render,capture));
                std::cerr<<"Wrote "<<output.string()<<" at t="<<player.time()<<"s\n";
            }
            report();
        }
        gpu.finish();
        return 0;
    } catch(const std::exception& error) {std::cerr<<"swan render: "<<error.what()<<'\n';return 1;}
}
}
