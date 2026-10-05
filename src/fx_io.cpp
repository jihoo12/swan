#include "fx_io.hpp"
#include "fx_json.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
namespace swan {
namespace {
using Json=nlohmann::json;
std::string join(std::initializer_list<std::string_view> names) {
    std::string text;
    for(auto name:names) {if(!text.empty()) text+=", ";text+=name;}
    return text;
}
// An empty Lua table arrives as [], so treat [] as an empty object too.
bool isObject(const Json& value) {return value.is_object() || (value.is_array() && value.empty());}
void keys(const Json& value,std::initializer_list<std::string_view> allowed,const char* what) {
    if(!isObject(value)) throw std::invalid_argument(std::string("expected a JSON object for ")+what);
    if(!value.is_object()) return;
    for(auto it=value.begin();it!=value.end();++it)
        if(std::find(allowed.begin(),allowed.end(),it.key())==allowed.end())
            throw std::invalid_argument("unknown field '"+it.key()+"' in "+what+" (allowed: "+join(allowed)+")");
}
// Runs `body` and prefixes any error with the field name.
template<class F> auto field(const char* name,F&& body) {
    try {return body();}
    catch(const std::exception& e) {throw std::invalid_argument(std::string(name)+": "+e.what());}
}
float number(const Json& value) {
    if(!value.is_number()) throw std::invalid_argument("expected a number, got "+value.dump());
    double n=value.get<double>();
    if(!std::isfinite(n) || std::abs(n)>std::numeric_limits<float>::max()) throw std::invalid_argument("number exceeds the finite float range");
    return float(n);
}
bool boolean(const Json& value) {
    if(!value.is_boolean()) throw std::invalid_argument("expected true or false, got "+value.dump());
    return value.get<bool>();
}
std::string text(const Json& value) {
    if(!value.is_string()) throw std::invalid_argument("expected a string, got "+value.dump());
    return value.get<std::string>();
}
uint32_t count(const Json& value,uint32_t maximum) {
    if(!value.is_number_integer() || value.get<int64_t>()<0 || value.get<int64_t>()>int64_t(maximum))
        throw std::invalid_argument("expected an integer 0.."+std::to_string(maximum)+", got "+value.dump());
    return value.get<uint32_t>();
}
glm::vec3 vec3(const Json& value) {
    if(!value.is_array() || value.size()!=3) throw std::invalid_argument("expected [x, y, z], got "+value.dump());
    return {number(value[0]),number(value[1]),number(value[2])};
}
// [r, g, b] (alpha 1) or [r, g, b, a].
glm::vec4 color(const Json& value) {
    if(!value.is_array() || (value.size()!=3 && value.size()!=4)) throw std::invalid_argument("expected [r, g, b] or [r, g, b, a], got "+value.dump());
    return {number(value[0]),number(value[1]),number(value[2]),value.size()==4?number(value[3]):1.0f};
}
Json json(float v) {return jsonNumber(v);}
Json json(glm::vec3 v) {return Json::array({json(v.x),json(v.y),json(v.z)});}
Json json(glm::vec4 v) {return v.a==1?Json::array({json(v.r),json(v.g),json(v.b)}):Json::array({json(v.r),json(v.g),json(v.b),json(v.a)});}
Range range(const Json& value) {
    if(value.is_number()) {float v=number(value);return {v,v};}
    if(!value.is_array() || value.size()!=2) throw std::invalid_argument("expected a number or [min, max], got "+value.dump());
    return {number(value[0]),number(value[1])};
}
Json json(Range r) {return r.min==r.max?json(r.min):Json::array({json(r.min),json(r.max)});}
Ease ease(const Json& value) {
    auto name=text(value);
    if(name=="linear") return Ease::Linear;
    if(name=="step" || name=="hold" || name=="constant") return Ease::Step;
    if(name=="smooth" || name=="ease_in_out" || name=="in_out") return Ease::Smooth;
    if(name=="in" || name=="ease_in") return Ease::In;
    if(name=="out" || name=="ease_out") return Ease::Out;
    throw std::invalid_argument("unknown ease '"+name+"' (use linear, step, smooth, in, out)");
}
template<class T> T value(const Json& v);
template<> float value<float>(const Json& v) {return number(v);}
template<> glm::vec3 value<glm::vec3>(const Json& v) {return vec3(v);}
template<> glm::vec4 value<glm::vec4>(const Json& v) {return color(v);}
// A curve: an array of [time, value], [time, value, ease], or {"t"/"time", "value", "ease"} keys.
// A bare value is a constant curve.
template<class T> Curve<T> curve(const Json& source) {
    Curve<T> result;
    bool bare=source.is_number() || (!std::is_same_v<T,float> && source.is_array() && !source.empty() && source[0].is_number());
    if(bare) {result.keys.push_back({0,value<T>(source)});return result;}
    if(!source.is_array()) throw std::invalid_argument("expected an array of keys, got "+source.dump());
    if(source.size()>maxCurveKeys) throw std::invalid_argument("more than 256 keys");
    for(size_t i=0;i<source.size();++i) {
        const auto& key=source[i];
        try {
            Keyframe<T> frame;
            if(key.is_array()) {
                if(key.size()!=2 && key.size()!=3) throw std::invalid_argument("expected [time, value] or [time, value, ease]");
                frame.time=number(key[0]);frame.value=value<T>(key[1]);
                if(key.size()==3) frame.ease=ease(key[2]);
            } else {
                keys(key,{"t","time","value","ease"},"key");
                if(key.contains("t")==key.contains("time")) throw std::invalid_argument("a key needs exactly one of 't' or 'time'");
                frame.time=number(key.contains("t")?key.at("t"):key.at("time"));
                if(!key.contains("value")) throw std::invalid_argument("a key needs a 'value'");
                frame.value=value<T>(key.at("value"));
                if(key.contains("ease")) frame.ease=ease(key.at("ease"));
            }
            result.keys.push_back(frame);
        } catch(const std::exception& e) {throw std::invalid_argument("key "+std::to_string(i)+": "+e.what());}
    }
    return result;
}
template<class T> Json json(const Curve<T>& c) {
    Json result=Json::array();
    for(const auto& key:c.keys) {
        Json entry=Json::array({json(key.time),json(key.value)});
        if(key.ease!=Ease::Linear) entry.push_back(easeName(key.ease));
        result.push_back(std::move(entry));
    }
    return result;
}
template<class E> E named(const Json& value,std::initializer_list<std::pair<const char*,E>> options,const char* (*name)(E)) {
    auto chosen=text(value);
    for(const auto& [label,option]:options) if(chosen==label) return option;
    std::string allowed;
    for(const auto& [label,option]:options) {(void)label;if(!allowed.empty()) allowed+=", ";allowed+=name(option);}
    throw std::invalid_argument("unknown value '"+chosen+"' (use "+allowed+")");
}
EmitterDef emitterFromJson(const Json& source) {
    keys(source,{"name","offset","delay","duration","loop","rate","rate_over_time","bursts","lifetime","shape","radius","box","surface",
                 "direction","spread","radial","speed","velocity","gravity","drag","noise","noise_frequency","orbit","size","size_over_life",
                 "color","color_over_life","intensity","rotation","spin","stretch","blend","sprite","texture","space","max_particles","seed"},"emitter");
    EmitterDef e;
    if(!source.is_object()) return e;
    auto has=[&](const char* name){return source.contains(name);};
    auto at=[&](const char* name)->const Json&{return source.at(name);};
    if(has("name")) e.name=field("name",[&]{return text(at("name"));});
    if(has("offset")) e.offset=field("offset",[&]{return vec3(at("offset"));});
    if(has("delay")) e.delay=field("delay",[&]{return number(at("delay"));});
    if(has("duration")) e.duration=field("duration",[&]{return number(at("duration"));});
    if(has("loop")) e.loop=field("loop",[&]{return boolean(at("loop"));});
    if(has("rate")) e.rate=field("rate",[&]{return number(at("rate"));});
    if(has("rate_over_time")) e.rateOverTime=field("rate_over_time",[&]{return curve<float>(at("rate_over_time"));});
    if(has("bursts")) e.bursts=field("bursts",[&]{
        const auto& list=at("bursts");
        if(!list.is_array()) throw std::invalid_argument("expected an array of {\"time\", \"count\"} or [time, count]");
        std::vector<Burst> bursts;
        for(const auto& b:list) {
            if(b.is_array()) {
                if(b.size()!=2) throw std::invalid_argument("expected [time, count]");
                bursts.push_back({number(b[0]),count(b[1],maxParticlesPerEmitter)});
            } else {
                keys(b,{"time","t","count"},"burst");
                if(b.contains("t")==b.contains("time") || !b.contains("count")) throw std::invalid_argument("a burst needs 'time' and 'count'");
                bursts.push_back({number(b.contains("t")?b.at("t"):b.at("time")),count(b.at("count"),maxParticlesPerEmitter)});
            }
        }
        return bursts;});
    if(has("lifetime")) e.lifetime=field("lifetime",[&]{return range(at("lifetime"));});
    if(has("shape")) e.shape=field("shape",[&]{return named<EmitterShape>(at("shape"),{{"point",EmitterShape::Point},{"sphere",EmitterShape::Sphere},
        {"box",EmitterShape::Box},{"ring",EmitterShape::Ring},{"disc",EmitterShape::Disc}},shapeName);});
    if(has("radius")) e.radius=field("radius",[&]{return number(at("radius"));});
    if(has("box")) e.size3=field("box",[&]{return vec3(at("box"));});
    if(has("surface")) e.surface=field("surface",[&]{return boolean(at("surface"));});
    if(has("direction")) e.direction=field("direction",[&]{return vec3(at("direction"));});
    if(has("spread")) e.spread=field("spread",[&]{return number(at("spread"));});
    if(has("radial")) e.radial=field("radial",[&]{return boolean(at("radial"));});
    if(has("speed")) e.speed=field("speed",[&]{return range(at("speed"));});
    if(has("velocity")) e.velocity=field("velocity",[&]{return vec3(at("velocity"));});
    if(has("gravity")) e.gravity=field("gravity",[&]{return vec3(at("gravity"));});
    if(has("drag")) e.drag=field("drag",[&]{return number(at("drag"));});
    if(has("noise")) e.noise=field("noise",[&]{return number(at("noise"));});
    if(has("noise_frequency")) e.noiseFrequency=field("noise_frequency",[&]{return number(at("noise_frequency"));});
    if(has("orbit")) e.orbit=field("orbit",[&]{return number(at("orbit"));});
    if(has("size")) e.size=field("size",[&]{return range(at("size"));});
    if(has("size_over_life")) e.sizeOverLife=field("size_over_life",[&]{return curve<float>(at("size_over_life"));});
    if(has("color")) e.color=field("color",[&]{return color(at("color"));});
    if(has("color_over_life")) e.colorOverLife=field("color_over_life",[&]{return curve<glm::vec4>(at("color_over_life"));});
    if(has("intensity")) e.intensity=field("intensity",[&]{return number(at("intensity"));});
    if(has("rotation")) e.rotation=field("rotation",[&]{return range(at("rotation"));});
    if(has("spin")) e.spin=field("spin",[&]{return range(at("spin"));});
    if(has("stretch")) e.stretch=field("stretch",[&]{return number(at("stretch"));});
    if(has("blend")) e.blend=field("blend",[&]{return named<ParticleBlend>(at("blend"),{{"additive",ParticleBlend::Additive},{"alpha",ParticleBlend::Alpha}},blendName);});
    if(has("sprite")) e.sprite=field("sprite",[&]{return named<ParticleSprite>(at("sprite"),{{"soft",ParticleSprite::Soft},{"circle",ParticleSprite::Circle},
        {"ring",ParticleSprite::Ring},{"square",ParticleSprite::Square},{"spark",ParticleSprite::Spark},{"smoke",ParticleSprite::Smoke}},spriteName);});
    if(has("texture")) e.texture=field("texture",[&]{return text(at("texture"));});
    if(has("space")) e.local=field("space",[&]{
        auto space=text(at("space"));
        if(space!="local" && space!="world") throw std::invalid_argument("unknown value '"+space+"' (use world, local)");
        return space=="local";});
    if(has("max_particles")) e.maxParticles=field("max_particles",[&]{return count(at("max_particles"),maxParticlesPerEmitter);});
    if(has("seed")) e.seed=field("seed",[&]{return count(at("seed"),std::numeric_limits<uint32_t>::max());});
    return e;
}
bool same(Range a,Range b) {return a.min==b.min && a.max==b.max;}
Json emitterToJson(const EmitterDef& e) {
    const EmitterDef d;
    Json out=Json::object();
    if(!e.name.empty()) out["name"]=e.name;
    if(e.offset!=d.offset) out["offset"]=json(e.offset);
    if(e.delay!=d.delay) out["delay"]=json(e.delay);
    out["duration"]=json(e.duration);
    if(e.loop) out["loop"]=true;
    if(e.rate!=d.rate) out["rate"]=json(e.rate);
    if(!e.rateOverTime.empty()) out["rate_over_time"]=json(e.rateOverTime);
    if(!e.bursts.empty()) {
        out["bursts"]=Json::array();
        for(const auto& b:e.bursts) out["bursts"].push_back({{"time",json(b.time)},{"count",b.count}});
    }
    out["lifetime"]=json(e.lifetime);
    if(e.shape!=d.shape) out["shape"]=shapeName(e.shape);
    if(e.shape==EmitterShape::Sphere || e.shape==EmitterShape::Ring || e.shape==EmitterShape::Disc || e.radius!=d.radius) out["radius"]=json(e.radius);
    if(e.shape==EmitterShape::Box || e.size3!=d.size3) out["box"]=json(e.size3);
    if(e.surface) out["surface"]=true;
    if(e.direction!=d.direction) out["direction"]=json(e.direction);
    if(e.spread!=d.spread) out["spread"]=json(e.spread);
    if(e.radial) out["radial"]=true;
    if(!same(e.speed,d.speed)) out["speed"]=json(e.speed);
    if(e.velocity!=d.velocity) out["velocity"]=json(e.velocity);
    if(e.gravity!=d.gravity) out["gravity"]=json(e.gravity);
    if(e.drag!=d.drag) out["drag"]=json(e.drag);
    if(e.noise!=d.noise) out["noise"]=json(e.noise);
    if(e.noiseFrequency!=d.noiseFrequency) out["noise_frequency"]=json(e.noiseFrequency);
    if(e.orbit!=d.orbit) out["orbit"]=json(e.orbit);
    out["size"]=json(e.size);
    if(!e.sizeOverLife.empty()) out["size_over_life"]=json(e.sizeOverLife);
    if(e.color!=d.color) out["color"]=json(e.color);
    if(!e.colorOverLife.empty()) out["color_over_life"]=json(e.colorOverLife);
    if(e.intensity!=d.intensity) out["intensity"]=json(e.intensity);
    if(!same(e.rotation,d.rotation)) out["rotation"]=json(e.rotation);
    if(!same(e.spin,d.spin)) out["spin"]=json(e.spin);
    if(e.stretch!=d.stretch) out["stretch"]=json(e.stretch);
    if(e.blend!=d.blend) out["blend"]=blendName(e.blend);
    if(e.sprite!=d.sprite) out["sprite"]=spriteName(e.sprite);
    if(!e.texture.empty()) out["texture"]=e.texture;
    if(e.local) out["space"]="local";
    if(e.maxParticles!=d.maxParticles) out["max_particles"]=e.maxParticles;
    if(e.seed!=d.seed) out["seed"]=e.seed;
    return out;
}
TrackProperty property(const std::string& name) {
    for(auto p:{TrackProperty::Position,TrackProperty::Scale,TrackProperty::Yaw,TrackProperty::Emit,TrackProperty::Color,TrackProperty::Emission,
                TrackProperty::Exposure,TrackProperty::Bloom,TrackProperty::Background})
        if(name==propertyName(p)) return p;
    throw std::invalid_argument("unknown property '"+name+"' (entities: position, scale, yaw, emit; materials: color, emission; environment: exposure, bloom, background)");
}
TimelineTrack trackFromJson(const Json& source) {
    keys(source,{"entity","material","environment","property","keys"},"track");
    TimelineTrack track;
    int targets=int(source.contains("entity"))+int(source.contains("material"))+int(source.contains("environment"));
    if(targets!=1) throw std::invalid_argument("a track needs exactly one of 'entity', 'material', or 'environment' (true)");
    if(source.contains("environment")) {
        if(!boolean(source.at("environment"))) throw std::invalid_argument("'environment' must be true");
        track.kind=TrackTarget::Environment;
    } else {
        track.kind=source.contains("material")?TrackTarget::Material:TrackTarget::Entity;
        track.target=text(source.at(track.kind==TrackTarget::Material?"material":"entity"));
    }
    if(!source.contains("property")) throw std::invalid_argument("a track needs a 'property'");
    track.property=property(text(source.at("property")));
    if(!source.contains("keys")) throw std::invalid_argument("a track needs 'keys'");
    if(track.isVector()) track.vector=field("keys",[&]{return curve<glm::vec3>(source.at("keys"));});
    else track.scalar=field("keys",[&]{return curve<float>(source.at("keys"));});
    return track;
}
TimelineEvent eventFromJson(const Json& source) {
    keys(source,{"time","t","effect","entity","position","offset","yaw","seed"},"event");
    TimelineEvent event;
    if(source.contains("t")==source.contains("time")) throw std::invalid_argument("an event needs exactly one of 't' or 'time'");
    event.time=number(source.contains("t")?source.at("t"):source.at("time"));
    if(!source.contains("effect")) throw std::invalid_argument("an event needs an 'effect'");
    event.effect=text(source.at("effect"));
    if(source.contains("entity")) event.entity=text(source.at("entity"));
    if(source.contains("position") && source.contains("offset")) throw std::invalid_argument("use 'position' or 'offset', not both");
    if(source.contains("position")) event.position=field("position",[&]{return vec3(source.at("position"));});
    if(source.contains("offset")) event.position=field("offset",[&]{return vec3(source.at("offset"));});
    if(source.contains("yaw")) event.yaw=field("yaw",[&]{return number(source.at("yaw"));});
    if(source.contains("seed")) event.seed=field("seed",[&]{return count(source.at("seed"),std::numeric_limits<uint32_t>::max());});
    return event;
}
template<class F> auto indexed(const char* what,size_t i,F&& body) {
    try {return body();}
    catch(const std::exception& e) {throw std::invalid_argument(std::string(what)+" "+std::to_string(i)+": "+e.what());}
}
}
namespace {
// An array is "inline" when it holds only scalars or arrays of scalars (e.g. [0.5, [1, 0, 0]]).
bool inlineArray(const Json& value,int depth=0) {
    if(!value.is_array()) return !value.is_object();
    if(depth>1) return false;
    for(const auto& item:value) if(!inlineArray(item,depth+1)) return false;
    return true;
}
void dump(const Json& value,std::string& out,int indent) {
    if(value.is_object()) {
        if(value.empty()) {out+="{}";return;}
        out+="{\n";
        size_t i=0;
        for(auto it=value.begin();it!=value.end();++it,++i) {
            out.append(size_t(indent+2),' ');
            out+=Json(it.key()).dump();out+=": ";
            dump(it.value(),out,indent+2);
            out+=i+1<value.size()?",\n":"\n";
        }
        out.append(size_t(indent),' ');out+="}";
    } else if(value.is_array()) {
        if(value.empty()) {out+="[]";return;}
        if(inlineArray(value)) {
            out+="[";
            for(size_t i=0;i<value.size();++i) {if(i) out+=", ";dump(value[i],out,indent);}
            out+="]";
            return;
        }
        out+="[\n";
        for(size_t i=0;i<value.size();++i) {
            out.append(size_t(indent+2),' ');
            dump(value[i],out,indent+2);
            out+=i+1<value.size()?",\n":"\n";
        }
        out.append(size_t(indent),' ');out+="]";
    } else out+=value.dump();
}
}
std::string dumpJson(const Json& value) {std::string out;dump(value,out,0);return out;}
EffectDef effectFromJson(const Json& source) {
    keys(source,{"emitters"},"effect");
    if(!source.is_object() || !source.contains("emitters") || !source.at("emitters").is_array()) throw std::invalid_argument("an effect needs an 'emitters' array");
    const auto& list=source.at("emitters");
    if(list.size()>maxEmittersPerEffect) throw std::invalid_argument("an effect supports at most 16 emitters");
    EffectDef effect;
    for(size_t i=0;i<list.size();++i) effect.emitters.push_back(indexed("emitter",i,[&]{return emitterFromJson(list[i]);}));
    validateEffect(effect);
    return effect;
}
Json effectToJson(const EffectDef& effect) {
    Json emitters=Json::array();
    for(const auto& e:effect.emitters) emitters.push_back(emitterToJson(e));
    return {{"emitters",std::move(emitters)}};
}
Timeline timelineFromJson(const Json& source) {
    keys(source,{"duration","loop","camera","tracks","events"},"timeline");
    Timeline timeline;
    if(!source.is_object()) return timeline;
    if(source.contains("duration")) timeline.duration=field("duration",[&]{return number(source.at("duration"));});
    if(source.contains("loop")) timeline.loop=field("loop",[&]{return boolean(source.at("loop"));});
    if(source.contains("camera")) field("camera",[&]{
        const auto& camera=source.at("camera");
        keys(camera,{"position","target","fov"},"camera");
        if(camera.contains("position")) timeline.camera.position=field("position",[&]{return curve<glm::vec3>(camera.at("position"));});
        if(camera.contains("target")) timeline.camera.target=field("target",[&]{return curve<glm::vec3>(camera.at("target"));});
        if(camera.contains("fov")) timeline.camera.fov=field("fov",[&]{return curve<float>(camera.at("fov"));});
        return 0;});
    if(source.contains("tracks")) {
        const auto& tracks=source.at("tracks");
        if(!tracks.is_array() || tracks.size()>maxTimelineTracks) throw std::invalid_argument("tracks must be an array of at most 512 tracks");
        for(size_t i=0;i<tracks.size();++i) timeline.tracks.push_back(indexed("track",i,[&]{return trackFromJson(tracks[i]);}));
    }
    if(source.contains("events")) {
        const auto& events=source.at("events");
        if(!events.is_array() || events.size()>maxTimelineEvents) throw std::invalid_argument("events must be an array of at most 1024 events");
        for(size_t i=0;i<events.size();++i) timeline.events.push_back(indexed("event",i,[&]{return eventFromJson(events[i]);}));
        std::stable_sort(timeline.events.begin(),timeline.events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
    }
    validateTimeline(timeline);
    return timeline;
}
Json timelineToJson(const Timeline& timeline) {
    Json out=Json::object();
    if(timeline.duration>0) out["duration"]=json(timeline.duration);
    if(timeline.loop) out["loop"]=true;
    if(!timeline.camera.empty()) {
        out["camera"]=Json::object();
        if(!timeline.camera.position.empty()) out["camera"]["position"]=json(timeline.camera.position);
        if(!timeline.camera.target.empty()) out["camera"]["target"]=json(timeline.camera.target);
        if(!timeline.camera.fov.empty()) out["camera"]["fov"]=json(timeline.camera.fov);
    }
    if(!timeline.tracks.empty()) {
        out["tracks"]=Json::array();
        for(const auto& track:timeline.tracks) {
            Json entry=Json::object();
            if(track.kind==TrackTarget::Environment) entry["environment"]=true;
            else entry[track.kind==TrackTarget::Material?"material":"entity"]=track.target;
            entry["property"]=propertyName(track.property);
            entry["keys"]=track.isVector()?json(track.vector):json(track.scalar);
            out["tracks"].push_back(std::move(entry));
        }
    }
    if(!timeline.events.empty()) {
        out["events"]=Json::array();
        for(const auto& event:timeline.events) {
            Json e={{"time",json(event.time)},{"effect",event.effect}};
            if(!event.entity.empty()) e["entity"]=event.entity;
            if(event.position!=glm::vec3(0) || event.entity.empty()) e[event.entity.empty()?"position":"offset"]=json(event.position);
            if(event.yaw!=0) e["yaw"]=json(event.yaw);
            if(event.seed) e["seed"]=event.seed;
            out["events"].push_back(std::move(e));
        }
    }
    return out;
}
Environment environmentFromJson(const Json& source,Environment e) {
    keys(source,{"background","fog","ambient","sun","local_light","exposure","bloom","bloom_threshold","tonemap"},"environment");
    if(!source.is_object()) return e;
    auto has=[&](const char* name){return source.contains(name);};
    if(has("background")) e.background=field("background",[&]{return vec3(source.at("background"));});
    if(has("fog")) e.fog=field("fog",[&]{return number(source.at("fog"));});
    if(has("ambient")) e.ambient=field("ambient",[&]{return number(source.at("ambient"));});
    if(has("sun")) e.sun=field("sun",[&]{return number(source.at("sun"));});
    if(has("local_light")) e.localLight=field("local_light",[&]{return number(source.at("local_light"));});
    if(has("exposure")) e.exposure=field("exposure",[&]{return number(source.at("exposure"));});
    if(has("bloom")) e.bloom=field("bloom",[&]{return number(source.at("bloom"));});
    if(has("bloom_threshold")) e.bloomThreshold=field("bloom_threshold",[&]{return number(source.at("bloom_threshold"));});
    if(has("tonemap")) e.toneMap=field("tonemap",[&]{
        auto name=text(source.at("tonemap"));
        if(name=="reinhard") return ToneMap::Reinhard;
        if(name=="aces") return ToneMap::Aces;
        throw std::invalid_argument("unknown tone map '"+name+"' (use reinhard, aces)");});
    Scene check;check.setEnvironment(e); // Range validation.
    return e;
}
Json environmentToJson(const Environment& e) {
    return {{"background",json(e.background)},{"fog",json(e.fog)},{"ambient",json(e.ambient)},{"sun",json(e.sun)},{"local_light",json(e.localLight)},
            {"exposure",json(e.exposure)},{"bloom",json(e.bloom)},{"bloom_threshold",json(e.bloomThreshold)},{"tonemap",e.toneMap==ToneMap::Aces?"aces":"reinhard"}};
}
EffectDef parseEffect(std::string_view text) {
    try {return effectFromJson(Json::parse(text));}
    catch(const Json::exception& e) {throw std::invalid_argument(std::string("Invalid effect JSON: ")+e.what());}
}
std::string serializeEffect(const EffectDef& effect) {return dumpJson(effectToJson(effect));}
Timeline parseTimeline(std::string_view text) {
    try {return timelineFromJson(Json::parse(text));}
    catch(const Json::exception& e) {throw std::invalid_argument(std::string("Invalid timeline JSON: ")+e.what());}
}
std::string serializeTimeline(const Timeline& timeline) {return dumpJson(timelineToJson(timeline));}
}
