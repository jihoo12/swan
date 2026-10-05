#include "timeline.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace swan {
namespace {
void require(bool condition,const std::string& message) {if(!condition) throw std::invalid_argument(message);}
bool finite(glm::vec3 v) {return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z);}
template<class T> void keys(const Curve<T>& curve,const std::string& name) {
    require(curve.keys.size()<=maxCurveKeys,name+" has more than 256 keys");
    for(size_t i=0;i<curve.keys.size();++i) {
        const auto& key=curve.keys[i];
        std::string where=name+" key "+std::to_string(i);
        require(std::isfinite(key.time) && key.time>=0,where+": time must be finite and nonnegative");
        if constexpr(std::is_same_v<T,float>) require(std::isfinite(key.value),where+": value must be finite");
        else require(finite(key.value),where+": value must be finite");
        if(i) require(key.time>=curve.keys[i-1].time,where+": times must not decrease");
    }
}
template<class T> float last(const Curve<T>& curve) {return curve.keys.empty()?0:curve.keys.back().time;}
}
float Timeline::length() const {
    if(duration>0) return duration;
    float end=std::max({last(camera.position),last(camera.target),last(camera.fov)});
    for(const auto& track:tracks) end=std::max(end,track.isVector()?last(track.vector):last(track.scalar));
    for(const auto& event:events) end=std::max(end,event.time);
    return end;
}
float Timeline::localTime(double elapsed) const {
    float end=length();
    if(elapsed<=0 || end<=0) return 0;
    if(loop) return float(std::fmod(elapsed,double(end)));
    return float(std::min(elapsed,double(end)));
}
std::optional<Camera> Timeline::cameraAt(float time,const Camera& fallback) const {
    if(camera.empty()) return std::nullopt;
    auto position=camera.position.empty()?fallback.position:camera.position.sample(time);
    float fov=camera.fov.sample(time,fallback.fov);
    if(camera.target.empty()) {Camera result=fallback;result.position=position;result.fov=fov;return result;}
    return lookAt(position,camera.target.sample(time),fov);
}
void Timeline::removeEntity(const std::string& key) {
    std::erase_if(tracks,[&](const TimelineTrack& track){return track.kind==TrackTarget::Entity && track.target==key;});
    std::erase_if(events,[&](const TimelineEvent& event){return event.entity==key;});
}
void Timeline::removeEffect(const std::string& id) {
    std::erase_if(events,[&](const TimelineEvent& event){return event.effect==id;});
}
void validateTimeline(const Timeline& timeline) {
    require(std::isfinite(timeline.duration) && timeline.duration>=0,"timeline duration must be nonnegative");
    require(timeline.tracks.size()<=maxTimelineTracks,"timeline supports at most 512 tracks");
    require(timeline.events.size()<=maxTimelineEvents,"timeline supports at most 1024 events");
    keys(timeline.camera.position,"camera position");
    keys(timeline.camera.target,"camera target");
    keys(timeline.camera.fov,"camera fov");
    for(const auto& key:timeline.camera.fov.keys) require(key.value>=1 && key.value<=170,"camera fov must be within 1..170 degrees");
    for(size_t i=0;i<timeline.tracks.size();++i) {
        const auto& track=timeline.tracks[i];
        const char* kind=track.kind==TrackTarget::Material?"material ":track.kind==TrackTarget::Environment?"environment":"entity ";
        std::string where="track "+std::to_string(i)+" ("+kind+track.target+" "+propertyName(track.property)+")";
        require((track.kind==TrackTarget::Environment)==track.target.empty(),where+": entity and material tracks need a target; environment tracks none");
        require(propertyTarget(track.property)==track.kind,where+": property does not apply to this target type");
        if(track.isVector()) {require(!track.vector.empty() && track.scalar.empty(),where+": needs vector keys");keys(track.vector,where);}
        else {require(!track.scalar.empty() && track.vector.empty(),where+": needs numeric keys");keys(track.scalar,where);}
        if(track.property==TrackProperty::Scale) for(const auto& key:track.vector.keys) require(key.value.x>0 && key.value.y>0 && key.value.z>0,where+": scale keys must be positive");
        if(track.property==TrackProperty::Color || track.property==TrackProperty::Background)
            for(const auto& key:track.vector.keys) require(key.value.x>=0 && key.value.y>=0 && key.value.z>=0,where+": color keys must be nonnegative");
        if(track.property==TrackProperty::Emit || track.property==TrackProperty::Emission || track.property==TrackProperty::Exposure || track.property==TrackProperty::Bloom)
            for(const auto& key:track.scalar.keys) require(key.value>=0,where+": keys must be nonnegative");
        for(size_t j=0;j<i;++j) {
            const auto& other=timeline.tracks[j];
            require(other.kind!=track.kind || other.target!=track.target || other.property!=track.property,where+": duplicates track "+std::to_string(j));
        }
    }
    for(size_t i=0;i<timeline.events.size();++i) {
        const auto& event=timeline.events[i];
        std::string where="event "+std::to_string(i);
        require(std::isfinite(event.time) && event.time>=0,where+": time must be nonnegative");
        require(!event.effect.empty(),where+": needs an effect");
        require(finite(event.position) && std::isfinite(event.yaw),where+": position and yaw must be finite");
        if(i) require(event.time>=timeline.events[i-1].time,where+": events must be sorted by time");
    }
    require(!timeline.loop || timeline.length()>0,"a looping timeline needs a positive duration or keys");
}
TrackTarget propertyTarget(TrackProperty property) {
    switch(property) {
    case TrackProperty::Color: case TrackProperty::Emission: return TrackTarget::Material;
    case TrackProperty::Exposure: case TrackProperty::Bloom: case TrackProperty::Background: return TrackTarget::Environment;
    default: return TrackTarget::Entity;
    }
}
const char* propertyName(TrackProperty property) {
    switch(property) {
    case TrackProperty::Exposure: return "exposure";
    case TrackProperty::Bloom: return "bloom";
    case TrackProperty::Background: return "background";
    case TrackProperty::Scale: return "scale";
    case TrackProperty::Yaw: return "yaw";
    case TrackProperty::Emit: return "emit";
    case TrackProperty::Color: return "color";
    case TrackProperty::Emission: return "emission";
    default: return "position";
    }
}
Camera lookAt(glm::vec3 position,glm::vec3 target,float fov) {
    Camera camera;camera.position=position;camera.fov=fov;
    auto direction=target-position;
    float length=glm::length(direction);
    if(length>1e-6f) {
        direction/=length;
        camera.yaw=std::atan2(direction.z,direction.x);
        camera.pitch=std::clamp(std::asin(std::clamp(direction.y,-1.0f,1.0f)),-1.5f,1.5f);
    }
    return camera;
}
}
