#include "fx_player.hpp"
#include "script_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
namespace swan {
namespace {
Camera frameScene(const Scene& scene) {
    std::vector<glm::vec3> points;
    auto worldOf=[&](const std::string& key)->std::optional<glm::vec3>{
        auto id=scene.find(key);
        if(!scene.get(id)) return std::nullopt;
        return scene.worldTransform(id).position;
    };
    for(auto id:scene.entities()) if(!scene.get(id)->effectId.empty()) points.push_back(scene.worldTransform(id).position);
    for(const auto& event:scene.timeline().events) {
        if(event.entity.empty()) points.push_back(event.position);
        else if(auto p=worldOf(event.entity)) points.push_back(*p+event.position);
    }
    for(const auto& track:scene.timeline().tracks) {
        if(track.kind!=TrackTarget::Entity || track.property!=TrackProperty::Position) continue;
        auto id=scene.find(track.target);
        if(!scene.get(id)) continue;
        auto parent=scene.parent(id);
        for(const auto& key:track.vector.keys) {
            Transform local=scene.get(id)->transform;local.position=key.value;
            points.push_back(parent?composeTransform(scene.worldTransform(*parent),local).position:key.value);
        }
    }
    if(points.empty()) for(auto id:scene.entities()) points.push_back(scene.worldTransform(id).position);
    glm::vec3 low(0),high(0);
    if(!points.empty()) {
        low=high=points.front();
        for(auto p:points) {low=glm::min(low,p);high=glm::max(high,p);}
    }
    auto center=(low+high)*0.5f;
    float radius=std::max(glm::length(high-low)*0.5f,1.0f)+1.5f;
    constexpr float fov=55;
    float distance=radius/std::tan(fov*0.5f*3.14159265f/180)*1.15f;
    auto direction=glm::normalize(glm::vec3(0.3f,0.42f,1));
    return lookAt(center+direction*distance,center,fov);
}
}
struct FxPlayer::Impl {
    SceneDocument source;
    FxPlayerOptions options;
    Scene scene;
    std::unique_ptr<FxRuntime> fx;
    std::unique_ptr<ScriptRuntime> scripts;
    std::vector<std::string> messages;
    Camera automatic;
    float time=0;
    void start() {
        scripts.reset();fx.reset();
        scene=source.scene;
        fx=std::make_unique<FxRuntime>(scene,options.seed);
        automatic=frameScene(scene);
        time=0;
        bool scripted=false;
        for(auto id:scene.entities()) scripted|=!scene.get(id)->scriptId.empty();
        if(options.scripts && scripted) {
            scripts=std::make_unique<ScriptRuntime>(scene,[this](const std::string& line){
                messages.push_back(line);
                if(messages.size()>256) messages.erase(messages.begin());
            });
            scripts->setEffectPlayer([this](const std::string& effect,glm::vec3 position,float yaw,const std::string& entity,uint32_t seed){
                fx->play(scene,effect,position,yaw,entity,seed);
            });
        }
    }
    void tick() {
        float dt=float(tickSeconds);
        time+=dt;
        // The same procedural bob/spin the game applies.
        for(auto id:scene.entities()) {
            auto* entity=scene.get(id);
            if(!entity->animation) continue;
            const auto& a=*entity->animation;
            entity->transform.position.y=a.baseHeight+std::sin(time*1.3f+a.phase)*a.bob;
            entity->transform.yaw=std::remainder(entity->transform.yaw+dt*a.speed,6.2831853f);
        }
        if(scripts) scripts->update(dt,ScriptGameState{time,{},false,false,0,0,{}});
        fx->update(scene,dt);
    }
};
FxPlayer::FxPlayer(const SceneDocument& document,FxPlayerOptions options):impl(std::make_unique<Impl>()) {
    impl->source=document;impl->options=options;
    impl->source.scene.validate();
    impl->start();
}
FxPlayer FxPlayer::load(const std::filesystem::path& scene,FxPlayerOptions options) {return FxPlayer(loadScene(scene),options);}
FxPlayer::~FxPlayer()=default;
FxPlayer::FxPlayer(FxPlayer&&) noexcept=default;
FxPlayer& FxPlayer::operator=(FxPlayer&&) noexcept=default;
uint64_t FxPlayer::step(double seconds) {
    if(!std::isfinite(seconds) || seconds<0) throw std::invalid_argument("Effect preview step must be finite and nonnegative");
    if(seconds>3600) throw std::invalid_argument("Effect preview steps are limited to one hour");
    auto count=uint64_t(std::llround(seconds/tickSeconds));
    for(uint64_t i=0;i<count;++i) {impl->tick();++tickCount;}
    return count;
}
void FxPlayer::seek(double seconds) {
    if(!std::isfinite(seconds) || seconds<0) throw std::invalid_argument("Effect preview time must be finite and nonnegative");
    if(seconds>3600) throw std::invalid_argument("Effect preview time is limited to one hour");
    auto target=uint64_t(std::llround(seconds/tickSeconds));
    if(target<tickCount) restart();
    while(tickCount<target) {impl->tick();++tickCount;}
}
void FxPlayer::restart() {impl->start();tickCount=0;}
float FxPlayer::timelineTime() const {return impl->fx->timelineTime();}
float FxPlayer::duration() const {return impl->scene.timeline().length();}
const Scene& FxPlayer::scene() const {return impl->scene;}
const FxRuntime& FxPlayer::effects() const {return *impl->fx;}
FxStats FxPlayer::stats() const {return impl->fx->stats();}
void FxPlayer::play(const std::string& effect,glm::vec3 position,float yaw,const std::string& entity,uint32_t seed) {
    impl->fx->play(impl->scene,effect,position,yaw,entity,seed);
}
Camera FxPlayer::camera() const {
    if(auto shot=impl->scene.timeline().cameraAt(timelineTime(),impl->automatic)) return *shot;
    return impl->automatic;
}
Camera FxPlayer::autoCamera() const {return impl->automatic;}
RenderFrame FxPlayer::frame(std::optional<Camera> camera) const {
    RenderFrame frame;
    frame.camera=camera?*camera:this->camera();
    frame.time=float(time());
    const auto& scene=impl->scene;
    frame.environment=scene.environment();
    frame.objects.reserve(scene.size());
    for(auto id:scene.entities()) {
        const auto* entity=scene.get(id);
        const auto& material=scene.assets().get(entity->materialId);
        frame.objects.push_back({scene.worldTransform(id),material,scene.meshes().get(entity->meshId).data,scene.textures().get(material.textureId).data});
    }
    impl->fx->appendTo(frame,scene);
    return frame;
}
std::vector<std::string> FxPlayer::takeMessages() {return std::exchange(impl->messages,{});}
}
