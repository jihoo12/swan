#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "fuzzy.hpp"
#include <imgui.h>
#include <set>
#include <unordered_map>
namespace swan {
namespace {
const char* entityIcon(const Entity& entity) {
    if(entity.goal) return icon::Flag;
    if(entity.collectible) return icon::Gem;
    return entity.meshId=="builtin:cube"?icon::Box:icon::Shapes;
}
ImU32 entityColor(const Entity& entity) {
    if(entity.goal) return theme::Warning;
    if(entity.collectible) return IM_COL32(120,214,226,255);
    return theme::TextDim;
}
std::string label(const Entity& entity) {return entity.name.empty()?entity.key:entity.name;}
}
void EditorLayer::drawHierarchy() {
    auto title=std::string(icon::Tree)+"  Hierarchy###Hierarchy";
    if(!ImGui::Begin(title.c_str(),&showHierarchy)) {ImGui::End();return;}
    const auto& scene=document.document().scene;
    // Header: filter and quick add.
    float button=ImGui::GetFrameHeight();
    ImGui::SetNextItemWidth(-button-ImGui::GetStyle().ItemSpacing.x);
    auto hint=std::string(icon::Search)+"  Filter";
    ImGui::InputTextWithHint("##filter",hint.c_str(),hierarchyFilter.data(),hierarchyFilter.size());
    ImGui::SameLine();
    ImGui::BeginDisabled(!editable());
    if(ImGui::Button(icon::Plus,{button,button})) ImGui::OpenPopup("##add-entity");
    probe::item("hierarchy/add");
    ImGui::SetItemTooltip("Add entity");
    ImGui::EndDisabled();
    if(ImGui::BeginPopup("##add-entity")) {
        ImGui::SeparatorText("Add");
        // Create after the loop: an edit replaces the scene, invalidating this map's iterators.
        std::optional<std::string> chosen;
        for(const auto& [id,mesh]:scene.meshes().entries()) {
            (void)mesh;
            auto text=std::string(id=="builtin:cube"?icon::Box:icon::Shapes)+"  "+(id=="builtin:cube"?"Cube":id);
            if(ImGui::MenuItem(text.c_str())) chosen=id;
            probe::item("hierarchy/add/"+id);
        }
        ImGui::EndPopup();
        if(chosen) createEntity(*chosen);
    }
    // Build the tree once per frame from stable keys.
    std::unordered_map<std::string,std::vector<EntityId>> children;
    std::vector<EntityId> roots;
    for(auto id:scene.entities()) {
        if(auto parent=scene.parent(id)) children[scene.get(*parent)->key].push_back(id);
        else roots.push_back(id);
    }
    std::set<std::string> reveal;
    if(!scrollToKey.empty()) {
        auto id=scene.find(scrollToKey);
        if(scene.get(id)) for(auto parent=scene.parent(id);parent;parent=scene.parent(*parent)) reveal.insert(scene.get(*parent)->key);
    }
    // Requests are applied after drawing; edits replace the scene that the loop is reading.
    std::optional<std::pair<std::string,std::optional<std::string>>> reparentRequest;
    std::optional<std::pair<std::string,std::string>> materialRequest,scriptRequest;
    std::optional<std::string> selectRequest,focusRequest,deleteRequest,duplicateRequest;
    auto dropTarget=[&](const std::optional<std::string>& key) {
        if(!ImGui::BeginDragDropTarget()) return;
        if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_ENTITY")) {
            std::string dragged(static_cast<const char*>(payload->Data));
            if(!key || dragged!=*key) reparentRequest={dragged,key};
        }
        if(key) if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_MATERIAL")) materialRequest={*key,static_cast<const char*>(payload->Data)};
        if(key) if(const auto* payload=ImGui::AcceptDragDropPayload("SWAN_SCRIPT")) scriptRequest={*key,static_cast<const char*>(payload->Data)};
        ImGui::EndDragDropTarget();
    };
    auto rowInteractions=[&](const Entity& entity) {
        const auto& key=entity.key;
        probe::item("hierarchy/"+key);
        if(ImGui::IsItemClicked(ImGuiMouseButton_Left) && !ImGui::IsItemToggledOpen()) selectRequest=key;
        if(ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) focusRequest=key;
        if(editable() && ImGui::BeginDragDropSource()) {
            ImGui::SetDragDropPayload("SWAN_ENTITY",key.c_str(),key.size()+1);
            ImGui::Text("%s  %s",entityIcon(entity),label(entity).c_str());
            ImGui::EndDragDropSource();
        }
        if(editable()) dropTarget(key);
        if(ImGui::BeginPopupContextItem("##context")) {
            selectRequest=key;
            ImGui::BeginDisabled(!editable());
            if(ImGui::MenuItem((std::string(icon::Focus)+"  Frame").c_str(),"F")) focusRequest=key;
            if(ImGui::MenuItem((std::string(icon::Pencil)+"  Rename").c_str(),"F2")) {showInspector=true;focusName=true;}
            if(ImGui::MenuItem((std::string(icon::Copy)+"  Duplicate").c_str(),"Ctrl+D")) duplicateRequest=key;
            if(scene.parent(scene.find(key)) && ImGui::MenuItem((std::string(icon::ArrowLeft)+"  Unparent").c_str())) reparentRequest={key,std::nullopt};
            ImGui::Separator();
            if(ImGui::MenuItem((std::string(icon::Trash)+"  Delete").c_str(),"Delete")) deleteRequest=key;
            ImGui::EndDisabled();
            ImGui::EndPopup();
        }
    };
    ImGui::BeginChild("##tree",{0,0},ImGuiChildFlags_None);
    std::string_view filter=hierarchyFilter.data();
    if(!filter.empty()) {
        // Filtering shows a flat, ranked list with the parent path for context.
        std::vector<std::pair<int,EntityId>> matches;
        for(auto id:scene.entities()) {
            const auto& entity=*scene.get(id);
            auto score=fuzzyScore(filter,label(entity));
            if(!score) score=fuzzyScore(filter,entity.key);
            if(score) matches.push_back({*score,id});
        }
        std::stable_sort(matches.begin(),matches.end(),[](const auto& a,const auto& b){return a.first>b.first;});
        for(const auto& [score,id]:matches) {
            (void)score;const auto& entity=*scene.get(id);
            ImGui::PushID(entity.key.c_str());
            auto text=std::string(entityIcon(entity))+"  "+label(entity);
            ImGui::Selectable(text.c_str(),document.selection()==entity.key);
            rowInteractions(entity);
            ImGui::PopID();
        }
        if(matches.empty()) ImGui::TextDisabled("No entities match \"%s\"",hierarchyFilter.data());
    } else {
        std::function<void(EntityId)> node=[&](EntityId id) {
            const auto& entity=*scene.get(id);
            const auto& kids=children[entity.key];
            ImGuiTreeNodeFlags flags=ImGuiTreeNodeFlags_OpenOnArrow|ImGuiTreeNodeFlags_SpanFullWidth|ImGuiTreeNodeFlags_DefaultOpen|ImGuiTreeNodeFlags_FramePadding;
            if(kids.empty()) flags|=ImGuiTreeNodeFlags_Leaf;
            if(document.selection()==entity.key) flags|=ImGuiTreeNodeFlags_Selected;
            if(reveal.contains(entity.key)) ImGui::SetNextItemOpen(true);
            ImGui::PushID(entity.key.c_str());
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding,{4,3});
            bool open=ImGui::TreeNodeEx("##node",flags,"%s","");
            ImGui::PopStyleVar();
            rowInteractions(entity);
            if(scrollToKey==entity.key) {ImGui::SetScrollHereY(0.4f);scrollToKey.clear();}
            ImGui::SameLine(0,2);
            ImGui::TextColored(toVec4(entityColor(entity)),"%s",entityIcon(entity));
            ImGui::SameLine(0,6);
            ImGui::TextUnformatted(label(entity).c_str());
            if(entity.animation) {ImGui::SameLine(0,6);ImGui::TextColored(toVec4(theme::TextDim),"%s",icon::Sparkles);ImGui::SetItemTooltip("Animated");}
            if(!entity.scriptId.empty()) {ImGui::SameLine(0,6);ImGui::TextColored(toVec4(IM_COL32(236,190,110,200)),"%s",icon::FileCode);ImGui::SetItemTooltip("Script: %s",entity.scriptId.c_str());}
            if(open) {for(auto kid:kids) node(kid);ImGui::TreePop();}
            ImGui::PopID();
        };
        for(auto id:roots) node(id);
        if(roots.empty()) {
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text,toVec4(theme::TextDim));
            ImGui::TextWrapped("No entities yet. Press Shift+A or drag a mesh from Assets into the viewport.");
            ImGui::PopStyleColor();
        }
    }
    // The remaining space deselects on click and unparents dropped entities.
    auto rest=ImGui::GetContentRegionAvail();
    ImGui::InvisibleButton("##background",{std::max(rest.x,1.0f),std::max(rest.y,ImGui::GetFrameHeight())});
    if(ImGui::IsItemClicked() && editable()) selectRequest=std::string();
    if(editable()) dropTarget(std::nullopt);
    ImGui::EndChild();
    ImGui::End();
    if(selectRequest) attempt([&]{document.select(*selectRequest);});
    if(focusRequest) {attempt([&]{document.select(*focusRequest);});focusSelection();}
    if(reparentRequest) reparent(reparentRequest->first,reparentRequest->second);
    if(materialRequest) assignMaterial(materialRequest->first,materialRequest->second);
    if(scriptRequest) assignScript(scriptRequest->first,scriptRequest->second);
    if(duplicateRequest) duplicateSelection();
    if(deleteRequest) deleteSelection();
}
}
