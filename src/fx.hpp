#pragma once
#include <glm/glm.hpp>
#include <cstdint>
#include <string>
#include <vector>
namespace swan {
// Interpolation from a key toward the next one.
enum class Ease : uint8_t { Linear, Step, Smooth, In, Out };
float applyEase(Ease ease,float t);
template<class T> struct Keyframe { float time=0; T value{}; Ease ease=Ease::Linear; };
// Keys are sorted by nondecreasing time; samples clamp to the first/last key. An empty curve
// samples `fallback`.
template<class T> struct Curve {
    std::vector<Keyframe<T>> keys;
    bool empty() const { return keys.empty(); }
    T sample(float time,T fallback=T(1)) const {
        if(keys.empty()) return fallback;
        if(time<=keys.front().time) return keys.front().value;
        if(time>=keys.back().time) return keys.back().value;
        size_t next=1;
        while(next<keys.size() && keys[next].time<time) ++next;
        const auto& a=keys[next-1];const auto& b=keys[next];
        float span=b.time-a.time;
        float t=span>0?(time-a.time)/span:1;
        return a.value+(b.value-a.value)*applyEase(a.ease,t);
    }
};
using FloatCurve=Curve<float>;
using Vec3Curve=Curve<glm::vec3>;
using ColorCurve=Curve<glm::vec4>;
// Inclusive uniform random range.
struct Range { float min=0,max=0; };
enum class EmitterShape : uint8_t { Point, Sphere, Box, Ring, Disc };
enum class ParticleBlend : uint8_t { Additive, Alpha };
// Procedural sprite masks drawn by the particle shader (multiplied with an optional texture).
enum class ParticleSprite : uint8_t { Soft, Circle, Ring, Square, Spark, Smoke };
struct Burst { float time=0; uint32_t count=0; };
// One particle emitter. Times are seconds; angles are degrees; over-life curves take the
// normalized particle age 0..1. See docs/FX.md for the JSON form.
struct EmitterDef {
    std::string name;
    glm::vec3 offset{};                      // From the effect origin, rotated by its yaw.
    float delay=0,duration=1;                // Emission window, relative to the instance start.
    bool loop=false;                         // Repeat the window (rate curve and bursts) forever.
    float rate=0;                            // Particles per second.
    FloatCurve rateOverTime;                 // Multiplier over the normalized emission window.
    std::vector<Burst> bursts;               // Times within the window.
    Range lifetime{1,1};
    EmitterShape shape=EmitterShape::Point;
    float radius=0.5f;                       // Sphere, ring, and disc.
    glm::vec3 size3{1};                      // Box extents.
    bool surface=false;                      // Spawn on the shape's surface/edge only.
    glm::vec3 direction{0,1,0};               // Also the axis of ring/disc shapes and of orbit.
    float spread=0;                          // Cone half-angle around the direction (0..180).
    bool radial=false;                       // Aim outward from the emitter center instead.
    Range speed{1,1};
    glm::vec3 velocity{};                    // Added to every new particle.
    glm::vec3 gravity{};                     // Acceleration.
    float drag=0;                            // Exponential velocity damping per second.
    float noise=0,noiseFrequency=1;          // Smooth turbulent acceleration.
    float orbit=0;                           // Radians per second around the emitter axis (direction).
    Range size{0.2f,0.2f};
    FloatCurve sizeOverLife;
    glm::vec4 color{1};                      // Linear RGB and alpha.
    ColorCurve colorOverLife;                // Multiplies `color`.
    float intensity=1;                       // HDR brightness multiplier for RGB.
    Range rotation{0,0},spin{0,0};
    float stretch=0;                         // Elongate along screen-space velocity.
    ParticleBlend blend=ParticleBlend::Additive;
    ParticleSprite sprite=ParticleSprite::Soft;
    std::string texture;                     // Optional texture asset ID.
    bool local=false;                        // JSON "space": "local": particles follow the moving emitter.
    uint32_t maxParticles=1000;
    uint32_t seed=0;
};
struct EffectDef { std::vector<EmitterDef> emitters; };
constexpr size_t maxEmittersPerEffect=16,maxEffects=256,maxParticlesPerEmitter=20000,maxCurveKeys=256;
// Throws std::invalid_argument naming the emitter and field.
void validateEffect(const EffectDef& effect);
// Seconds until every emitter stops and its last particle dies; infinity if any emitter loops.
float effectDuration(const EffectDef& effect);
const char* easeName(Ease ease);
const char* shapeName(EmitterShape shape);
const char* blendName(ParticleBlend blend);
const char* spriteName(ParticleSprite sprite);
}
