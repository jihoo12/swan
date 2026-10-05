#include "fx.hpp"
#include <algorithm>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <type_traits>
namespace swan {
namespace {
bool finite(float value) {return std::isfinite(value);}
bool finite(glm::vec3 v) {return finite(v.x) && finite(v.y) && finite(v.z);}
bool finite(glm::vec4 v) {return finite(v.x) && finite(v.y) && finite(v.z) && finite(v.w);}
void require(bool condition,const std::string& message) {if(!condition) throw std::invalid_argument(message);}
void range(const Range& r,const char* name,float low,float high) {
    require(finite(r.min) && finite(r.max),std::string(name)+" must be finite");
    require(r.min<=r.max,std::string(name)+" minimum exceeds its maximum");
    require(r.min>=low && r.max<=high,std::string(name)+" must be within "+std::to_string(low)+".."+std::to_string(high));
}
template<class T> void curve(const Curve<T>& c,const char* name,bool normalized,bool nonnegative) {
    require(c.keys.size()<=maxCurveKeys,std::string(name)+" has more than 256 keys");
    for(size_t i=0;i<c.keys.size();++i) {
        const auto& key=c.keys[i];
        std::string where=std::string(name)+" key "+std::to_string(i);
        require(finite(key.time) && finite(key.value),where+" must be finite");
        if(normalized) require(key.time>=0 && key.time<=1,where+": time must be within 0..1 (normalized age)");
        else require(key.time>=0,where+": time must be nonnegative");
        if(i) require(key.time>=c.keys[i-1].time,where+": times must not decrease");
        if(nonnegative) {
            if constexpr(std::is_same_v<T,float>) require(key.value>=0,where+": value must be nonnegative");
            else for(int k=0;k<T::length();++k) require(key.value[k]>=0,where+": components must be nonnegative");
        }
    }
}
void emitter(const EmitterDef& e) {
    require(e.name.size()<=64,"name exceeds 64 characters");
    require(finite(e.offset),"offset must be finite");
    require(finite(e.delay) && e.delay>=0,"delay must be nonnegative");
    require(finite(e.duration) && e.duration>0,"duration must be positive");
    require(finite(e.rate) && e.rate>=0 && e.rate<=100000,"rate must be within 0..100000 per second");
    curve(e.rateOverTime,"rate_over_time",true,true);
    require(e.bursts.size()<=64,"at most 64 bursts");
    for(size_t i=0;i<e.bursts.size();++i) {
        const auto& b=e.bursts[i];
        require(finite(b.time) && b.time>=0 && b.time<=e.duration,"burst "+std::to_string(i)+": time must be within 0..duration");
        require(b.count<=maxParticlesPerEmitter,"burst "+std::to_string(i)+": count exceeds 20000");
        if(i) require(b.time>=e.bursts[i-1].time,"burst "+std::to_string(i)+": times must not decrease");
    }
    range(e.lifetime,"lifetime",0.001f,600);
    require(finite(e.radius) && e.radius>=0,"radius must be nonnegative");
    require(finite(e.size3) && e.size3.x>=0 && e.size3.y>=0 && e.size3.z>=0,"box size must be nonnegative");
    require(finite(e.direction),"direction must be finite");
    require(finite(e.spread) && e.spread>=0 && e.spread<=180,"spread must be within 0..180 degrees");
    range(e.speed,"speed",-10000,10000);
    require(finite(e.velocity) && finite(e.gravity),"velocity and gravity must be finite");
    require(finite(e.drag) && e.drag>=0,"drag must be nonnegative");
    require(finite(e.noise) && e.noise>=0,"noise must be nonnegative");
    require(finite(e.noiseFrequency) && e.noiseFrequency>=0,"noise_frequency must be nonnegative");
    require(finite(e.orbit),"orbit must be finite");
    range(e.size,"size",0,10000);
    curve(e.sizeOverLife,"size_over_life",true,true);
    require(finite(e.color) && e.color.r>=0 && e.color.g>=0 && e.color.b>=0 && e.color.a>=0 && e.color.a<=1,"color must be nonnegative with alpha 0..1");
    curve(e.colorOverLife,"color_over_life",true,true);
    require(finite(e.intensity) && e.intensity>=0,"intensity must be nonnegative");
    range(e.rotation,"rotation",-36000,36000);
    range(e.spin,"spin",-36000,36000);
    require(finite(e.stretch) && e.stretch>=0,"stretch must be nonnegative");
    require(e.texture.size()<=256,"texture ID exceeds 256 characters");
    require(e.maxParticles>=1 && e.maxParticles<=maxParticlesPerEmitter,"max_particles must be within 1..20000");
}
}
float applyEase(Ease ease,float t) {
    t=std::clamp(t,0.0f,1.0f);
    switch(ease) {
    case Ease::Step: return t>=1?1.0f:0.0f;
    case Ease::Smooth: return t*t*(3-2*t);
    case Ease::In: return t*t;
    case Ease::Out: return 1-(1-t)*(1-t);
    default: return t;
    }
}
void validateEffect(const EffectDef& effect) {
    require(!effect.emitters.empty(),"an effect needs at least one emitter");
    require(effect.emitters.size()<=maxEmittersPerEffect,"an effect supports at most 16 emitters");
    for(size_t i=0;i<effect.emitters.size();++i) {
        const auto& e=effect.emitters[i];
        try {emitter(e);}
        catch(const std::exception& error) {
            throw std::invalid_argument("emitter "+std::to_string(i)+(e.name.empty()?"":" ("+e.name+")")+": "+error.what());
        }
    }
}
float effectDuration(const EffectDef& effect) {
    float end=0;
    for(const auto& e:effect.emitters) {
        if(e.loop) return std::numeric_limits<float>::infinity();
        end=std::max(end,e.delay+e.duration+e.lifetime.max);
    }
    return end;
}
const char* easeName(Ease ease) {
    switch(ease) {
    case Ease::Step: return "step";
    case Ease::Smooth: return "smooth";
    case Ease::In: return "in";
    case Ease::Out: return "out";
    default: return "linear";
    }
}
const char* shapeName(EmitterShape shape) {
    switch(shape) {
    case EmitterShape::Sphere: return "sphere";
    case EmitterShape::Box: return "box";
    case EmitterShape::Ring: return "ring";
    case EmitterShape::Disc: return "disc";
    default: return "point";
    }
}
const char* blendName(ParticleBlend blend) {return blend==ParticleBlend::Alpha?"alpha":"additive";}
const char* spriteName(ParticleSprite sprite) {
    switch(sprite) {
    case ParticleSprite::Circle: return "circle";
    case ParticleSprite::Ring: return "ring";
    case ParticleSprite::Square: return "square";
    case ParticleSprite::Spark: return "spark";
    case ParticleSprite::Smoke: return "smoke";
    default: return "soft";
    }
}
}
