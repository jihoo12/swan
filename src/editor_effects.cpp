#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "editor_widgets.hpp"
#include "fx_io.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_stdlib.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace swan {
using namespace ui;
namespace {
std::string uniqueEffectId(const EffectAssets& effects,const std::string& base) {
    if(!effects.contains(base)) return base;
    for(int i=2;;++i) if(auto id=base+"-"+std::to_string(i);!effects.contains(id)) return id;
}
// A pleasant default: a looping fountain of glowing sparks.
EmitterDef starterEmitter() {
    EmitterDef e;
    e.name="sparks";e.loop=true;e.rate=40;e.lifetime={0.6f,1.2f};e.speed={1,2.5f};e.spread=25;e.gravity={0,-2,0};
    e.size={0.08f,0.16f};e.sizeOverLife.keys={{0,1},{1,0}};
    e.colorOverLife.keys={{0,{1,0.85f,0.5f,0}},{0.1f,{1,0.6f,0.25f,1}},{1,{1,0.25f,0.1f,0}}};
    e.intensity=3;
    return e;
}
template<class E,size_t N> bool enumCombo(const char* id,E& value,const E (&options)[N],const char* (*name)(E)) {
    bool changed=false;
    if(ImGui::BeginCombo(id,name(value))) {
        for(auto option:options) if(ImGui::Selectable(name(option),option==value)) {value=option;changed=true;}
        ImGui::EndCombo();
    }
    return changed;
}
bool range(const char* id,Range& r,float speed,float minimum,float maximum,const char* format) {
    return ImGui::DragFloatRange2(id,&r.min,&r.max,speed,minimum,maximum,format,nullptr,ImGuiSliderFlags_AlwaysClamp);
}
// Read-only previews of the over-life curves (edit them in JSON).
void curvePreview(const FloatCurve& size,const ColorCurve& color) {
    auto* draw=ImGui::GetWindowDrawList();
    float width=ImGui::GetContentRegionAvail().x,height=ImGui::GetFrameHeight()*1.6f;
    auto pos=ImGui::GetCursorScreenPos();
    draw->AddRectFilled(pos,{pos.x+width,pos.y+height},theme::Surface,4);
    constexpr int samples=48;
    float bar=height*0.35f;
    for(int i=0;i<samples;++i) {
        float t0=float(i)/samples,t1=float(i+1)/samples;
        auto c=color.sample((t0+t1)*0.5f,glm::vec4(1));
        ImU32 rgba=ImGui::ColorConvertFloat4ToU32({std::min(c.r,1.0f),std::min(c.g,1.0f),std::min(c.b,1.0f),std::clamp(c.a,0.0f,1.0f)});
        draw->AddRectFilled({pos.x+t0*width,pos.y+height-bar},{pos.x+t1*width,pos.y+height},rgba);
    }
    float peak=1;
    for(const auto& key:size.keys) peak=std::max(peak,key.value);
    ImVec2 previous{};
    for(int i=0;i<=samples;++i) {
        float t=float(i)/samples;
        ImVec2 point{pos.x+t*width,pos.y+(height-bar)-(size.sample(t,1)/peak)*(height-bar-4)};
        if(i) draw->AddLine(previous,point,theme::Accent,1.5f);
        previous=point;
    }
    ImGui::Dummy({width,height});
    ImGui::SetItemTooltip("Size over life (line) and color over life (bar); edit curves in JSON below");
}
}
void EditorLayer::attachEffect(const std::string& key,const std::string& effectId) {
    const auto& scene=document.document().scene;
    const auto* entity=scene.get(scene.find(key));
    if(!entity || entity->effectId==effectId) return;
    auto display=entity->name.empty()?key:entity->name;
    if(attempt([&]{document.apply(SetEntityEffect{key,effectId},effectId.empty()?"Remove effect from "+display:"Attach "+effectId+" to "+display);}))
        notify(Level::Success,effectId.empty()?"Removed effect":"Attached effect "+effectId);
}
void EditorLayer::newEffect() {
    if(!editable()) return;
    auto id=uniqueEffectId(document.document().scene.effects(),"effect");
    EffectDef effect;effect.emitters.push_back(starterEmitter());
    std::vector<SceneEdit> edits{SetEffect{id,effect}};
    const auto* entity=selectedEntity();
    if(entity) edits.push_back(SetEntityEffect{entity->key,id});
    if(!attempt([&]{document.apply(edits,"Create effect "+id);})) return;
    selectedEffect=id;showEffectEditor=true;focusWindow="###EffectEditor";
    notify(Level::Success,entity?"Created effect "+id+" on "+(entity->name.empty()?entity->key:entity->name):"Created effect "+id);
    if(!fxPlaying) {fxPlaying=true;} // Show it immediately.
}
void EditorLayer::drawEffectSection(const Entity& entity) {
    if(!sectionHeader(icon::Flame,"Effect","effect")) return;
    const auto& scene=document.document().scene;
    const auto key=entity.key;
    std::optional<std::string> chosen;
    if(beginProperties("##effect")) {
        property("Particles");
        float edit=ImGui::GetFrameHeight();
        ImGui::SetNextItemWidth(-edit-ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::BeginCombo("##effect-id",entity.effectId.empty()?"None":entity.effectId.c_str())) {
            if(ImGui::Selectable("None",entity.effectId.empty())) chosen=std::string();
            for(const auto& [id,effect]:scene.effects().entries()) {
                if(ImGui::Selectable((std::string(icon::Flame)+"  "+id).c_str(),id==entity.effectId)) chosen=id;
                ImGui::SetItemTooltip("%zu emitter(s)",effect->emitters.size());
            }
            ImGui::EndCombo();
        }
        probe::item("inspector/effect");
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_EFFECT")) chosen=std::string(static_cast<const char*>(payload->Data));
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        ImGui::BeginDisabled(entity.effectId.empty());
        if(ImGui::Button(icon::Pencil,{edit,0})) {selectedEffect=entity.effectId;showEffectEditor=true;focusWindow="###EffectEditor";}
        ImGui::SetItemTooltip("Edit this effect");
        ImGui::EndDisabled();
        ImGui::EndTable();
    }
    // Which properties the timeline animates (they override authored values while previewing).
    std::string animated;
    for(const auto& track:scene.timeline().tracks)
        if(track.kind==TrackTarget::Entity && track.target==key) animated+=(animated.empty()?"":", ")+std::string(propertyName(track.property));
    if(!animated.empty()) ImGui::TextColored(toVec4(IM_COL32(236,190,110,255)),"%s  Animated: %s",icon::Diamond,animated.c_str());
    if(ImGui::SmallButton((std::string(icon::Plus)+"  New effect on this entity").c_str())) newEffect();
    if(chosen && *chosen!=entity.effectId) attachEffect(key,*chosen);
}
void EditorLayer::drawEnvironment() {
    const auto& scene=document.document().scene;
    auto environment=scene.environment();
    bool changed=false;
    if(sectionHeader(icon::Sun,"Environment","environment") && beginProperties("##environment")) {
        property("Background");changed|=ImGui::ColorEdit3("##background",&environment.background.x,ImGuiColorEditFlags_Float);
        probe::item("environment/background");
        property("Fog");changed|=ImGui::DragFloat("##fog",&environment.fog,0.00005f,0,0.1f,"%.5f",ImGuiSliderFlags_AlwaysClamp);
        property("Ambient");changed|=ImGui::DragFloat("##ambient",&environment.ambient,0.01f,0,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        property("Sun");changed|=ImGui::DragFloat("##sun",&environment.sun,0.01f,0,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        property("Local light");changed|=ImGui::DragFloat("##local",&environment.localLight,0.01f,0,10,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        property("Exposure");changed|=ImGui::DragFloat("##exposure",&environment.exposure,0.01f,0.01f,16,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        property("Bloom");changed|=ImGui::DragFloat("##bloom",&environment.bloom,0.01f,0,4,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        probe::item("environment/bloom");
        property("Threshold");changed|=ImGui::DragFloat("##threshold",&environment.bloomThreshold,0.01f,0,16,"%.2f",ImGuiSliderFlags_AlwaysClamp);
        property("Tone map");
        static const ToneMap maps[]={ToneMap::Reinhard,ToneMap::Aces};
        changed|=enumCombo("##tonemap",environment.toneMap,maps,+[](ToneMap m)->const char*{return m==ToneMap::Aces?"ACES":"Reinhard";});
        ImGui::EndTable();
    }
    if(changed) preview(SetEnvironment{environment},"Edit environment");
    ImGui::Spacing();
    ImGui::TextColored(toVec4(theme::TextDim),"%s  %zu entities  \xc2\xb7  %zu effects  \xc2\xb7  select an entity to inspect it",icon::Info,scene.size(),scene.effects().entries().size());
}
void EditorLayer::drawEffectEditor() {
    auto title=std::string(icon::Flame)+"  Effect###EffectEditor";
    if(auto* inspector=ImGui::FindWindowByName("###Inspector");inspector && inspector->DockId) ImGui::SetNextWindowDockID(inspector->DockId,ImGuiCond_FirstUseEver);
    if(!ImGui::Begin(title.c_str(),&showEffectEditor)) {ImGui::End();return;}
    const auto& scene=document.document().scene;
    std::function<void()> action;
    ImGui::BeginDisabled(!editable());
    if(!selectedEffect.empty() && !scene.effects().contains(selectedEffect)) selectedEffect.clear();
    if(selectedEffect.empty() && !scene.effects().entries().empty()) selectedEffect=scene.effects().entries().begin()->first;
    float buttons=(ImGui::GetFrameHeight()+ImGui::GetStyle().ItemSpacing.x)*3;
    ImGui::SetNextItemWidth(-buttons);
    if(ImGui::BeginCombo("##effect-select",selectedEffect.empty()?"No effects":selectedEffect.c_str())) {
        for(const auto& [id,effect]:scene.effects().entries()) {
            (void)effect;
            if(ImGui::Selectable((std::string(icon::Flame)+"  "+id).c_str(),id==selectedEffect)) selectedEffect=id;
        }
        ImGui::EndCombo();
    }
    probe::item("effect/select");
    ImGui::SameLine();
    if(ImGui::Button(icon::Plus)) action=[this]{newEffect();};
    probe::item("effect/new");
    ImGui::SetItemTooltip("New effect (attached to the selection, if any)");
    ImGui::SameLine();
    ImGui::BeginDisabled(selectedEffect.empty() || !selectedEntity());
    if(ImGui::Button(icon::Box)) action=[this,key=selectedEntity()->key,id=selectedEffect]{attachEffect(key,id);};
    ImGui::SetItemTooltip("Attach to the selected entity");
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::BeginDisabled(selectedEffect.empty());
    if(ImGui::Button(icon::Trash)) action=[this,id=selectedEffect]{
        if(attempt([&]{document.apply(SetEffect{id,std::nullopt});})) notify(Level::Info,"Deleted effect "+id+" (Ctrl+Z restores it)");
    };
    ImGui::SetItemTooltip("Delete the effect (detaches it and removes its timeline events)");
    ImGui::EndDisabled();
    if(selectedEffect.empty()) {
        ImGui::EndDisabled();
        ImGui::Spacing();
        ImGui::TextColored(toVec4(theme::TextDim),"%s  Effects are particle emitters. Create one with +.",icon::Info);
        ImGui::End();
        if(action) action();
        return;
    }
    const auto id=selectedEffect;
    EffectDef effect=*scene.effects().get(id); // Copy: previews replace the scene while drawing.
    bool changed=false;
    std::optional<size_t> removeEmitter,duplicateEmitter;
    static const EmitterShape shapes[]={EmitterShape::Point,EmitterShape::Sphere,EmitterShape::Box,EmitterShape::Ring,EmitterShape::Disc};
    static const ParticleBlend blends[]={ParticleBlend::Additive,ParticleBlend::Alpha};
    static const ParticleSprite sprites[]={ParticleSprite::Soft,ParticleSprite::Circle,ParticleSprite::Ring,ParticleSprite::Square,ParticleSprite::Spark,ParticleSprite::Smoke};
    for(size_t i=0;i<effect.emitters.size();++i) {
        auto& e=effect.emitters[i];
        ImGui::PushID(int(i));
        auto heading=(e.name.empty()?"Emitter "+std::to_string(i+1):e.name)+"###emitter";
        if(sectionHeader(icon::Sparkles,heading.c_str(),("emitter"+std::to_string(i)).c_str())) {
            if(beginProperties("##emitter")) {
                property("Name");changed|=ImGui::InputText("##name",&e.name);
                property("Rate");changed|=ImGui::DragFloat("##rate",&e.rate,0.5f,0,100000,"%.1f /s",ImGuiSliderFlags_AlwaysClamp);
                probe::item("effect/"+std::to_string(i)+"/rate");
                property("Bursts");
                {
                    // The first burst is editable here; more bursts live in JSON.
                    int count=e.bursts.empty()?0:int(e.bursts.front().count);
                    if(ImGui::DragInt("##burst",&count,1,0,int(maxParticlesPerEmitter),count?"%d at start":"none",ImGuiSliderFlags_AlwaysClamp)) {
                        if(count==0) e.bursts.clear();
                        else if(e.bursts.empty()) e.bursts.push_back({0,uint32_t(count)});
                        else e.bursts.front().count=uint32_t(count);
                        changed=true;
                    }
                }
                property("Delay");changed|=ImGui::DragFloat("##delay",&e.delay,0.01f,0,3600,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
                property("Duration");changed|=ImGui::DragFloat("##duration",&e.duration,0.01f,0.01f,3600,"%.2f s",ImGuiSliderFlags_AlwaysClamp);
                property("Loop");changed|=ImGui::Checkbox("##loop",&e.loop);
                ImGui::SameLine();ImGui::TextColored(toVec4(theme::TextDim),"repeat the emission window");
                property("Lifetime");changed|=range("##lifetime",e.lifetime,0.01f,0.01f,600,"%.2f s");
                property("Shape");changed|=enumCombo("##shape",e.shape,shapes,shapeName);
                if(e.shape==EmitterShape::Sphere || e.shape==EmitterShape::Ring || e.shape==EmitterShape::Disc) {property("Radius");changed|=ImGui::DragFloat("##radius",&e.radius,0.01f,0,1000,"%.2f",ImGuiSliderFlags_AlwaysClamp);}
                if(e.shape==EmitterShape::Box) {property("Box");changed|=vectorControl("box",e.size3,0.01f,1,nullptr,"%.2f",0.0001f);}
                if(e.shape!=EmitterShape::Point && e.shape!=EmitterShape::Ring) {property("Surface");changed|=ImGui::Checkbox("##surface",&e.surface);}
                property("Offset");changed|=vectorControl("offset",e.offset,0.01f,0,nullptr);
                property("Direction");changed|=vectorControl("direction",e.direction,0.01f,0,nullptr);
                property("Spread");changed|=ImGui::SliderFloat("##spread",&e.spread,0,180,"%.0f\xc2\xb0");
                property("Radial");changed|=ImGui::Checkbox("##radial",&e.radial);
                property("Speed");changed|=range("##speed",e.speed,0.01f,-1000,1000,"%.2f");
                property("Gravity");changed|=vectorControl("gravity",e.gravity,0.05f,0,nullptr);
                property("Drag");changed|=ImGui::DragFloat("##drag",&e.drag,0.01f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                property("Noise");changed|=ImGui::DragFloat("##noise",&e.noise,0.01f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                property("Orbit");changed|=ImGui::DragFloat("##orbit",&e.orbit,0.01f,-100,100,"%.2f rad/s",ImGuiSliderFlags_AlwaysClamp);
                property("Size");changed|=range("##size",e.size,0.002f,0,1000,"%.3f");
                property("Color");
                changed|=ImGui::ColorEdit4("##color",&e.color.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_AlphaBar|ImGuiColorEditFlags_NoInputs);
                ImGui::SameLine();ImGui::TextColored(toVec4(theme::TextDim),"%.2f %.2f %.2f  \xce\xb1 %.2f",e.color.r,e.color.g,e.color.b,e.color.a);
                property("Intensity");changed|=ImGui::DragFloat("##intensity",&e.intensity,0.02f,0,1000,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                probe::item("effect/"+std::to_string(i)+"/intensity");
                property("Over life");curvePreview(e.sizeOverLife,e.colorOverLife);
                property("Blend");changed|=enumCombo("##blend",e.blend,blends,blendName);
                property("Sprite");changed|=enumCombo("##sprite",e.sprite,sprites,spriteName);
                property("Stretch");changed|=ImGui::DragFloat("##stretch",&e.stretch,0.005f,0,10,"%.3f",ImGuiSliderFlags_AlwaysClamp);
                property("Spin");changed|=range("##spin",e.spin,1,-36000,36000,"%.0f\xc2\xb0/s");
                property("Space");
                {
                    int space=e.local?1:0;
                    if(ImGui::Combo("##space",&space,"world\0local\0")) {e.local=space==1;changed=true;}
                }
                property("Max");
                {
                    int maximum=int(e.maxParticles);
                    if(ImGui::DragInt("##max",&maximum,5,1,int(maxParticlesPerEmitter),"%d particles",ImGuiSliderFlags_AlwaysClamp)) {e.maxParticles=uint32_t(maximum);changed=true;}
                }
                ImGui::EndTable();
            }
            if(ImGui::SmallButton((std::string(icon::Copy)+"  Duplicate").c_str())) duplicateEmitter=i;
            ImGui::SameLine();
            ImGui::BeginDisabled(effect.emitters.size()<=1);
            if(ImGui::SmallButton((std::string(icon::Trash)+"  Remove").c_str())) removeEmitter=i;
            ImGui::EndDisabled();
        }
        ImGui::PopID();
    }
    ImGui::Spacing();
    ImGui::BeginDisabled(effect.emitters.size()>=maxEmittersPerEffect);
    if(ImGui::Button((std::string(icon::Plus)+"  Add emitter").c_str())) {
        auto next=effect;next.emitters.push_back(starterEmitter());next.emitters.back().name="emitter-"+std::to_string(next.emitters.size());
        action=[this,id,next]{attempt([&]{document.apply(SetEffect{id,next},"Add emitter to "+id);});};
    }
    ImGui::EndDisabled();
    if(removeEmitter) {auto next=effect;next.emitters.erase(next.emitters.begin()+long(*removeEmitter));action=[this,id,next]{attempt([&]{document.apply(SetEffect{id,next},"Remove emitter from "+id);});};}
    if(duplicateEmitter) {
        auto next=effect;
        if(next.emitters.size()<maxEmittersPerEffect) {auto copy=next.emitters[*duplicateEmitter];copy.name+=" copy";next.emitters.insert(next.emitters.begin()+long(*duplicateEmitter)+1,copy);}
        action=[this,id,next]{attempt([&]{document.apply(SetEffect{id,next},"Duplicate emitter in "+id);});};
    }
    // Full JSON (the same schema as scene files and the Lua API); every field, including curves.
    if(sectionHeader(icon::FileCode,"JSON","effect-json")) {
        bool stale=effectJsonFor!=id || (effectJsonRevision!=document.revision() && !ImGui::IsItemActive());
        ImGuiID textId=ImGui::GetID("##effect-json");
        if(stale && ImGui::GetActiveID()!=textId) {effectJson=serializeEffect(effect);effectJsonFor=id;effectJsonRevision=document.revision();effectError.clear();}
        ImGui::InputTextMultiline("##effect-json",&effectJson,{-FLT_MIN,ImGui::GetTextLineHeight()*14},ImGuiInputTextFlags_AllowTabInput);
        probe::item("effect/json");
        if(ImGui::Button((std::string(icon::Check)+"  Apply JSON").c_str())) {
            try {
                auto parsed=parseEffect(effectJson);
                action=[this,id,parsed]{if(attempt([&]{document.apply(SetEffect{id,parsed},"Edit effect "+id+" (JSON)");})) effectJsonRevision=0;};
                effectError.clear();
            } catch(const std::exception& error) {effectError=error.what();}
        }
        probe::item("effect/apply-json");
        ImGui::SameLine();
        if(ImGui::Button((std::string(icon::Refresh)+"  Revert").c_str())) {effectJsonFor.clear();effectError.clear();}
        if(!effectError.empty()) {ImGui::PushStyleColor(ImGuiCol_Text,toVec4(theme::Error));ImGui::TextWrapped("%s",effectError.c_str());ImGui::PopStyleColor();}
        else ImGui::TextColored(toVec4(theme::TextDim),"See docs/FX.md for every field.");
    }
    ImGui::EndDisabled();
    ImGui::End();
    if(changed) {
        try {validateEffect(effect);preview(SetEffect{id,effect},"Edit effect "+id);}
        catch(const std::exception& error) {if(logs.empty() || logs.back().text!=error.what()) log(Level::Warning,error.what());}
    }
    if(action) action();
}
}
