#include "fx_runtime.hpp"
#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
namespace swan {
namespace {
constexpr float pi=3.14159265f;
// PCG32: small, fast, and identical on every platform.
struct Rng {
    uint64_t state=0x853c49e6748fea9bULL;
    explicit Rng(uint64_t seed=0) {state=seed*6364136223846793005ULL+1442695040888963407ULL;next();}
    uint32_t next() {
        uint64_t old=state;state=old*6364136223846793005ULL+1442695040888963407ULL;
        auto shifted=uint32_t(((old>>18u)^old)>>27u);auto rotation=uint32_t(old>>59u);
        return (shifted>>rotation)|(shifted<<((32-rotation)&31));
    }
    float uniform() {return float(next()>>8)*(1.0f/16777216.0f);}
    float range(Range r) {return r.min+(r.max-r.min)*uniform();}
    glm::vec3 unit() {
        float z=uniform()*2-1,a=uniform()*2*pi,r=std::sqrt(std::max(0.0f,1-z*z));
        return {r*std::cos(a),z,r*std::sin(a)};
    }
};
uint64_t mix(uint64_t a,uint64_t b) {
    uint64_t x=a^(b+0x9e3779b97f4a7c15ULL+(a<<6)+(a>>2));
    x^=x>>31;x*=0xbf58476d1ce4e5b9ULL;x^=x>>29;
    return x;
}
// FNV-1a: entity keys seed attached effects identically on every platform.
uint32_t stableHash(const std::string& text) {
    uint32_t hash=2166136261u;
    for(unsigned char c:text) {hash^=c;hash*=16777619u;}
    return hash;
}
glm::vec3 rotateYaw(glm::vec3 v,float yaw) {
    float c=std::cos(yaw),s=std::sin(yaw);
    // Same convention as composeTransform().
    return {c*v.x+s*v.z,v.y,-s*v.x+c*v.z};
}
glm::vec3 coneDirection(Rng& rng,glm::vec3 axis,float spreadDegrees) {
    float length=glm::length(axis);
    axis=length>1e-6f?axis/length:glm::vec3(0,1,0);
    if(spreadDegrees<=0) return axis;
    float cosine=std::cos(std::min(spreadDegrees,180.0f)*pi/180);
    float z=1-(1-cosine)*rng.uniform(),a=rng.uniform()*2*pi,r=std::sqrt(std::max(0.0f,1-z*z));
    glm::vec3 helper=std::abs(axis.y)<0.99f?glm::vec3(0,1,0):glm::vec3(1,0,0);
    glm::vec3 u=glm::normalize(glm::cross(helper,axis)),v=glm::cross(axis,u);
    return u*(r*std::cos(a))+v*(r*std::sin(a))+axis*z;
}
glm::vec3 axisOf(const EmitterDef& e) {
    float length=glm::length(e.direction);
    return length>1e-6f?e.direction/length:glm::vec3(0,1,0);
}
// Maps the XZ plane onto the plane perpendicular to `axis` (identity for +Y).
glm::vec3 toAxisPlane(glm::vec3 p,glm::vec3 axis) {
    if(axis.y>0.9999f) return p;
    if(axis.y<-0.9999f) return {p.x,-p.y,-p.z};
    glm::vec3 u=glm::normalize(glm::cross(glm::vec3(0,0,1),axis));
    if(glm::length(glm::cross(glm::vec3(0,0,1),axis))<1e-4f) u=glm::vec3(1,0,0);
    glm::vec3 v=glm::cross(u,axis);
    return u*p.x+axis*p.y+v*p.z;
}
// Rodrigues rotation of `v` around unit `axis`.
glm::vec3 rotateAxis(glm::vec3 v,glm::vec3 axis,float angle) {
    float c=std::cos(angle),s=std::sin(angle);
    return v*c+glm::cross(axis,v)*s+axis*glm::dot(axis,v)*(1-c);
}
glm::vec3 shapePoint(Rng& rng,const EmitterDef& e) {
    switch(e.shape) {
    case EmitterShape::Sphere: {
        auto direction=rng.unit();
        return direction*(e.surface?e.radius:e.radius*std::cbrt(rng.uniform()));
    }
    case EmitterShape::Box: {
        glm::vec3 p{(rng.uniform()-0.5f)*e.size3.x,(rng.uniform()-0.5f)*e.size3.y,(rng.uniform()-0.5f)*e.size3.z};
        if(e.surface) {
            // Pick a face pair weighted by area, then a side.
            float yz=e.size3.y*e.size3.z,xz=e.size3.x*e.size3.z,xy=e.size3.x*e.size3.y,total=yz+xz+xy;
            float pick=rng.uniform()*(total>0?total:1);
            int axis=pick<yz?0:pick<yz+xz?1:2;
            p[axis]=(rng.uniform()<0.5f?-0.5f:0.5f)*e.size3[axis];
        }
        return p;
    }
    case EmitterShape::Ring: {
        float a=rng.uniform()*2*pi;
        return toAxisPlane({std::cos(a)*e.radius,0,std::sin(a)*e.radius},axisOf(e));
    }
    case EmitterShape::Disc: {
        float a=rng.uniform()*2*pi,r=e.surface?e.radius:e.radius*std::sqrt(rng.uniform());
        return toAxisPlane({std::cos(a)*r,0,std::sin(a)*r},axisOf(e));
    }
    default: return {};
    }
}
glm::vec3 turbulence(glm::vec3 p,float t,float frequency) {
    glm::vec3 q=p*frequency;
    return glm::vec3(std::sin(q.y+t*0.7f+1.3f)+std::sin(q.z*1.7f+t*1.1f),
                     std::sin(q.z+t*0.9f+2.1f)+std::sin(q.x*1.3f+t*0.6f),
                     std::sin(q.x+t*0.8f+4.2f)+std::sin(q.y*1.9f+t*1.3f))*0.5f;
}
struct Particle {
    glm::vec3 position{},velocity{};   // Emitter space when the emitter is local, else world.
    float age=0,lifetime=1,size=1,rotation=0,spin=0;
    uint32_t seed=0;
};
struct EmitterState {
    std::vector<Particle> particles;
    Rng rng;
    double accumulator=0;
    uint64_t cycle=0,spawned=0,dropped=0;
    size_t burst=0;
};
struct Origin { glm::vec3 position{}; float yaw=0; };
struct Instance {
    SharedEffect effect;
    std::string effectId,entity;
    glm::vec3 offset{};                // From the followed entity.
    float extraYaw=0;
    bool attached=false,emitting=true;
    Origin origin,previous;
    double time=0;
    std::vector<EmitterState> emitters;
    bool finished() const {
        for(const auto& state:emitters) if(!state.particles.empty()) return false;
        if(!emitting) return true;
        for(const auto& e:effect->emitters) if(e.loop || time<=double(e.delay+e.duration)) return false;
        return true;
    }
};
Origin lerp(const Origin& a,const Origin& b,float f) {
    return {glm::mix(a.position,b.position,f),a.yaw+std::remainder(b.yaw-a.yaw,2*pi)*f};
}
// Applies tracks; emit multipliers go to `emit` when given.
void applyTracks(Scene& scene,const Timeline& timeline,float time,std::map<std::string,float>* emit) {
    for(const auto& track:timeline.tracks) {
        if(track.kind==TrackTarget::Environment) {
            auto environment=scene.environment();
            if(track.property==TrackProperty::Exposure) environment.exposure=std::clamp(track.scalar.sample(time),1e-3f,64.0f);
            else if(track.property==TrackProperty::Bloom) environment.bloom=std::max(0.0f,track.scalar.sample(time));
            else environment.background=glm::max(track.vector.sample(time),glm::vec3(0));
            scene.setEnvironment(environment);
            continue;
        }
        if(track.kind==TrackTarget::Material) {
            if(!scene.assets().contains(track.target)) continue;
            auto material=scene.assets().get(track.target);
            if(track.property==TrackProperty::Color) material.color=glm::max(track.vector.sample(time),glm::vec3(0));
            else material.emission=std::max(0.0f,track.scalar.sample(time));
            scene.assets().set(track.target,material);
            continue;
        }
        auto* entity=scene.get(scene.find(track.target));
        if(!entity) continue;
        switch(track.property) {
        case TrackProperty::Position: entity->transform.position=track.vector.sample(time);break;
        case TrackProperty::Scale: entity->transform.scale=glm::max(track.vector.sample(time),glm::vec3(1e-4f));break;
        case TrackProperty::Yaw: entity->transform.yaw=track.scalar.sample(time);break;
        case TrackProperty::Emit: if(emit) (*emit)[track.target]=std::max(0.0f,track.scalar.sample(time));break;
        default: break;
        }
    }
}
}
struct FxRuntime::Impl {
    uint32_t seed=0;
    uint64_t serial=0;
    double elapsed=0;
    float timelineTime=0;
    std::vector<Instance> instances;
    std::map<std::string,float> emitScale;  // Timeline `emit` multipliers by entity key.
    size_t live=0;
    Origin entityOrigin(const Scene& scene,EntityId id,glm::vec3 offset,float yaw) const {
        auto world=scene.worldTransform(id);
        return {world.position+rotateYaw(offset,world.yaw),world.yaw+yaw};
    }
    Instance& start(const Scene& scene,const std::string& effect,Origin origin,uint32_t instanceSeed) {
        Instance instance;
        instance.effect=scene.effects().get(effect);instance.effectId=effect;
        instance.origin=instance.previous=origin;
        uint64_t base=mix(mix(seed,serial++),instanceSeed);
        for(size_t i=0;i<instance.effect->emitters.size();++i) {
            EmitterState state;state.rng=Rng(mix(mix(base,i),instance.effect->emitters[i].seed));
            instance.emitters.push_back(std::move(state));
        }
        instances.push_back(std::move(instance));
        return instances.back();
    }
    void syncAttached(const Scene& scene) {
        std::map<std::string,size_t> current;
        for(size_t i=0;i<instances.size();++i) if(instances[i].attached) current[instances[i].entity]=i;
        for(auto id:scene.entities()) {
            const auto& entity=*scene.get(id);
            if(entity.effectId.empty()) continue;
            auto found=current.find(entity.key);
            if(found!=current.end()) {
                if(instances[found->second].effectId==entity.effectId) {current.erase(found);continue;}
                // Changed effect: the old instance lets its particles finish.
                instances[found->second].attached=false;instances[found->second].emitting=false;current.erase(found);
            }
            auto& instance=start(scene,entity.effectId,entityOrigin(scene,id,{},0),stableHash(entity.key));
            instance.attached=true;instance.entity=entity.key;
        }
        // Entities that disappeared or lost their effect.
        for(const auto& [key,index]:current) {(void)key;instances[index].attached=false;instances[index].emitting=false;}
    }
    void fireEvents(const Scene& scene,const Timeline& timeline,double from,double to,bool first) {
        if(timeline.events.empty()) return;
        float length=timeline.length();
        auto fireRange=[&](float low,float high,bool inclusiveLow) {
            for(const auto& event:timeline.events)
                if((inclusiveLow?event.time>=low:event.time>low) && event.time<=high) fireEvent(scene,event);
        };
        if(first) {fireRange(0,0,true);return;}
        if(!timeline.loop || length<=0) {
            fireRange(float(std::min(from,double(length))),float(std::min(to,double(length))),false);
            return;
        }
        auto fromCycle=uint64_t(std::floor(from/length)),toCycle=uint64_t(std::floor(to/length));
        float a=float(from-double(fromCycle)*length),b=float(to-double(toCycle)*length);
        if(fromCycle==toCycle) {fireRange(a,b,false);return;}
        fireRange(a,length,false);
        // Whole cycles skipped by a very long step fire at most a few times.
        for(uint64_t c=fromCycle+1;c<toCycle && c<fromCycle+4;++c) fireRange(0,length,true);
        if(b>=0) fireRange(0,b,true);
    }
    void fireEvent(const Scene& scene,const TimelineEvent& event) {
        if(!scene.effects().contains(event.effect)) return;
        Origin origin{event.position,event.yaw};
        EntityId id;
        if(!event.entity.empty()) {
            id=scene.find(event.entity);
            if(!scene.get(id)) return;
            origin=entityOrigin(scene,id,event.position,event.yaw);
        }
        auto& instance=start(scene,event.effect,origin,event.seed);
        if(scene.get(id)) {instance.entity=event.entity;instance.offset=event.position;instance.extraYaw=event.yaw;}
    }
    void spawn(Instance& instance,size_t index,const Origin& origin,float preAge) {
        const auto& e=instance.effect->emitters[index];
        auto& state=instance.emitters[index];
        if(state.particles.size()>=e.maxParticles || live>=maxLiveParticles) {++state.dropped;return;}
        Particle p;
        auto& rng=state.rng;
        auto local=shapePoint(rng,e);
        glm::vec3 axis=e.radial?local:e.direction;
        if(e.radial && glm::length(axis)<1e-6f) axis=rng.unit();
        auto velocity=coneDirection(rng,axis,e.spread)*rng.range(e.speed)+e.velocity;
        p.position=e.offset+local;p.velocity=velocity;
        if(!e.local) {p.position=origin.position+rotateYaw(p.position,origin.yaw);p.velocity=rotateYaw(p.velocity,origin.yaw);}
        p.lifetime=rng.range(e.lifetime);p.size=rng.range(e.size);
        p.rotation=rng.range(e.rotation)*pi/180;p.spin=rng.range(e.spin)*pi/180;
        p.seed=rng.next();
        state.particles.push_back(p);++state.spawned;++live;
        if(preAge>0) integrate(instance,index,state.particles.back(),preAge,origin);
    }
    void integrate(const Instance& instance,size_t index,Particle& p,float dt,const Origin& origin) const {
        const auto& e=instance.effect->emitters[index];
        glm::vec3 acceleration=e.gravity;
        if(e.noise>0) acceleration+=turbulence(p.position,float(instance.time),e.noiseFrequency)*e.noise;
        p.velocity+=acceleration*dt;
        if(e.drag>0) p.velocity*=std::exp(-e.drag*dt);
        if(e.orbit!=0) {
            glm::vec3 center=e.local?e.offset:origin.position+rotateYaw(e.offset,origin.yaw);
            glm::vec3 axis=e.local?axisOf(e):rotateYaw(axisOf(e),origin.yaw);
            float angle=e.orbit*dt; // Right-handed about the axis: the same sense as entity yaw about +Y.
            p.position=center+rotateAxis(p.position-center,axis,angle);
            p.velocity=rotateAxis(p.velocity,axis,angle);
        }
        p.position+=p.velocity*dt;
        p.rotation+=p.spin*dt;
        p.age+=dt;
    }
    void emit(Instance& instance,size_t index,double t0,double t1,float multiplier) {
        const auto& e=instance.effect->emitters[index];
        auto& state=instance.emitters[index];
        double dt=t1-t0;
        if(dt<=0) return;
        double begin=t0-e.delay,end=t1-e.delay; // Emitter-window time.
        auto originAt=[&](double windowTime){
            float f=float(std::clamp((windowTime+e.delay-t0)/dt,0.0,1.0));
            return std::pair{lerp(instance.previous,instance.origin,f),float((1-f)*dt)};
        };
        // Bursts, in order, possibly across loop cycles.
        while(!e.bursts.empty() && multiplier>0) {
            if(state.burst<e.bursts.size()) {
                double at=double(state.cycle)*e.duration+e.bursts[state.burst].time;
                if(at>end) break;
                if(at>=begin || (begin<=0 && at==0)) {
                    auto [origin,age]=originAt(at);
                    auto count=uint32_t(std::lround(double(e.bursts[state.burst].count)*multiplier));
                    for(uint32_t k=0;k<count;++k) spawn(instance,index,origin,age);
                }
                ++state.burst;
            } else if(e.loop && double(state.cycle+1)*e.duration<=end) {++state.cycle;state.burst=0;}
            else break;
        }
        if(e.rate<=0 || multiplier<=0) return;
        double activeBegin=std::max(begin,0.0),activeEnd=e.loop?end:std::min(end,double(e.duration));
        if(activeEnd<=activeBegin) return;
        double middle=(activeBegin+activeEnd)/2;
        float phase=float(std::fmod(middle,double(e.duration))/e.duration);
        state.accumulator+=double(e.rate)*e.rateOverTime.sample(phase,1.0f)*multiplier*(activeEnd-activeBegin);
        auto count=uint64_t(std::floor(state.accumulator));
        state.accumulator-=double(count);
        count=std::min<uint64_t>(count,maxParticlesPerEmitter);
        for(uint64_t k=0;k<count;++k) {
            double at=activeBegin+(activeEnd-activeBegin)*double(k+1)/double(count);
            auto [origin,age]=originAt(at);
            spawn(instance,index,origin,age);
        }
    }
    void step(Instance& instance,const Scene& scene,float dt) {
        instance.previous=instance.origin;
        if(!instance.entity.empty()) {
            auto id=scene.find(instance.entity);
            if(scene.get(id)) instance.origin=entityOrigin(scene,id,instance.offset,instance.extraYaw);
        }
        double t0=instance.time,t1=t0+dt;
        instance.time=t1;
        for(size_t i=0;i<instance.emitters.size();++i) {
            auto& state=instance.emitters[i];
            auto before=state.particles.size();
            for(auto& p:state.particles) integrate(instance,i,p,dt,instance.origin);
            std::erase_if(state.particles,[](const Particle& p){return p.age>=p.lifetime;});
            live-=before-state.particles.size();
            if(instance.emitting) {
                float multiplier=1;
                if(instance.attached) if(auto found=emitScale.find(instance.entity);found!=emitScale.end()) multiplier=found->second;
                emit(instance,i,t0,t1,multiplier);
            }
        }
    }
};
FxRuntime::FxRuntime(Scene& scene,uint32_t seed):impl(std::make_unique<Impl>()) {
    impl->seed=seed;
    applyTracks(scene,scene.timeline(),0,&impl->emitScale);
    impl->fireEvents(scene,scene.timeline(),0,0,true);
    impl->syncAttached(scene);
}
FxRuntime::~FxRuntime()=default;
FxRuntime::FxRuntime(FxRuntime&&) noexcept=default;
FxRuntime& FxRuntime::operator=(FxRuntime&&) noexcept=default;
void FxRuntime::update(Scene& scene,float dt) {
    if(!std::isfinite(dt) || dt<0) throw std::invalid_argument("Effect update step must be finite and nonnegative");
    auto& r=*impl;
    double before=r.elapsed;
    r.elapsed+=dt;
    const auto& timeline=scene.timeline();
    r.timelineTime=timeline.localTime(r.elapsed);
    applyTracks(scene,timeline,r.timelineTime,&r.emitScale);
    r.fireEvents(scene,timeline,before,r.elapsed,false);
    r.syncAttached(scene);
    // Instances started this tick also simulate it (their previous origin equals the current one).
    for(size_t i=0;i<r.instances.size();++i) r.step(r.instances[i],scene,dt);
    std::erase_if(r.instances,[&](const Instance& instance){
        if(instance.attached) return false;
        return instance.finished();
    });
}
void FxRuntime::play(const Scene& scene,const std::string& effect,glm::vec3 position,float yaw,const std::string& entity,uint32_t seed) {
    if(!scene.effects().contains(effect)) throw std::invalid_argument("Unknown effect asset: "+effect);
    if(!std::isfinite(position.x) || !std::isfinite(position.y) || !std::isfinite(position.z) || !std::isfinite(yaw)) throw std::invalid_argument("Effect position must be finite");
    TimelineEvent event{0,effect,entity,position,yaw,seed};
    if(!entity.empty() && !scene.get(scene.find(entity))) throw std::invalid_argument("Unknown entity: "+entity);
    impl->fireEvent(scene,event);
}
double FxRuntime::elapsed() const {return impl->elapsed;}
float FxRuntime::timelineTime() const {return impl->timelineTime;}
size_t FxRuntime::particleCount() const {return impl->live;}
FxStats FxRuntime::stats() const {
    FxStats result;
    result.instances=impl->instances.size();
    bool any=false;
    for(const auto& instance:impl->instances) {
        for(size_t i=0;i<instance.emitters.size();++i) {
            const auto& state=instance.emitters[i];const auto& e=instance.effect->emitters[i];
            FxEmitterStats s;
            s.effect=instance.effectId;s.emitter=e.name.empty()?std::to_string(i):e.name;s.entity=instance.entity;
            s.particles=state.particles.size();s.spawned=state.spawned;s.dropped=state.dropped;
            bool first=true;
            for(const auto& p:state.particles) {
                auto world=e.local?instance.origin.position+rotateYaw(p.position,instance.origin.yaw):p.position;
                if(first) {s.minimum=s.maximum=world;first=false;}
                else {s.minimum=glm::min(s.minimum,world);s.maximum=glm::max(s.maximum,world);}
            }
            if(s.particles) {
                if(!any) {result.minimum=s.minimum;result.maximum=s.maximum;any=true;}
                else {result.minimum=glm::min(result.minimum,s.minimum);result.maximum=glm::max(result.maximum,s.maximum);}
            }
            result.particles+=s.particles;result.spawned+=s.spawned;result.dropped+=s.dropped;
            result.emitters.push_back(std::move(s));
        }
    }
    return result;
}
void FxRuntime::appendTo(RenderFrame& frame,const Scene& scene) const {
    std::map<std::pair<int,const TextureData*>,size_t> batches;
    for(const auto& instance:impl->instances) {
        for(size_t i=0;i<instance.emitters.size();++i) {
            const auto& state=instance.emitters[i];const auto& e=instance.effect->emitters[i];
            if(state.particles.empty()) continue;
            auto texture=!e.texture.empty() && scene.textures().contains(e.texture)?scene.textures().get(e.texture).data:whiteTexture();
            auto key=std::pair{int(e.blend),texture.get()};
            auto found=batches.find(key);
            if(found==batches.end()) {
                found=batches.emplace(key,frame.particles.size()).first;
                frame.particles.push_back({e.blend,texture,{}});
            }
            auto& out=frame.particles[found->second].particles;
            for(const auto& p:state.particles) {
                float life=std::clamp(p.age/p.lifetime,0.0f,1.0f);
                float size=p.size*e.sizeOverLife.sample(life,1.0f);
                auto color=e.color*e.colorOverLife.sample(life,glm::vec4(1));
                if(size<=0 || color.a<=0) continue;
                ParticleVertex v;
                v.position=e.local?instance.origin.position+rotateYaw(p.position,instance.origin.yaw):p.position;
                v.velocity=e.local?rotateYaw(p.velocity,instance.origin.yaw):p.velocity;
                v.size=size;v.color=glm::vec4(glm::vec3(color)*e.intensity,std::min(color.a,1.0f));
                v.rotation=p.rotation;v.stretch=e.stretch;v.sprite=uint32_t(e.sprite);v.seed=p.seed;
                out.push_back(v);
            }
        }
    }
    std::erase_if(frame.particles,[](const ParticleBatch& batch){return batch.particles.empty();});
}
void applyTimeline(Scene& scene,float time) {applyTracks(scene,scene.timeline(),time,nullptr);}
}
