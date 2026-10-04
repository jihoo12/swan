#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <cstdio>
namespace swan {
namespace {
// Two-column property table: dim label on the left, full-width editor on the right.
bool beginProperties(const char* id) {
    if(!ImGui::BeginTable(id,2,ImGuiTableFlags_SizingStretchProp)) return false;
    ImGui::TableSetupColumn("label",ImGuiTableColumnFlags_WidthFixed,ImGui::GetFontSize()*4.6f);
    ImGui::TableSetupColumn("value",ImGuiTableColumnFlags_WidthStretch);
    return true;
}
void property(const char* label) {
    ImGui::TableNextRow();ImGui::TableNextColumn();
    ImGui::AlignTextToFramePadding();ImGui::TextColored(toVec4(theme::TextDim),"%s",label);
    ImGui::TableNextColumn();ImGui::SetNextItemWidth(-FLT_MIN);
}
// X/Y/Z drags with colored axis tabs; clicking a tab resets that component.
bool vectorControl(const char* id,glm::vec3& value,float speed,float reset,const char* probePrefix,const char* format="%.2f",float minimum=0) {
    bool changed=false;
    ImGui::PushID(id);
    float tab=ImGui::GetFrameHeight()*0.62f,spacing=ImGui::GetStyle().ItemSpacing.x*0.5f;
    float field=(ImGui::GetContentRegionAvail().x-tab*3-spacing*2)/3;
    static constexpr const char* names[3]={"X","Y","Z"};
    const ImU32 colors[3]={theme::AxisX,theme::AxisY,theme::AxisZ};
    for(int i=0;i<3;++i) {
        ImGui::PushID(i);
        if(i) ImGui::SameLine(0,spacing);
        auto color=toVec4(colors[i]);
        ImGui::PushStyleColor(ImGuiCol_Button,ImVec4(color.x,color.y,color.z,0.75f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,color);ImGui::PushStyleColor(ImGuiCol_ButtonActive,color);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{0,ImGui::GetStyle().FramePadding.y});
        if(ImGui::Button(names[i],{tab,0})) {value[i]=reset;changed=true;}
        ImGui::SetItemTooltip("Reset %s to %g",names[i],reset);
        ImGui::PopStyleVar();ImGui::PopStyleColor(3);
        ImGui::SameLine(0,0);
        ImGui::SetNextItemWidth(field);
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4,ImGui::GetStyle().FramePadding.y});
        changed|=ImGui::DragFloat("##value",&value[i],speed,minimum,minimum?1e6f:0.0f,format,minimum?ImGuiSliderFlags_AlwaysClamp:0);
        ImGui::PopStyleVar();
        if(probePrefix) probe::item(std::string(probePrefix)+"/"+char('x'+i));
        ImGui::PopID();
    }
    ImGui::PopID();
    return changed;
}
bool sectionHeader(const char* glyph,const char* title,const char* id) {
    auto text=std::string(glyph)+"  "+title+"###"+id;
    ImGui::Spacing();
    // Quiet surface headers; the accent color is reserved for selection and focus.
    ImGui::PushStyleColor(ImGuiCol_Header,toVec4(IM_COL32(32,33,40,255)));
    ImGui::PushStyleColor(ImGuiCol_HeaderHovered,toVec4(IM_COL32(40,42,51,255)));
    ImGui::PushStyleColor(ImGuiCol_HeaderActive,toVec4(IM_COL32(46,48,58,255)));
    bool open=ImGui::CollapsingHeader(text.c_str(),ImGuiTreeNodeFlags_DefaultOpen);
    ImGui::PopStyleColor(3);
    return open;
}
}
void EditorLayer::drawInspector() {
    auto title=std::string(icon::Sliders)+"  Inspector###Inspector";
    if(!ImGui::Begin(title.c_str(),&showInspector)) {ImGui::End();return;}
    const auto* selected=selectedEntity();
    if(!selected) {
        auto region=ImGui::GetContentRegionAvail();
        const char* text="Select an entity to inspect it";
        ImGui::SetCursorPos({std::max(0.0f,(region.x-ImGui::CalcTextSize(text).x)*0.5f),region.y*0.4f});
        ImGui::TextColored(toVec4(theme::TextDim),"%s",text);
        ImGui::End();return;
    }
    // Copy: previews replace the document while widgets below are still drawing.
    Entity entity=*selected;
    const auto key=entity.key;
    const auto& scene=document.document().scene;
    auto sceneId=scene.find(key);
    auto parent=scene.parent(sceneId);
    std::string parentKey=parent?scene.get(*parent)->key:"";
    auto display=entity.name.empty()?key:entity.name;
    std::vector<std::function<void()>> deferred;
    auto properties=[&](const Entity& e){return SetEntityProperties{key,e.name,e.meshId,e.materialId,e.solid,e.collectible,e.goal};};
    ImGui::BeginDisabled(!editable());
    // Header: icon, inline name editor, and identity.
    {
        float iconSize=ImGui::GetFrameHeight()*1.6f;
        auto pos=ImGui::GetCursorScreenPos();
        ImGui::GetWindowDrawList()->AddRectFilled(pos,{pos.x+iconSize,pos.y+iconSize},theme::AccentSoft,8.0f);
        ImGui::PushFont(nullptr,ImGui::GetStyle().FontSizeBase*1.4f);
        auto glyph=entity.goal?icon::Flag:entity.collectible?icon::Gem:entity.meshId=="builtin:cube"?icon::Box:icon::Shapes;
        auto glyphSize=ImGui::CalcTextSize(glyph);
        ImGui::GetWindowDrawList()->AddText({pos.x+(iconSize-glyphSize.x)*0.5f,pos.y+(iconSize-glyphSize.y)*0.5f-2},theme::Accent,glyph);
        ImGui::PopFont();
        ImGui::Dummy({iconSize,iconSize});ImGui::SameLine();
        ImGui::BeginGroup();
        ImGuiID nameId=ImGui::GetID("##name");
        if(nameKey!=key || (ImGui::GetActiveID()!=nameId && entity.name!=nameBuffer.data())) {
            std::snprintf(nameBuffer.data(),nameBuffer.size(),"%s",entity.name.c_str());nameKey=key;
        }
        if(focusName) {ImGui::SetKeyboardFocusHere();focusName=false;}
        ImGui::SetNextItemWidth(-FLT_MIN);
        if(ImGui::InputTextWithHint("##name","Name",nameBuffer.data(),nameBuffer.size(),ImGuiInputTextFlags_AutoSelectAll)) {
            auto renamed=entity;renamed.name=nameBuffer.data();
            preview(properties(renamed),"Rename "+display);
        }
        probe::item("inspector/name");
        ImGui::TextColored(toVec4(theme::TextDim),"%s  %s",icon::Command,key.c_str());
        ImGui::SetItemTooltip("Stable entity ID (used in scene files)");
        ImGui::EndGroup();
    }
    // Transform.
    if(sectionHeader(icon::Move,"Transform","transform") && beginProperties("##transform")) {
        auto t=entity.transform;
        property("Position");
        if(vectorControl("position",t.position,0.05f,0,"inspector/position")) preview(SetTransform{key,t},"Move "+display);
        property("Rotation");
        float degrees=glm::degrees(t.yaw);
        if(ImGui::DragFloat("##yaw",&degrees,0.5f,0,0,"Y  %.1f\xc2\xb0")) {t.yaw=glm::radians(degrees);preview(SetTransform{key,t},"Rotate "+display);}
        probe::item("inspector/yaw");
        property("Scale");
        if(vectorControl("scale",t.scale,0.01f,1,"inspector/scale","%.3f",0.001f)) preview(SetTransform{key,t},"Scale "+display);
        ImGui::EndTable();
        if(parent) ImGui::TextColored(toVec4(theme::TextDim),"%s  Relative to parent %s",icon::Info,parentKey.c_str());
    }
    // Rendering.
    if(sectionHeader(icon::Box,"Rendering","rendering") && beginProperties("##rendering")) {
        property("Mesh");
        // Choices are applied after each loop: a preview replaces the maps being iterated.
        std::optional<std::string> chosenMesh,chosenMaterial;
        if(ImGui::BeginCombo("##mesh",entity.meshId.c_str())) {
            for(const auto& [id,asset]:scene.meshes().entries()) {
                (void)asset;
                if(ImGui::Selectable(id.c_str(),id==entity.meshId)) chosenMesh=id;
            }
            ImGui::EndCombo();
        }
        if(chosenMesh) {auto changed=entity;changed.meshId=*chosenMesh;preview(properties(changed),"Change mesh of "+display);}
        property("Material");
        float unique=ImGui::CalcTextSize(icon::Copy).x+ImGui::GetStyle().FramePadding.x*2;
        ImGui::SetNextItemWidth(-unique-ImGui::GetStyle().ItemSpacing.x);
        if(ImGui::BeginCombo("##material",entity.materialId.c_str())) {
            for(const auto& [id,material]:scene.assets().entries()) {
                auto color=ImVec4(material.color.x,material.color.y,material.color.z,1);
                ImGui::ColorButton(("##swatch"+id).c_str(),color,ImGuiColorEditFlags_NoTooltip|ImGuiColorEditFlags_NoBorder,{ImGui::GetTextLineHeight(),ImGui::GetTextLineHeight()});
                ImGui::SameLine();
                if(ImGui::Selectable(id.c_str(),id==entity.materialId)) chosenMaterial=id;
            }
            ImGui::EndCombo();
        }
        if(chosenMaterial) {auto changed=entity;changed.materialId=*chosenMaterial;preview(properties(changed),"Assign "+*chosenMaterial+" to "+display);}
        if(ImGui::BeginDragDropTarget()) {
            if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_MATERIAL")) {auto id=std::string(static_cast<const char*>(payload->Data));deferred.push_back([this,key,id]{assignMaterial(key,id);});}
            ImGui::EndDragDropTarget();
        }
        ImGui::SameLine();
        if(ImGui::Button(icon::Copy)) deferred.push_back([this]{makeMaterialUnique();});
        ImGui::SetItemTooltip("Make unique: copy this material for this entity only");
        ImGui::EndTable();
    }
    // Gameplay flags.
    if(sectionHeader(icon::Gamepad,"Gameplay","gameplay") && beginProperties("##gameplay")) {
        auto flag=[&](const char* label,bool Entity::*member,const char* help,bool disabled) {
            property(label);
            auto changed=entity;
            ImGui::BeginDisabled(disabled);
            if(ImGui::Checkbox((std::string("##")+label).c_str(),&(changed.*member))) preview(properties(changed),std::string(changed.*member?"Enable ":"Disable ")+label+" on "+display);
            ImGui::EndDisabled();
            ImGui::SameLine();ImGui::TextColored(toVec4(theme::TextDim),"%s",help);
        };
        flag("Solid",&Entity::solid,"Blocks the player",bool(entity.animation));
        flag("Collectible",&Entity::collectible,"Picked up with E",entity.goal);
        flag("Goal",&Entity::goal,"Needs every pickup",entity.collectible);
        ImGui::EndTable();
    }
    // Hierarchy.
    if(sectionHeader(icon::Tree,"Hierarchy","hierarchy") && beginProperties("##parenting")) {
        property("Parent");
        if(ImGui::BeginCombo("##parent",parentKey.empty()?"None":parentKey.c_str())) {
            if(ImGui::Selectable("None",parentKey.empty()) && !parentKey.empty()) deferred.push_back([this,key]{reparent(key,std::nullopt);});
            for(auto id:scene.entities()) {
                const auto& candidate=*scene.get(id);
                bool descendant=false;
                for(auto up=std::optional<EntityId>(id);up;up=scene.parent(*up)) if(scene.get(*up)->key==key) {descendant=true;break;}
                if(descendant) continue;
                auto text=(candidate.name.empty()?candidate.key:candidate.name)+"##"+candidate.key;
                if(ImGui::Selectable(text.c_str(),candidate.key==parentKey) && candidate.key!=parentKey) {
                    auto target=candidate.key;deferred.push_back([this,key,target]{reparent(key,target);});
                }
            }
            ImGui::EndCombo();
        }
        ImGui::EndTable();
    }
    // Shared material.
    if(scene.assets().contains(entity.materialId)) {
        auto id=entity.materialId;
        auto material=scene.assets().get(id);
        size_t users=0;
        for(auto other:scene.entities()) users+=scene.get(other)->materialId==id;
        auto heading="Material  \xc2\xb7  "+id;
        if(sectionHeader(icon::Palette,heading.c_str(),"material")) {
            if(users>1) {
                ImGui::TextColored(toVec4(theme::Warning),"%s  Shared by %zu entities",icon::Warning,users);
                ImGui::SameLine();
                if(ImGui::SmallButton("Make unique")) deferred.push_back([this]{makeMaterialUnique();});
            }
            if(beginProperties("##material-props")) {
                auto edited=material;bool changed=false;
                property("Color");changed|=ImGui::ColorEdit3("##color",&edited.color.x,ImGuiColorEditFlags_Float|ImGuiColorEditFlags_DisplayHex);
                property("Emission");changed|=ImGui::DragFloat("##emission",&edited.emission,0.02f,0,100,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                property("UV scale");changed|=ImGui::DragFloat2("##uv",&edited.uvScale.x,0.02f,0.01f,1000,"%.2f",ImGuiSliderFlags_AlwaysClamp);
                property("Texture");
                if(ImGui::BeginCombo("##texture",edited.textureId.c_str())) {
                    for(const auto& [texture,value]:scene.textures().entries()) {
                        (void)value;
                        if(ImGui::Selectable((std::string(icon::Image)+"  "+texture).c_str(),texture==edited.textureId)) {edited.textureId=texture;changed=true;}
                    }
                    ImGui::EndCombo();
                }
                if(changed) preview(SetMaterial{id,edited},"Edit material "+id);
                ImGui::EndTable();
            }
        }
    }
    ImGui::EndDisabled();
    if(play) {ImGui::Spacing();ImGui::TextColored(toVec4(theme::TextDim),"%s  Stop play to edit authored data",icon::Info);}
    ImGui::End();
    for(auto& action:deferred) action();
}
}
