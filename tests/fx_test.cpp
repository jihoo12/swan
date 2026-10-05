#include "editor_document.hpp"
#include "fx_io.hpp"
#include "fx_runtime.hpp"
#include "image.hpp"
#include "scene_io.hpp"
#include <nlohmann/json.hpp>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
using Json=nlohmann::json;
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
// Requires a rejection whose message mentions `fragment`.
template<class F> void rejects(F operation,const std::string& fragment) {
    try {operation();}
    catch(const std::exception& e) {
        require(std::string(e.what()).find(fragment)!=std::string::npos,"Rejected for the wrong reason: "+std::string(e.what())+" (expected '"+fragment+"')");
        return;
    }
    throw std::runtime_error("Accepted invalid input (expected '"+fragment+"')");
}
bool near(float a,float b,float tolerance=1e-4f) {return std::abs(a-b)<=tolerance;}
swan::EffectDef effect(const char* json) {return swan::parseEffect(json);}
void step(swan::FxRuntime& fx,swan::Scene& scene,double seconds) {
    for(int i=0,n=int(std::lround(seconds*120));i<n;++i) fx.update(scene,1.0f/120);
}
void curves() {
    swan::FloatCurve c;c.keys={{0,0,swan::Ease::Linear},{1,10,swan::Ease::Step},{2,20,swan::Ease::Smooth},{3,0}};
    require(near(c.sample(-1),0) && near(c.sample(0.5f),5) && near(c.sample(1.5f),10) && near(c.sample(2.5f),10) && near(c.sample(9),0),"Curve sampling/ease");
    require(near(swan::applyEase(swan::Ease::In,0.5f),0.25f) && near(swan::applyEase(swan::Ease::Out,0.5f),0.75f),"Ease functions");
    require(near(swan::FloatCurve{}.sample(0.3f,7),7),"Empty curve fallback");
}
void effectSchema() {
    auto e=effect(R"({"emitters":[{"name":"spark","rate":50,"lifetime":[0.5,1],"size_over_life":[[0,1],[1,0,"in"]],
        "color_over_life":[{"t":0,"value":[1,0.5,0]},{"t":1,"value":[1,0,0,0],"ease":"ease_out"}],"bursts":[[0,20],{"time":0.5,"count":5}],
        "shape":"sphere","radius":2,"blend":"alpha","sprite":"smoke","speed":3}]})");
    const auto& m=e.emitters.at(0);
    require(m.name=="spark" && m.rate==50 && m.lifetime.min==0.5f && m.lifetime.max==1 && m.speed.min==3 && m.speed.max==3,"Emitter fields");
    require(m.sizeOverLife.keys.size()==2 && m.sizeOverLife.keys[1].ease==swan::Ease::In,"Size curve");
    require(m.colorOverLife.keys[0].value==glm::vec4(1,0.5f,0,1) && m.colorOverLife.keys[1].ease==swan::Ease::Out,"Color curve with default alpha");
    require(m.bursts.size()==2 && m.bursts[1].count==5 && m.shape==swan::EmitterShape::Sphere && m.blend==swan::ParticleBlend::Alpha && m.sprite==swan::ParticleSprite::Smoke,"Bursts/enums");
    // Serialization omits defaults and round-trips exactly.
    auto text=swan::serializeEffect(e);
    require(swan::serializeEffect(swan::parseEffect(text))==text,"Effect JSON round trip");
    require(Json::parse(text)["emitters"][0].contains("rate") && !Json::parse(text)["emitters"][0].contains("drag"),"Defaults should be omitted");
    rejects([]{effect(R"({"emitters":[{"sise":1}]})");},"unknown field 'sise'");
    rejects([]{effect(R"({"emitters":[{"lifetime":[2,1]}]})");},"lifetime minimum exceeds");
    rejects([]{effect(R"({"emitters":[{"size_over_life":[[0,1],[1.5,0]]}]})");},"within 0..1");
    rejects([]{effect(R"({"emitters":[{"size_over_life":[[0.5,1],[0.2,0]]}]})");},"must not decrease");
    rejects([]{effect(R"({"emitters":[{"blend":"screen"}]})");},"use additive, alpha");
    rejects([]{effect(R"({"emitters":[{"bursts":[[3,1]]}]})");},"within 0..duration");
    rejects([]{effect(R"({"emitters":[]})");},"at least one emitter");
    rejects([]{effect(R"({"emitters":[{"spread":200}]})");},"spread");
    rejects([]{effect(R"({"emitters":[{"rate":-1}]})");},"rate");
    rejects([]{effect(R"({"emitters":[{},{"color":[1,1,1,2]}]})");},"emitter 1");
    rejects([]{effect(R"({"emitters":[{"max_particles":0}]})");},"max_particles");
    rejects([]{swan::parseEffect("{");},"Invalid effect JSON");
}
swan::Scene stage() {
    swan::Scene scene;
    swan::Entity floor;floor.key="floor";floor.transform={{0,-0.5f,0},{10,1,10},0};scene.create(floor);
    return scene;
}
void bursts() {
    auto scene=stage();
    scene.effects().set("pop",effect(R"({"emitters":[{"bursts":[[0,50],[0.5,10]],"duration":1,"lifetime":0.3,"speed":2,"spread":180}]})"));
    swan::FxRuntime fx(scene);
    fx.play(scene,"pop",{0,1,0});
    fx.update(scene,1.0f/120);
    require(fx.particleCount()==50,"Burst at time 0 should spawn exactly its count, got "+std::to_string(fx.particleCount()));
    step(fx,scene,0.35);
    require(fx.particleCount()==0,"Particles must die after their lifetime");
    step(fx,scene,0.2);
    require(fx.particleCount()==10,"Second burst count");
    step(fx,scene,0.6);
    require(fx.particleCount()==0 && fx.stats().instances==0,"Finished one-shot instances are removed");
    rejects([&]{fx.play(scene,"missing",{});},"Unknown effect");
}
void rateCapAndDeterminism() {
    auto scene=stage();
    scene.effects().set("stream",effect(R"({"emitters":[{"rate":100,"duration":1,"lifetime":5,"speed":[1,3],"spread":30,"noise":1,"drag":0.2}]})"));
    scene.effects().set("capped",effect(R"({"emitters":[{"rate":1000,"loop":true,"lifetime":10,"max_particles":64}]})"));
    swan::FxRuntime fx(scene);
    fx.play(scene,"stream",{});
    step(fx,scene,1.5);
    auto stats=fx.stats();
    require(stats.spawned>=99 && stats.spawned<=101,"Rate 100/s for 1 s spawned "+std::to_string(stats.spawned));
    require(stats.maximum.y>stats.minimum.y,"Particles moved");
    swan::FxRuntime capped(scene);
    capped.play(scene,"capped",{});
    step(capped,scene,0.5);
    require(capped.particleCount()==64 && capped.stats().dropped>0,"max_particles cap and dropped count");
    auto run=[&](uint32_t seed){
        auto copy=scene;swan::FxRuntime f(copy,seed);f.play(copy,"stream",{1,2,3},0.5f);step(f,copy,0.7);
        swan::RenderFrame frame;f.appendTo(frame,copy);return frame;
    };
    auto a=run(1),b=run(1),c=run(2);
    require(a.particles.size()==1 && a.particles[0].particles.size()==b.particles[0].particles.size(),"Deterministic particle count");
    bool same=true,differs=false;
    for(size_t i=0;i<a.particles[0].particles.size();++i) {
        same&=a.particles[0].particles[i].position==b.particles[0].particles[i].position;
        if(i<c.particles[0].particles.size()) differs|=a.particles[0].particles[i].position!=c.particles[0].particles[i].position;
    }
    require(same,"Same seed must reproduce particles exactly");
    require(differs,"A different seed must change particles");
}
void attachedAndLocal() {
    auto scene=stage();
    scene.effects().set("trail",effect(R"({"emitters":[{"rate":60,"loop":true,"lifetime":2,"speed":0}]})"));
    scene.effects().set("halo",effect(R"({"emitters":[{"rate":60,"loop":true,"lifetime":2,"speed":0,"space":"local","offset":[1,0,0]}]})"));
    swan::Entity mover;mover.key="mover";mover.effectId="trail";scene.create(mover);
    swan::Entity ring;ring.key="ring";ring.effectId="halo";ring.transform.position={0,0,5};scene.create(ring);
    swan::FxRuntime fx(scene);
    for(int i=0;i<120;++i) {scene.get(scene.find("mover"))->transform.position.x=float(i)*0.05f;scene.get(scene.find("ring"))->transform.yaw=float(i)*0.01f;fx.update(scene,1.0f/120);}
    auto stats=fx.stats();
    swan::FxEmitterStats trail,halo;
    for(const auto& e:stats.emitters) (e.effect=="trail"?trail:halo)=e;
    require(trail.entity=="mover" && trail.maximum.x-trail.minimum.x>4,"World-space particles stay behind a moving emitter (trail)");
    require(halo.particles>0 && near(halo.minimum.z,halo.maximum.z,0.05f) && near(halo.minimum.x,halo.maximum.x,0.05f),"Local particles move with the emitter");
    // Removing the effect stops emission; live particles finish.
    scene.get(scene.find("mover"))->effectId.clear();
    fx.update(scene,1.0f/120);
    auto before=fx.stats().spawned;
    step(fx,scene,0.5);
    require(fx.stats().spawned<=before+60,"Detached effect kept emitting");
    step(fx,scene,2.1);
    for(const auto& e:fx.stats().emitters) require(e.effect!="trail","Detached instance should finish");
}
void timeline() {
    auto scene=stage();
    scene.effects().set("pop",effect(R"({"emitters":[{"bursts":[[0,5]],"duration":0.1,"lifetime":10}]})"));
    swan::Entity orb;orb.key="orb";orb.materialId="glow";scene.assets().set("glow",{{1,1,1},0});scene.create(orb);
    scene.timeline()=swan::parseTimeline(R"({"duration":2,"loop":true,
        "camera":{"position":[[0,[0,2,8]],[2,[4,2,8]]],"target":[0,1,0],"fov":40},
        "tracks":[{"entity":"orb","property":"position","keys":[[0,[0,0,0],"smooth"],[1,[0,4,0]],[2,[0,0,0]]]},
                  {"material":"glow","property":"emission","keys":[[0,0],[1,5]]}],
        "events":[{"t":0.5,"effect":"pop","entity":"orb","offset":[1,0,0]},{"time":0,"effect":"pop","position":[3,0,0]}]})");
    scene.validateReferences();
    swan::FxRuntime fx(scene);
    fx.update(scene,1.0f/120);
    require(fx.particleCount()==5,"Time-0 event fires once at the start");
    step(fx,scene,0.5-1.0/120);
    require(near(scene.get(scene.find("orb"))->transform.position.y,2,0.01f),"Smooth position track at t=0.5");
    require(fx.particleCount()==10,"Event at 0.5 s");
    require(near(scene.assets().get("glow").emission,2.5f,0.05f),"Material emission track");
    step(fx,scene,1.6);
    require(fx.particleCount()==15,"Looping timeline refires the time-0 event at the wrap, got "+std::to_string(fx.particleCount()));
    step(fx,scene,0.5);
    require(fx.particleCount()==20,"Looping timeline refires later events each cycle, got "+std::to_string(fx.particleCount()));
    auto camera=scene.timeline().cameraAt(1);
    require(camera && near(camera->position.x,2) && near(camera->fov,40),"Camera track sampling");
    auto json=swan::serializeTimeline(scene.timeline());
    require(swan::serializeTimeline(swan::parseTimeline(json))==json,"Timeline JSON round trip");
    rejects([]{swan::parseTimeline(R"({"tracks":[{"entity":"a","property":"color","keys":[[0,[1,1,1]]]}]})");},"does not apply");
    rejects([]{swan::parseTimeline(R"({"tracks":[{"entity":"a","property":"spin","keys":[[0,1]]}]})");},"unknown property 'spin'");
    rejects([]{swan::parseTimeline(R"({"tracks":[{"entity":"a","property":"yaw","keys":[[0,1]]},{"entity":"a","property":"yaw","keys":[[0,2]]}]})");},"duplicates");
    rejects([]{swan::parseTimeline(R"({"loop":true})");},"looping timeline");
    auto broken=scene;broken.timeline().tracks[0].target="ghost";
    rejects([&]{broken.validate();},"unknown entity: ghost");
}
void documents() {
    auto directory=std::filesystem::temp_directory_path()/("swan-fx-"+std::to_string(getpid()));
    std::filesystem::create_directories(directory);
    swan::SceneDocument document;document.scene=stage();
    swan::EditorDocument editor(document);
    editor.apply(swan::SetEffect{"pop",effect(R"({"emitters":[{"bursts":[[0,5]]}]})")});
    swan::Entity e;e.key="emitter";e.effectId="pop";
    editor.apply(swan::CreateEntity{e,{}});
    editor.apply(swan::SetTimeline{swan::parseTimeline(R"({"tracks":[{"entity":"emitter","property":"yaw","keys":[[0,0],[1,3]]}],"events":[{"time":1,"effect":"pop","entity":"emitter"}]})")});
    rejects([&]{editor.apply(swan::SetEntityEffect{"emitter","missing"});},"Unknown editor effect");
    editor.save(directory/"fx.swan.json");
    auto loaded=swan::loadScene(directory/"fx.swan.json");
    require(loaded.scene.get(loaded.scene.find("emitter"))->effectId=="pop" && loaded.scene.timeline().tracks.size()==1 && loaded.scene.effects().contains("pop"),"Version 7 round trip");
    require(swan::serializeScene(loaded)==swan::serializeScene(editor.document()),"Saved FX scene is not stable");
    // Deleting the effect clears references and events; deleting an entity drops its tracks. Both undo.
    editor.apply(swan::SetEffect{"pop",std::nullopt});
    const auto& scene=editor.document().scene;
    require(scene.get(scene.find("emitter"))->effectId.empty() && scene.timeline().events.empty() && !scene.effects().contains("pop"),"Effect delete cascade");
    require(editor.undo() && editor.document().scene.timeline().events.size()==1,"Undo effect delete");
    editor.apply(swan::DeleteEntity{"emitter"});
    require(editor.document().scene.timeline().tracks.empty() && editor.document().scene.timeline().events.empty(),"Entity delete cascade");
    require(editor.undo() && editor.document().scene.timeline().tracks.size()==1,"Undo entity delete");
    // Older versions cannot declare effects.
    auto json=Json::parse(swan::serializeScene(editor.document()));
    json["version"]=6;
    rejects([&]{swan::parseScene(json.dump());},"version 7");
    json["version"]=8;
    rejects([&]{swan::parseScene(json.dump());},"Unsupported");
    json["version"]=7;json["effects"]["pop"]["emitters"][0]["bursts"]=Json::array({Json::array({0,-1})});
    rejects([&]{swan::parseScene(json.dump());},"Effect pop");
    std::filesystem::remove_all(directory);
}
void images() {
    swan::TextureData image{4,2,std::vector<uint8_t>(4*2*4,0)};
    for(size_t i=0;i<image.rgba.size();i+=4) {image.rgba[i]=(i/4)%2?255:0;image.rgba[i+3]=255;}
    auto half=swan::downsample(image,2);
    require(half.width==2 && half.height==1 && half.rgba[0]>180 && half.rgba[0]<195,"Linear-light downsample of black/white is ~188");
    auto sheet=swan::contactSheet({half,half,half},{"t=0","",""},2);
    require(sheet.width==2*2+3*4 && sheet.height==2*1+3*4,"Contact sheet layout");
    auto path=std::filesystem::temp_directory_path()/("swan-image-"+std::to_string(getpid())+".png");
    swan::TextureData labeled{64,16,std::vector<uint8_t>(64*16*4,0)};
    swan::drawText(labeled,1,1,"T=1.5S",{255,255,255,255});
    swan::savePng(path,labeled);
    auto loaded=swan::loadPngTexture(path);
    require(loaded->width==64 && loaded->rgba==labeled.rgba,"PNG write/read round trip");
    std::filesystem::remove(path);
    rejects([&]{swan::downsample(image,3);},"multiple");
}
int main() {
    try {
        curves();std::cout<<"curves: ok\n";
        effectSchema();std::cout<<"effect schema and errors: ok\n";
        bursts();std::cout<<"bursts and lifetimes: ok\n";
        rateCapAndDeterminism();std::cout<<"rate, cap, determinism: ok\n";
        attachedAndLocal();std::cout<<"attached and local emitters: ok\n";
        timeline();std::cout<<"timeline tracks, events, camera: ok\n";
        documents();std::cout<<"editor commands and scene version 7: ok\n";
        images();std::cout<<"image helpers: ok\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
