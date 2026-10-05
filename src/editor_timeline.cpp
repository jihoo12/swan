#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "fx_runtime.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
namespace swan {
namespace {
constexpr double maxPreviewSeconds=600;
constexpr float halfTick=0.5f/120;
// Inserts a key at `time`, replacing one within half a tick.
template<class T> void setKey(Curve<T>& curve,float time,T value) {
    for(auto& key:curve.keys) if(std::abs(key.time-time)<=halfTick) {key.value=value;return;}
    Keyframe<T> key{time,value,Ease::Smooth};
    auto at=std::upper_bound(curve.keys.begin(),curve.keys.end(),time,[](float t,const Keyframe<T>& k){return t<k.time;});
    curve.keys.insert(at,key);
}
template<class T> void moveKey(Curve<T>& curve,size_t index,float time) {
    if(index>=curve.keys.size()) return;
    curve.keys[index].time=std::max(0.0f,time);
    std::stable_sort(curve.keys.begin(),curve.keys.end(),[](const auto& a,const auto& b){return a.time<b.time;});
}
template<class T> std::vector<float> keyTimes(const Curve<T>& curve) {
    std::vector<float> times;
    for(const auto& key:curve.keys) times.push_back(key.time);
    return times;
}
std::string seconds(double value) {char text[32];std::snprintf(text,sizeof text,"%.2f s",value);return text;}
// One timeline row: the markers it draws and how to edit them in a copy of the timeline.
struct Row {
    std::string label,probe;
    std::vector<float> times;
    bool events=false;
    std::function<void(Timeline&,size_t,float)> move;
    std::function<void(Timeline&,size_t)> erase;
    std::function<void(Timeline&,size_t,Ease)> ease;        // Empty for events.
    std::function<Ease(const Timeline&,size_t)> easeOf;
    std::function<std::string(const Timeline&,size_t)> describe;
    std::function<void(Timeline&)> remove;
};
template<class T> std::string valueText(const T& value) {
    char text[96];
    if constexpr(std::is_same_v<T,float>) std::snprintf(text,sizeof text,"%.3f",value);
    else std::snprintf(text,sizeof text,"(%.2f, %.2f, %.2f)",value.x,value.y,value.z);
    return text;
}
template<class T> void curveRow(Row& row,Curve<T>& (*access)(Timeline&,size_t),size_t slot,const Curve<T>& curve) {
    row.times=keyTimes(curve);
    row.move=[access,slot](Timeline& t,size_t i,float time){moveKey(access(t,slot),i,time);};
    row.erase=[access,slot](Timeline& t,size_t i){auto& c=access(t,slot);if(i<c.keys.size()) c.keys.erase(c.keys.begin()+long(i));};
    row.ease=[access,slot](Timeline& t,size_t i,Ease e){auto& c=access(t,slot);if(i<c.keys.size()) c.keys[i].ease=e;};
    row.easeOf=[access,slot](const Timeline& t,size_t i){auto& c=access(const_cast<Timeline&>(t),slot);return i<c.keys.size()?c.keys[i].ease:Ease::Linear;};
    row.describe=[access,slot](const Timeline& t,size_t i){
        auto& c=access(const_cast<Timeline&>(t),slot);
        return i<c.keys.size()?seconds(c.keys[i].time)+"  "+valueText(c.keys[i].value)+"  "+easeName(c.keys[i].ease):std::string();
    };
}
Vec3Curve& trackVector(Timeline& t,size_t i) {return t.tracks[i].vector;}
FloatCurve& trackScalar(Timeline& t,size_t i) {return t.tracks[i].scalar;}
Vec3Curve& cameraVector(Timeline& t,size_t i) {return i==0?t.camera.position:t.camera.target;}
FloatCurve& cameraFov(Timeline& t,size_t) {return t.camera.fov;}
// Keys `property` of `local` for entity `key` at `time`; false when the track limit is reached.
bool addKey(Timeline& timeline,const std::string& key,const Transform& local,TrackProperty property,float time) {
    TimelineTrack* track=nullptr;
    for(auto& existing:timeline.tracks)
        if(existing.kind==TrackTarget::Entity && existing.target==key && existing.property==property) {track=&existing;break;}
    if(!track) {
        if(timeline.tracks.size()>=maxTimelineTracks) return false;
        timeline.tracks.push_back({TrackTarget::Entity,key,property,{},{}});
        track=&timeline.tracks.back();
    }
    if(property==TrackProperty::Position) setKey(track->vector,time,local.position);
    else if(property==TrackProperty::Scale) setKey(track->vector,time,local.scale);
    else if(property==TrackProperty::Yaw) setKey(track->scalar,time,local.yaw);
    else return false;
    return true;
}
// Tracks and camera curves must keep at least one key; an emptied row disappears.
void dropEmpty(Timeline& t) {
    std::erase_if(t.tracks,[](const TimelineTrack& track){return track.isVector()?track.vector.empty():track.scalar.empty();});
}
}
// ---------------------------------------------------------------------------------------------
// Preview
float playheadTime(const Timeline& timeline,double time) {
    float length=timeline.length();
    if(timeline.loop && length>0) return float(std::fmod(time,double(length)));
    return float(time);
}
void EditorLayer::updateFxPreview(float dt) {
    if(play) {fxPlaying=false;fxPreview.reset();displayed.reset();return;}
    const auto& current=document.document();
    const auto& timeline=current.scene.timeline();
    float length=timeline.length();
    if(fxPlaying) {
        fxTime+=dt;
        double end=length>0?double(length):maxPreviewSeconds;
        if(fxTime>end) {
            if(fxLoop && length>0) fxTime=std::fmod(fxTime,end);
            else {fxTime=end;fxPlaying=false;}
        }
    }
    // The authored scene as the timeline poses it at the playhead (also while dragging edits).
    if(timeline.empty()) displayed.reset();
    else {displayed=current;applyTimeline(displayed->scene,timeline.localTime(fxTime));}
    bool wanted=fxPlaying || fxTime>0;
    if(!wanted) {fxPreview.reset();return;}
    if(document.previewing()) return; // Rebuild once the edit commits.
    try {
        if(!fxPreview || fxRevision!=document.revision()) {
            fxPreview=std::make_unique<FxPlayer>(current);
            fxRevision=document.revision();
        }
        fxPreview->seek(fxTime);
        for(auto& line:fxPreview->takeMessages()) log(Level::Info,"[preview] "+line);
        fxPreviewFailed=false;
    } catch(const std::exception& error) {
        if(!fxPreviewFailed) notify(Level::Error,std::string("Effect preview: ")+error.what());
        fxPreviewFailed=true;fxPreview.reset();fxPlaying=false;
    }
}
bool EditorLayer::fxShowing() const {return !play && fxPreview && (fxPlaying || fxTime>0) && !document.previewing();}
void EditorLayer::setPlayhead(double seconds) {fxTime=std::clamp(seconds,0.0,maxPreviewSeconds);}
// ---------------------------------------------------------------------------------------------
// Keys
std::optional<Timeline> EditorLayer::keyedTimeline(const std::string& key,const Transform& local,TrackProperty property) const {
    auto timeline=document.document().scene.timeline();
    if(!addKey(timeline,key,local,property,playheadTime(timeline,fxTime))) return std::nullopt;
    return timeline;
}
void EditorLayer::keySelection(std::initializer_list<TrackProperty> properties) {
    const auto* entity=selectedEntity();
    if(!entity || !editable()) return;
    // Key what the viewport shows: the timeline pose for animated properties, else authored.
    const auto& shown=displayedScene();
    auto local=shown.get(shown.find(entity->key))->transform;
    auto timeline=document.document().scene.timeline();
    float time=playheadTime(timeline,fxTime);
    std::string names;
    for(auto property:properties)
        if(addKey(timeline,entity->key,local,property,time)) names+=(names.empty()?"":", ")+std::string(propertyName(property));
    if(names.empty()) {notify(Level::Warning,"The timeline is full (512 tracks)");return;}
    auto label="Key "+names+" of "+(entity->name.empty()?entity->key:entity->name)+" at "+seconds(time);
    if(attempt([&]{document.apply(SetTimeline{timeline},label);})) notify(Level::Success,label);
}
void EditorLayer::keyCamera() {
    if(!editable()) return;
    auto timeline=document.document().scene.timeline();
    float time=playheadTime(timeline,fxTime);
    auto view=camera.camera();
    setKey(timeline.camera.position,time,view.position);
    setKey(timeline.camera.target,time,view.position+forward(view.yaw,view.pitch)*std::max(camera.pivotDistance(),0.5f));
    setKey(timeline.camera.fov,time,view.fov);
    auto label="Key camera at "+seconds(time);
    if(attempt([&]{document.apply(SetTimeline{timeline},label);})) {fxShotCamera=true;notify(Level::Success,label);}
}
// ---------------------------------------------------------------------------------------------
// Panel
void EditorLayer::drawTimeline() {
    auto title=std::string(icon::Film)+"  Timeline###Timeline";
    // Layouts saved before this panel existed: dock it with Assets instead of floating.
    if(!ImGui::FindWindowSettingsByID(ImHashStr("###Timeline")))
        if(auto* assets=ImGui::FindWindowByName("###Assets");assets && assets->DockId) ImGui::SetNextWindowDockID(assets->DockId);
    // Never steal the tab from Assets when it first appears (it is shown on demand or by clicking).
    if(!ImGui::Begin(title.c_str(),&showTimeline,ImGuiWindowFlags_NoFocusOnAppearing)) {ImGui::End();return;}
    const auto& scene=document.document().scene;
    const Timeline timeline=scene.timeline(); // Copy: edits below replace the scene.
    float length=timeline.length();
    std::optional<std::pair<Timeline,std::string>> commit,live;
    auto& style=ImGui::GetStyle();
    // Transport.
    ImGui::BeginDisabled(bool(play));
    if(ImGui::Button(icon::SkipBack)) {fxPlaying=false;fxTime=0;}
    probe::item("timeline/rewind");
    ImGui::SetItemTooltip("Back to the start (Shift+Space)");
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,toVec4(fxPlaying?IM_COL32(40,96,72,255):theme::Surface));
    if(ImGui::Button(fxPlaying?icon::Pause:icon::Play)) actions.run("timeline.play");
    ImGui::PopStyleColor();
    probe::item("timeline/play");
    ImGui::SetItemTooltip("Play / pause the effect preview (Space)");
    ImGui::SameLine();
    auto toggle=[&](const char* glyph,bool& value,const char* tip,const char* id){
        bool on=value; // Pop what was pushed, even when this click flips the value.
        if(on) ImGui::PushStyleColor(ImGuiCol_Button,toVec4(theme::AccentSoft));
        if(ImGui::Button(glyph)) value=!value;
        if(on) ImGui::PopStyleColor();
        probe::item(id);
        ImGui::SetItemTooltip("%s",tip);
    };
    toggle(icon::Repeat,fxLoop,"Loop the preview","timeline/loop-preview");
    ImGui::SameLine();
    toggle(icon::Clapperboard,fxShotCamera,"View through the shot camera","timeline/shot-camera");
    ImGui::SameLine(0,14);
    char clock[64];std::snprintf(clock,sizeof clock,"%6.2f / %.2f s",fxTime,length);
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted(clock);
    if(fxShowing()) {ImGui::SameLine();ImGui::TextColored(toVec4(theme::TextDim),"%s %zu",icon::Sparkles,fxPreview->effects().particleCount());}
    ImGui::SameLine(0,14);
    // Auto Key turns red like a record button.
    bool recording=autoKey;
    if(recording) {ImGui::PushStyleColor(ImGuiCol_Button,toVec4(IM_COL32(150,52,58,255)));ImGui::PushStyleColor(ImGuiCol_Text,toVec4(IM_COL32(255,220,220,255)));}
    if(ImGui::Button((std::string(icon::Diamond)+" Auto Key").c_str())) autoKey=!autoKey;
    if(recording) ImGui::PopStyleColor(2);
    probe::item("timeline/autokey");
    ImGui::SetItemTooltip("Transform edits record keys at the playhead");
    ImGui::SameLine();
    if(ImGui::Button((std::string(icon::Diamond)+" Key").c_str())) ImGui::OpenPopup("##key-menu");
    probe::item("timeline/key");
    ImGui::SetItemTooltip("Insert keys at the playhead (I keys the selection's transform)");
    if(ImGui::BeginPopup("##key-menu")) {
        bool selection=selectedEntity()!=nullptr;
        if(ImGui::MenuItem("Transform (position, rotation, scale)","I",false,selection)) keySelection({TrackProperty::Position,TrackProperty::Yaw,TrackProperty::Scale});
        if(ImGui::MenuItem("Position","",false,selection)) keySelection({TrackProperty::Position});
        if(ImGui::MenuItem("Rotation","",false,selection)) keySelection({TrackProperty::Yaw});
        if(ImGui::MenuItem("Scale","",false,selection)) keySelection({TrackProperty::Scale});
        ImGui::Separator();
        if(ImGui::MenuItem((std::string(icon::Camera)+"  Viewport camera").c_str())) keyCamera();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    if(ImGui::Button((std::string(icon::Zap)+" Event").c_str())) ImGui::OpenPopup("##event-menu");
    probe::item("timeline/add-event");
    ImGui::SetItemTooltip("Play a one-shot effect at the playhead (on the selection, or at the origin)");
    if(ImGui::BeginPopup("##event-menu")) {
        if(scene.effects().entries().empty()) ImGui::TextColored(toVec4(theme::TextDim),"No effects yet");
        for(const auto& [id,effect]:scene.effects().entries()) {
            (void)effect;
            if(ImGui::MenuItem((std::string(icon::Flame)+"  "+id).c_str())) {
                auto next=timeline;
                TimelineEvent event;event.time=playheadTime(timeline,fxTime);event.effect=id;
                if(const auto* entity=selectedEntity()) event.entity=entity->key; else event.position={0,1,0};
                next.events.push_back(event);
                std::stable_sort(next.events.begin(),next.events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
                commit={next,"Add event "+id+" at "+seconds(event.time)};
            }
        }
        ImGui::EndPopup();
    }
    // Right: timeline length and looping (authored).
    float fieldWidth=90*lastScale;
    float right=fieldWidth+ImGui::CalcTextSize("Length").x+ImGui::CalcTextSize("Loop").x+ImGui::GetFrameHeight()+style.ItemSpacing.x*4+style.ItemInnerSpacing.x*2;
    // Right-aligned when it fits; otherwise it wraps below the transport row.
    ImGui::SameLine();
    if(ImGui::GetCursorPosX()+right<=contentRight()) ImGui::SameLine(contentRight()-right); else ImGui::NewLine();
    ImGui::AlignTextToFramePadding();ImGui::TextColored(toVec4(theme::TextDim),"Length");ImGui::SameLine();
    float duration=timeline.duration;
    ImGui::SetNextItemWidth(fieldWidth);
    if(ImGui::DragFloat("##length",&duration,0.05f,0,3600,duration>0?"%.2f s":"auto",ImGuiSliderFlags_AlwaysClamp)) {auto next=timeline;next.duration=duration;live={next,"Set timeline length"};}
    probe::item("timeline/length");
    ImGui::SetItemTooltip("0 = until the last key or event");
    ImGui::SameLine();
    bool loop=timeline.loop;
    if(ImGui::Checkbox("Loop",&loop)) {auto next=timeline;next.loop=loop;commit={next,loop?"Loop timeline":"Stop looping timeline"};}
    ImGui::EndDisabled();
    // Rows.
    std::vector<Row> rows;
    if(!timeline.camera.position.empty()) {Row r;r.label=std::string(icon::Camera)+"  Camera position";r.probe="camera/position";curveRow(r,cameraVector,0,timeline.camera.position);r.remove=[](Timeline& t){t.camera.position.keys.clear();};rows.push_back(r);}
    if(!timeline.camera.target.empty()) {Row r;r.label=std::string(icon::Camera)+"  Camera target";r.probe="camera/target";curveRow(r,cameraVector,1,timeline.camera.target);r.remove=[](Timeline& t){t.camera.target.keys.clear();};rows.push_back(r);}
    if(!timeline.camera.fov.empty()) {Row r;r.label=std::string(icon::Camera)+"  Camera fov";r.probe="camera/fov";curveRow(r,cameraFov,0,timeline.camera.fov);r.remove=[](Timeline& t){t.camera.fov.keys.clear();};rows.push_back(r);}
    for(size_t i=0;i<timeline.tracks.size();++i) {
        const auto& track=timeline.tracks[i];
        Row r;
        const char* glyph=track.kind==TrackTarget::Material?icon::Palette:track.kind==TrackTarget::Environment?icon::Sun:icon::Box;
        std::string target=track.kind==TrackTarget::Environment?"environment":track.target;
        if(track.kind==TrackTarget::Entity) if(const auto* e=scene.get(scene.find(track.target));e && !e->name.empty()) target=e->name;
        r.label=std::string(glyph)+"  "+target+"  \xc2\xb7  "+propertyName(track.property);
        r.probe=track.target+"/"+propertyName(track.property);
        if(track.isVector()) curveRow(r,trackVector,i,track.vector); else curveRow(r,trackScalar,i,track.scalar);
        r.remove=[i](Timeline& t){if(i<t.tracks.size()) t.tracks.erase(t.tracks.begin()+long(i));};
        rows.push_back(r);
    }
    if(!timeline.events.empty()) {
        Row r;r.label=std::string(icon::Zap)+"  Events";r.probe="events";r.events=true;
        for(const auto& event:timeline.events) r.times.push_back(event.time);
        r.move=[](Timeline& t,size_t i,float time){
            if(i>=t.events.size()) return;
            t.events[i].time=std::max(0.0f,time);
            std::stable_sort(t.events.begin(),t.events.end(),[](const auto& a,const auto& b){return a.time<b.time;});
        };
        r.erase=[](Timeline& t,size_t i){if(i<t.events.size()) t.events.erase(t.events.begin()+long(i));};
        r.describe=[](const Timeline& t,size_t i){
            if(i>=t.events.size()) return std::string();
            const auto& e=t.events[i];
            return seconds(e.time)+"  "+e.effect+(e.entity.empty()?"":"  on "+e.entity);
        };
        r.remove=[](Timeline& t){t.events.clear();};
        rows.push_back(r);
    }
    ImGui::Spacing();
    auto origin=ImGui::GetCursorScreenPos();
    auto available=ImGui::GetContentRegionAvail();
    float labelWidth=std::min(220*lastScale,available.x*0.35f);
    float rowHeight=ImGui::GetFrameHeight();
    float rulerHeight=ImGui::GetTextLineHeight()+6;
    float areaX=origin.x+labelWidth,areaWidth=std::max(40.0f,available.x-labelWidth-6);
    float viewEnd=float(std::max({double(length),fxTime,1.0}))*1.05f;
    auto toX=[&](double t){return areaX+float(t/viewEnd)*areaWidth;};
    auto toTime=[&](float x){return std::clamp(double(x-areaX)/areaWidth*viewEnd,0.0,maxPreviewSeconds);};
    auto* draw=ImGui::GetWindowDrawList();
    ImGui::BeginChild("##timeline-rows",{0,0});
    draw=ImGui::GetWindowDrawList();
    origin=ImGui::GetCursorScreenPos();
    float bottom=origin.y+rulerHeight+rowHeight*float(rows.size());
    // Ruler ticks every 0.1/0.5/1/5 s depending on zoom.
    double step=viewEnd<=2?0.1:viewEnd<=8?0.5:viewEnd<=30?1:5;
    draw->AddRectFilled({areaX,origin.y},{areaX+areaWidth,origin.y+rulerHeight},theme::Surface,4.0f);
    for(double t=0;t<=viewEnd+1e-6;t+=step) {
        int index=int(std::lround(t/step));
        bool major=index%(step<1?(step<0.5?5:2):1)==0;
        float x=toX(t);
        draw->AddLine({x,origin.y+rulerHeight-(major?8.0f:4.0f)},{x,origin.y+rulerHeight},theme::TextDim);
        if(major) {char text[16];std::snprintf(text,sizeof text,"%g",t);draw->AddText({x+3,origin.y+1},theme::TextDim,text);}
        if(major && !rows.empty()) draw->AddLine({x,origin.y+rulerHeight},{x,bottom},IM_COL32(255,255,255,14));
    }
    if(length>0) draw->AddRectFilled({toX(length),origin.y},{areaX+areaWidth,bottom},IM_COL32(0,0,0,70));
    // Scrub/drag surface over the ruler and rows.
    ImGui::SetCursorScreenPos({areaX,origin.y});
    ImGui::BeginDisabled(bool(play));
    ImGui::InvisibleButton("##scrub",{areaWidth,std::max(rulerHeight+rowHeight*float(rows.size()),rulerHeight)},ImGuiButtonFlags_MouseButtonLeft|ImGuiButtonFlags_MouseButtonRight);
    probe::item("timeline/ruler");
    bool areaHovered=ImGui::IsItemHovered(),areaActive=ImGui::IsItemActive();
    ImGui::EndDisabled();
    auto mouse=ImGui::GetIO().MousePos;
    // Hit-test markers (diamonds and event triangles).
    static constexpr float markerRadius=6;
    std::optional<std::pair<size_t,size_t>> hoveredKey;
    for(size_t r=0;r<rows.size();++r) {
        float y=origin.y+rulerHeight+rowHeight*(float(r)+0.5f);
        for(size_t k=0;k<rows[r].times.size();++k)
            if(std::abs(mouse.x-toX(rows[r].times[k]))<=markerRadius*lastScale+1 && std::abs(mouse.y-y)<=rowHeight*0.5f) hoveredKey={r,k};
    }
    // Key dragging retimes from the committed timeline captured at the press.
    static std::optional<std::pair<size_t,size_t>> dragging;
    static Timeline dragBase;
    static float dragGrab=0;
    if(ImGui::IsItemActivated() && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) {
        if(hoveredKey && editable()) {dragging=hoveredKey;dragBase=timeline;dragGrab=rows[hoveredKey->first].times[hoveredKey->second];setPlayhead(dragGrab);fxPlaying=false;}
        else dragging.reset();
    }
    if(areaActive && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        if(dragging && dragging->first<rows.size()) {
            float time=float(toTime(mouse.x));
            if(!ImGui::GetIO().KeyShift) time=std::round(time*120)/120; // Snap to ticks (Shift: free).
            if(std::abs(time-dragGrab)>1e-6f) {
                auto next=dragBase;rows[dragging->first].move(next,dragging->second,time);dropEmpty(next);
                live={next,"Retime key"};
            }
            setPlayhead(time);
        } else if(!dragging) {setPlayhead(toTime(mouse.x));fxPlaying=false;}
    }
    if(!areaActive) dragging.reset();
    static std::optional<std::pair<size_t,size_t>> menuKey;
    if(areaHovered && hoveredKey && ImGui::IsMouseClicked(ImGuiMouseButton_Right) && editable()) {menuKey=hoveredKey;ImGui::OpenPopup("##key-context");}
    if(ImGui::BeginPopup("##key-context")) {
        if(menuKey && menuKey->first<rows.size()) {
            auto& row=rows[menuKey->first];
            ImGui::TextColored(toVec4(theme::TextDim),"%s",row.describe?row.describe(timeline,menuKey->second).c_str():"");
            ImGui::Separator();
            if(row.ease) {
                auto current=row.easeOf(timeline,menuKey->second);
                for(auto e:{Ease::Linear,Ease::Smooth,Ease::In,Ease::Out,Ease::Step})
                    if(ImGui::MenuItem((std::string("Ease ")+easeName(e)).c_str(),nullptr,current==e)) {auto next=timeline;row.ease(next,menuKey->second,e);commit={next,std::string("Ease ")+easeName(e)};}
                ImGui::Separator();
            }
            if(ImGui::MenuItem((std::string(icon::Trash)+(row.events?"  Delete event":"  Delete key")).c_str())) {
                auto next=timeline;row.erase(next,menuKey->second);dropEmpty(next);commit={next,row.events?"Delete event":"Delete key"};
            }
        }
        ImGui::EndPopup();
    }
    // Rows: label column, markers, delete buttons.
    for(size_t r=0;r<rows.size();++r) {
        const auto& row=rows[r];
        float top=origin.y+rulerHeight+rowHeight*float(r),y=top+rowHeight*0.5f;
        if(r%2) draw->AddRectFilled({origin.x,top},{areaX+areaWidth,top+rowHeight},IM_COL32(255,255,255,6));
        ImGui::SetCursorScreenPos({origin.x,top});
        ImGui::PushID(int(r));
        ImGui::BeginDisabled(!editable());
        if(ImGui::SmallButton(icon::Trash)) {auto next=timeline;row.remove(next);dropEmpty(next);commit={next,"Remove "+row.probe+" track"};}
        ImGui::SetItemTooltip("Remove this track");
        ImGui::EndDisabled();
        ImGui::SameLine();
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(row.label.c_str());
        probe::rect("timeline/row/"+row.probe,{origin.x,top},{areaX+areaWidth,top+rowHeight});
        ImGui::PopID();
        for(size_t k=0;k<row.times.size();++k) {
            float x=toX(row.times[k]),s=markerRadius*lastScale;
            bool hot=hoveredKey && hoveredKey->first==r && hoveredKey->second==k;
            ImU32 color=row.events?theme::Warning:hot?IM_COL32(255,255,255,255):IM_COL32(236,190,110,255);
            if(row.events) draw->AddTriangleFilled({x,y+s},{x-s,y-s},{x+s,y-s},color);
            else draw->AddQuadFilled({x,y-s},{x+s,y},{x,y+s},{x-s,y},color);
            if(hot && row.describe) ImGui::SetTooltip("%s\nDrag to retime \xc2\xb7 right-click for ease/delete",row.describe(timeline,k).c_str());
            probe::rect("timeline/key/"+row.probe+"/"+std::to_string(k),{x-s,y-s},{x+s,y+s});
        }
    }
    // Playhead.
    float x=toX(fxTime);
    draw->AddLine({x,origin.y},{x,std::max(bottom,origin.y+rulerHeight)},theme::Accent,2);
    draw->AddTriangleFilled({x-5,origin.y},{x+5,origin.y},{x,origin.y+7},theme::Accent);
    if(rows.empty()) {
        ImGui::SetCursorScreenPos({origin.x,origin.y+rulerHeight+6});
        ImGui::PushStyleColor(ImGuiCol_Text,toVec4(theme::TextDim));
        ImGui::TextWrapped("%s  No animation yet. Select an entity, move the playhead, and press Key (or enable Auto Key and use the gizmo). Add effect events with Event; Key > Viewport camera records a shot camera.",icon::Info);
        ImGui::PopStyleColor();
    }
    ImGui::SetCursorScreenPos({origin.x,std::max(bottom,origin.y+rulerHeight)+4});
    ImGui::Dummy({1,1});
    ImGui::EndChild();
    ImGui::End();
    if(commit) attempt([&]{document.apply(SetTimeline{commit->first},commit->second);});
    else if(live) preview(SetTimeline{live->first},live->second);
}
}
