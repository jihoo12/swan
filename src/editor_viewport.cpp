#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "editor_view.hpp"
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/gtc/type_ptr.hpp>
#include <cmath>
#include <cstdio>
namespace swan {
namespace {
constexpr float lookSensitivity=0.0032f;
glm::mat4 viewMatrix(const Camera& camera) {
    return glm::lookAt(camera.position,camera.position+forward(camera.yaw,camera.pitch),glm::vec3(0,1,0));
}
// ImGuizmo expects a conventional (Y-up clip space) projection, unlike the Vulkan-flipped one.
glm::mat4 gizmoProjection(const Camera& camera,float aspect) {
    return glm::perspective(glm::radians(camera.fov),aspect,camera.nearPlane,camera.farPlane);
}
void drawBounds(ImDrawList* draw,const std::array<glm::vec3,8>& corners,const glm::mat4& viewProjection,ImVec2 origin,ImVec2 size,ImU32 color,float thickness) {
    std::array<ImVec2,8> screen;
    for(int i=0;i<8;++i) {
        auto clip=viewProjection*glm::vec4(corners[i],1);
        if(clip.w<=0.05f) return; // Skip outlines crossing the camera plane rather than drawing garbage.
        auto ndc=glm::vec2(clip)/clip.w;
        screen[i]={origin.x+(ndc.x+1)*0.5f*size.x,origin.y+(ndc.y+1)*0.5f*size.y};
    }
    static constexpr int edges[12][2]={{0,1},{2,3},{4,5},{6,7},{0,2},{1,3},{4,6},{5,7},{0,4},{1,5},{2,6},{3,7}};
    for(const auto& edge:edges) draw->AddLine(screen[edge[0]],screen[edge[1]],color,thickness);
}
void overlayChip(ImDrawList* draw,ImVec2 position,const std::string& text,ImU32 textColor=IM_COL32(220,222,230,255)) {
    auto size=ImGui::CalcTextSize(text.c_str());
    draw->AddRectFilled({position.x-8,position.y-4},{position.x+size.x+8,position.y+size.y+4},IM_COL32(16,17,21,190),6.0f);
    draw->AddText(position,textColor,text.c_str());
}
}
void EditorLayer::drawViewport(const GuiFrame& frame) {
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{0,0});
    auto title=std::string(icon::Camera)+"  Viewport###Viewport";
    bool visible=ImGui::Begin(title.c_str(),nullptr,ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoScrollWithMouse|ImGuiWindowFlags_NoCollapse);
    ImGui::PopStyleVar();
    if(!visible) {ImGui::End();navigation=Navigation::None;gizmoActive=false;return;}
    auto origin=ImGui::GetCursorScreenPos();
    auto size=ImGui::GetContentRegionAvail();
    size={std::max(size.x,16.0f),std::max(size.y,16.0f)};
    auto scale=ImGui::GetIO().DisplayFramebufferScale;
    viewportSize={unsigned(size.x*scale.x),unsigned(size.y*scale.y)};
    if(frame.sceneTexture) ImGui::Image(ImTextureRef(static_cast<ImTextureID>(frame.sceneTexture)),size);
    else ImGui::Dummy(size);
    probe::rect("viewport",origin,{origin.x+size.x,origin.y+size.y});
    bool hovered=ImGui::IsItemHovered();
    auto mouse=ImGui::GetIO().MousePos;
    glm::vec2 local{mouse.x-origin.x,mouse.y-origin.y};
    // Asset drops: meshes become entities on the ground, materials land on the entity under the cursor.
    if(!play && ImGui::BeginDragDropTarget()) {
        const auto& scene=document.document().scene;
        if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_MESH")) {
            std::string meshId(static_cast<const char*>(payload->Data));
            std::optional<glm::vec3> position;
            if(auto point=groundPoint(camera.camera(),local,{size.x,size.y})) {
                position=*point;
                if(scene.meshes().contains(meshId)) position->y-=scene.meshes().get(meshId).data->minimum.y;
            }
            createEntity(meshId,position);
        }
        const auto* hoverMaterial=ImGui::AcceptDragDropPayload("SWAN_MATERIAL",ImGuiDragDropFlags_AcceptBeforeDelivery|ImGuiDragDropFlags_AcceptNoDrawDefaultRect);
        if(hoverMaterial) {
            auto key=pickEntity(scene,camera.camera(),local,{size.x,size.y});
            if(!key.empty()) {
                auto vp=camera.camera().viewProjection(size.x/size.y);
                drawBounds(ImGui::GetWindowDrawList(),worldBounds(scene,scene.find(key)),vp,origin,size,theme::Warning,2);
                if(hoverMaterial->IsDelivery()) assignMaterial(key,static_cast<const char*>(hoverMaterial->Data));
            }
        }
        ImGui::EndDragDropTarget();
    }
    if(play) {
        if(hovered && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) playCaptured=true;
        navigation=Navigation::None;gizmoActive=false;
    } else {
        navigateViewport(hovered,origin,size,frame.deltaTime);
        drawGizmo(origin,size);
    }
    drawViewportOverlay(origin,size,frame);
    ImGui::End();
}
void EditorLayer::navigateViewport(bool hovered,ImVec2 origin,ImVec2 size,float dt) {
    auto& io=ImGui::GetIO();
    bool gizmoBusy=ImGuizmo::IsUsingAny() || ImGuizmo::IsUsingViewManipulate();
    if(navigation==Navigation::None && hovered && !gizmoBusy) {
        if(ImGui::IsMouseClicked(ImGuiMouseButton_Right)) navigation=Navigation::Fly;
        else if(ImGui::IsMouseClicked(ImGuiMouseButton_Middle)) navigation=Navigation::Pan;
        else if(io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left)) navigation=Navigation::Orbit;
    }
    if((navigation==Navigation::Fly && !ImGui::IsMouseDown(ImGuiMouseButton_Right))
        || (navigation==Navigation::Pan && !ImGui::IsMouseDown(ImGuiMouseButton_Middle))
        || (navigation==Navigation::Orbit && !ImGui::IsMouseDown(ImGuiMouseButton_Left))) navigation=Navigation::None;
    auto delta=io.MouseDelta;
    if(!std::isfinite(delta.x) || !std::isfinite(delta.y) || std::abs(delta.x)>2000 || std::abs(delta.y)>2000) delta={0,0};
    switch(navigation) {
    case Navigation::Fly: {
        camera.look({delta.x*lookSensitivity,-delta.y*lookSensitivity});
        auto key=[](ImGuiKey k){return ImGui::IsKeyDown(k)?1.0f:0.0f;};
        glm::vec3 move{key(ImGuiKey_D)-key(ImGuiKey_A),key(ImGuiKey_E)+key(ImGuiKey_Space)-key(ImGuiKey_Q)-key(ImGuiKey_LeftCtrl),key(ImGuiKey_W)-key(ImGuiKey_S)};
        camera.fly(move,io.KeyShift,dt);
        if(io.MouseWheel!=0) {camera.adjustSpeed(io.MouseWheel);settings.cameraSpeed=camera.speed();}
        break;
    }
    case Navigation::Orbit: camera.orbit({delta.x*lookSensitivity,-delta.y*lookSensitivity});break;
    case Navigation::Pan: camera.pan({delta.x,delta.y},size.y);break;
    case Navigation::None: if(hovered && io.MouseWheel!=0) camera.dolly(io.MouseWheel);break;
    }
    // Click-to-select fires on release, so gizmo drags and small slips never change selection.
    if(navigation==Navigation::None && hovered && !io.KeyAlt && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !ImGui::IsAnyItemHovered()) {
        selectArmed=true;pressPosition={io.MousePos.x,io.MousePos.y};
        pressOverGizmo=ImGuizmo::IsOver() || ImGuizmo::IsViewManipulateHovered() || gizmoBusy;
    }
    if(selectArmed && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
        selectArmed=false;
        bool still=glm::length(glm::vec2(io.MousePos.x,io.MousePos.y)-pressPosition)<5;
        if(still && !pressOverGizmo && !gizmoActive) {
            auto key=pickEntity(document.document().scene,camera.camera(),{io.MousePos.x-origin.x,io.MousePos.y-origin.y},{size.x,size.y});
            if(attempt([&]{document.select(key);}) && !key.empty()) scrollToKey=key;
        }
    }
}
void EditorLayer::drawGizmo(ImVec2 origin,ImVec2 size) {
    const auto& cam=camera.camera();
    auto view=viewMatrix(cam);
    auto projection=gizmoProjection(cam,size.x/size.y);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(origin.x,origin.y,size.x,size.y);
    ImGuizmo::SetOrthographic(false);
    if(settings.showGrid) {glm::mat4 identity(1);ImGuizmo::DrawGrid(glm::value_ptr(view),glm::value_ptr(projection),glm::value_ptr(identity),60);}
    // View cube: drag or click a face to orient the camera around its pivot.
    float cube=96*lastScale;
    auto before=view;
    ImGuizmo::ViewManipulate(glm::value_ptr(view),camera.pivotDistance(),{origin.x+size.x-cube-8,origin.y+8},{cube,cube},0x00000000);
    if(view!=before) {
        auto inverse=glm::inverse(view);auto ahead=-glm::vec3(inverse[2]);
        camera.place(glm::vec3(inverse[3]),std::atan2(ahead.z,ahead.x),std::asin(std::clamp(ahead.y,-1.0f,1.0f)));
        view=viewMatrix(camera.camera());
    }
    const auto* entity=selectedEntity();
    if(gizmo==Gizmo::Select || !entity) {gizmoActive=false;return;}
    const auto& scene=document.document().scene;
    auto id=scene.find(entity->key);
    auto world=scene.worldTransform(id);
    auto model=transformMatrix(world);
    bool hasChildren=false;
    for(auto other:scene.entities()) if(scene.parent(other)==id) {hasChildren=true;break;}
    auto operation=gizmo==Gizmo::Translate?ImGuizmo::TRANSLATE:gizmo==Gizmo::Rotate?ImGuizmo::ROTATE_Y:ImGuizmo::SCALE;
    auto mode=gizmoLocal || gizmo==Gizmo::Scale?ImGuizmo::LOCAL:ImGuizmo::WORLD;
    bool snapping=settings.snap!=ImGui::GetIO().KeyCtrl;
    float snap[3];
    float increment=gizmo==Gizmo::Translate?settings.snapTranslate:gizmo==Gizmo::Rotate?settings.snapRotate:settings.snapScale;
    snap[0]=snap[1]=snap[2]=increment;
    bool changed=ImGuizmo::Manipulate(glm::value_ptr(view),glm::value_ptr(projection),operation,mode,glm::value_ptr(model),nullptr,snapping?snap:nullptr);
    bool using_=ImGuizmo::IsUsing();
    if(using_ && !gizmoActive) gizmoFailed=false;
    gizmoActive=using_;
    probe::value("gizmo",using_?"active":"idle");
    if(!changed) return;
    auto next=transformFromMatrix(model);
    auto result=world;
    const char* verb="Move ";
    if(gizmo==Gizmo::Translate) result.position=next.position;
    else if(gizmo==Gizmo::Rotate) {result.yaw=next.yaw;verb="Rotate ";}
    else {
        verb="Scale ";
        if(hasChildren) {
            // Parents need uniform scale: apply the most-changed axis to all three.
            auto ratio=next.scale/world.scale;int axis=0;
            for(int i=1;i<3;++i) if(std::abs(ratio[i]-1)>std::abs(ratio[axis]-1)) axis=i;
            result.scale=world.scale*ratio[axis];
        } else result.scale=next.scale;
    }
    auto local=entity->transform;
    auto parent=scene.parent(id);
    auto converted=parent?relativeTransform(scene.worldTransform(*parent),result):result;
    // Copy only the manipulated component so untouched values stay bit-exact.
    if(gizmo==Gizmo::Translate) local.position=converted.position;
    else if(gizmo==Gizmo::Rotate) local.yaw=converted.yaw;
    else local.scale=converted.scale;
    try {document.showPreview(SetTransform{entity->key,local},verb+(entity->name.empty()?entity->key:entity->name));}
    catch(const std::exception& error) {if(!gizmoFailed) notify(Level::Warning,error.what());gizmoFailed=true;}
}
void EditorLayer::drawViewportOverlay(ImVec2 origin,ImVec2 size,const GuiFrame& frame) {
    auto* draw=ImGui::GetWindowDrawList();
    ImVec2 max{origin.x+size.x,origin.y+size.y};
    draw->PushClipRect(origin,max,true);
    const auto& scene=document.document().scene;
    if(!play) {
        // Selection outline (drawn beneath the gizmo, which renders in its own pass).
        auto id=scene.find(document.selection());
        if(scene.get(id)) drawBounds(draw,worldBounds(scene,id),camera.camera().viewProjection(size.x/size.y),origin,size,theme::Accent,1.5f);
        if(scene.get(id)) {
            auto clip=camera.camera().viewProjection(size.x/size.y)*glm::vec4(scene.worldTransform(id).position,1);
            if(clip.w>0) {auto ndc=glm::vec2(clip)/clip.w;ImVec2 p{origin.x+(ndc.x+1)*0.5f*size.x,origin.y+(ndc.y+1)*0.5f*size.y};probe::rect("viewport/selection",p,p);}
        }
    }
    float pad=12*lastScale;
    // Top-left: mode and navigation hints.
    std::string info;
    if(play) info=playCaptured?std::string(icon::Gamepad)+"  Playing  \xc2\xb7  Esc releases the mouse":std::string(icon::Gamepad)+"  Click the viewport to control  \xc2\xb7  F5 stops";
    else if(navigation==Navigation::Fly) {char text[96];std::snprintf(text,sizeof text,"%s  Flying  \xc2\xb7  %.1f m/s  \xc2\xb7  wheel changes speed",icon::Camera,camera.speed());info=text;}
    else if(navigation==Navigation::Orbit) info=std::string(icon::Rotate)+"  Orbit";
    else if(navigation==Navigation::Pan) info=std::string(icon::Move)+"  Pan";
    else if(gizmo!=Gizmo::Select) info=std::string(gizmo==Gizmo::Translate?icon::Move:gizmo==Gizmo::Rotate?icon::Rotate:icon::Scale)+(gizmo==Gizmo::Translate?"  Move":gizmo==Gizmo::Rotate?"  Rotate":"  Scale")+(gizmoLocal?"  \xc2\xb7  Local":"  \xc2\xb7  World")+(settings.snap?"  \xc2\xb7  Snap":"");
    if(!info.empty()) overlayChip(draw,{origin.x+pad+6,origin.y+pad},info,play?theme::Play:IM_COL32(220,222,230,255));
    // Bottom-left: statistics.
    if(settings.showStats) {
        char text[128];
        std::snprintf(text,sizeof text,"%.0f FPS   %llu drawn   %llu culled   %ux%u",frame.fps,(unsigned long long)frame.visibleObjects,(unsigned long long)frame.culledObjects,viewportSize.x,viewportSize.y);
        overlayChip(draw,{origin.x+pad+6,max.y-pad-ImGui::GetTextLineHeight()},text,theme::TextDim);
    }
    draw->PopClipRect();
    // Empty-scene welcome card with real buttons.
    if(!play && scene.size()==0) {
        // Size the card to its widest row (the three buttons).
        auto& style=ImGui::GetStyle();
        float buttons=ImGui::CalcTextSize((std::string(icon::Box)+"  Add Cube"+icon::FolderOpen+"  Open Scene..."+icon::Command+"  Commands").c_str()).x+style.FramePadding.x*6+style.ItemSpacing.x*2;
        float width=std::max(360*lastScale,buttons+40);
        ImVec2 cardMin{origin.x+(size.x-width)*0.5f,origin.y+size.y*0.28f};
        auto* card=ImGui::GetWindowDrawList();
        card->ChannelsSplit(2);card->ChannelsSetCurrent(1);
        ImGui::SetCursorScreenPos({cardMin.x+20,cardMin.y+18});
        ImGui::BeginGroup();
        ImGui::PushFont(nullptr,ImGui::GetStyle().FontSizeBase*1.35f);
        ImGui::TextColored(toVec4(theme::Accent),"%s",icon::Sparkles);ImGui::SameLine();ImGui::TextUnformatted("Empty scene");
        ImGui::PopFont();
        ImGui::TextColored(toVec4(theme::TextDim),"Add something to get started, or open a scene.");
        ImGui::Spacing();
        if(ImGui::Button((std::string(icon::Box)+"  Add Cube").c_str())) createEntity("builtin:cube");
        probe::item("viewport/add-cube");
        ImGui::SameLine();
        if(ImGui::Button((std::string(icon::FolderOpen)+"  Open Scene...").c_str())) actions.run("file.open");
        ImGui::SameLine();
        if(ImGui::Button((std::string(icon::Command)+"  Commands").c_str())) palette.open();
        if(!settings.recentScenes.empty()) {
            ImGui::Spacing();ImGui::TextColored(toVec4(theme::TextDim),"Recent");
            for(size_t i=0;i<std::min<size_t>(settings.recentScenes.size(),4);++i) {
                auto path=settings.recentScenes[i];
                ImGui::PushID(int(i));
                if(ImGui::Selectable((std::string(icon::File)+"  "+path.filename().string()).c_str(),false,0,{width-40,0})) guardUnsaved([this,path]{openScene(path);});
                ImGui::SetItemTooltip("%s",path.string().c_str());
                ImGui::PopID();
            }
        }
        ImGui::EndGroup();
        ImVec2 cardMax{cardMin.x+width,ImGui::GetItemRectMax().y+18};
        card->ChannelsSetCurrent(0);
        card->AddRectFilled(cardMin,cardMax,IM_COL32(22,23,28,236),10.0f);
        card->AddRect(cardMin,cardMax,IM_COL32(124,140,255,70),10.0f,1.0f);
        card->ChannelsMerge();
    }
}
}
