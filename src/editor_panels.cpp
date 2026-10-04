#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "fuzzy.hpp"
#include <imgui.h>
#include <cmath>
#include <cstdio>
namespace swan {
namespace {
std::string ellipsize(const std::string& text,float width) {
    if(ImGui::CalcTextSize(text.c_str()).x<=width) return text;
    std::string result=text;
    while(!result.empty() && ImGui::CalcTextSize((result+"...").c_str()).x>width) result.pop_back();
    return result+"...";
}
// Asset tile: an invisible button with custom drawing. Returns true when double-clicked.
bool assetTile(const std::string& id,const char* glyph,ImU32 glyphColor,float size,bool selected,const glm::vec3* swatch) {
    auto* draw=ImGui::GetWindowDrawList();
    auto pos=ImGui::GetCursorScreenPos();
    float labelHeight=ImGui::GetTextLineHeightWithSpacing();
    ImGui::InvisibleButton(id.c_str(),{size,size+labelHeight});
    bool hovered=ImGui::IsItemHovered(),active=ImGui::IsItemActive();
    ImU32 background=active?IM_COL32(50,52,64,255):hovered?IM_COL32(40,42,51,255):IM_COL32(31,32,39,255);
    draw->AddRectFilled(pos,{pos.x+size,pos.y+size},background,8.0f);
    if(selected) draw->AddRect(pos,{pos.x+size,pos.y+size},theme::Accent,8.0f,1.5f);
    ImVec2 center{pos.x+size*0.5f,pos.y+size*0.5f};
    if(swatch) {
        ImU32 color=ImGui::ColorConvertFloat4ToU32({std::min(swatch->x,1.0f),std::min(swatch->y,1.0f),std::min(swatch->z,1.0f),1});
        float radius=size*0.27f;
        draw->AddCircleFilled(center,radius,color,40);
        draw->AddCircleFilled({center.x-radius*0.35f,center.y-radius*0.35f},radius*0.28f,IM_COL32(255,255,255,60),24);
        draw->AddCircle(center,radius,IM_COL32(0,0,0,90),40,1.0f);
    } else {
        float glyphSize=size*0.42f;
        auto extent=ImGui::GetFont()->CalcTextSizeA(glyphSize,FLT_MAX,0,glyph);
        draw->AddText(nullptr,glyphSize,{center.x-extent.x*0.5f,center.y-extent.y*0.5f},glyphColor,glyph);
    }
    auto label=ellipsize(id,size-4);
    float width=ImGui::CalcTextSize(label.c_str()).x;
    draw->AddText({pos.x+(size-width)*0.5f,pos.y+size+3},hovered||selected?IM_COL32(230,231,236,255):theme::TextDim,label.c_str());
    return hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left);
}
}
void EditorLayer::drawAssets() {
    auto title=std::string(icon::Package)+"  Assets###Assets";
    if(!ImGui::Begin(title.c_str(),&showAssets)) {ImGui::End();return;}
    const auto& scene=document.document().scene;
    const auto* selected=selectedEntity();
    // Copies: actions below may replace the scene being listed.
    std::vector<std::pair<std::string,std::string>> meshes;
    for(const auto& [id,mesh]:scene.meshes().entries()) meshes.push_back({id,mesh.source.empty()?"Built-in":mesh.source.filename().string()+(mesh.part?" (part "+std::to_string(mesh.part)+")":"")});
    std::vector<std::pair<std::string,Material>> materials(scene.assets().entries().begin(),scene.assets().entries().end());
    std::vector<std::pair<std::string,std::string>> textures;
    for(const auto& [id,texture]:scene.textures().entries()) textures.push_back({id,texture.source.empty()?"Built-in":texture.source.filename().string()});
    auto usage=[&](auto predicate){size_t n=0;for(auto id:scene.entities()) n+=predicate(*scene.get(id));return n;};
    float filterWidth=200*lastScale;
    auto topRight=ImGui::GetCursorScreenPos();topRight.x+=ImGui::GetContentRegionAvail().x;
    std::function<void()> action;
    if(ImGui::BeginTabBar("##asset-tabs")) {
        auto tab=[&](const char* glyph,const char* name,size_t count) {
            auto text=std::string(glyph)+"  "+name+"  "+std::to_string(count)+"###"+name;
            bool open=ImGui::BeginTabItem(text.c_str());
            probe::item(std::string("assets/tab/")+name);
            if(open) assetTab=name;
            return open;
        };
        auto grid=[&](size_t count,const std::function<void(size_t)>& tile) {
            float size=84*lastScale,spacing=ImGui::GetStyle().ItemSpacing.x;
            int columns=std::max(1,int((ImGui::GetContentRegionAvail().x+spacing)/(size+spacing)));
            ImGui::BeginChild("##grid",{0,0});
            int column=0;
            for(size_t i=0;i<count;++i) {
                if(column) ImGui::SameLine();
                tile(i);
                column=(column+1)%columns;
            }
            ImGui::EndChild();
        };
        std::string_view filter=assetFilter.data();
        auto visible=[&](const std::string& id){return filter.empty() || fuzzyScore(filter,id).has_value();};
        if(tab(icon::Shapes,"Meshes",meshes.size())) {
            std::vector<size_t> shown;
            for(size_t i=0;i<meshes.size();++i) if(visible(meshes[i].first)) shown.push_back(i);
            grid(shown.size(),[&](size_t index){
                const auto& [id,source]=meshes[shown[index]];
                ImGui::PushID(id.c_str());
                bool open=assetTile(id,id=="builtin:cube"?icon::Box:icon::Shapes,IM_COL32(170,180,255,255),84*lastScale,selected && selected->meshId==id,nullptr);
                probe::item("assets/mesh/"+id);
                if(open && editable()) action=[this,id]{createEntity(id);};
                if(editable() && ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SWAN_MESH",id.c_str(),id.size()+1);
                    ImGui::Text("%s  Place %s",icon::Shapes,id.c_str());
                    ImGui::EndDragDropSource();
                }
                if(ImGui::BeginItemTooltip()) {
                    ImGui::TextUnformatted(id.c_str());
                    ImGui::TextColored(toVec4(theme::TextDim),"%s",source.c_str());
                    ImGui::TextColored(toVec4(theme::TextDim),"Used by %zu  \xc2\xb7  drag into the viewport or double-click to add",usage([&](const Entity& e){return e.meshId==id;}));
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            });
            ImGui::EndTabItem();
        }
        if(tab(icon::Palette,"Materials",materials.size())) {
            std::vector<size_t> shown;
            for(size_t i=0;i<materials.size();++i) if(visible(materials[i].first)) shown.push_back(i);
            grid(shown.size(),[&](size_t index){
                const auto& [id,material]=materials[shown[index]];
                ImGui::PushID(id.c_str());
                bool open=assetTile(id,icon::Palette,0,84*lastScale,selected && selected->materialId==id,&material.color);
                probe::item("assets/material/"+id);
                if(open && editable() && selected) action=[this,key=selected->key,id]{assignMaterial(key,id);};
                if(editable() && ImGui::BeginDragDropSource()) {
                    ImGui::SetDragDropPayload("SWAN_MATERIAL",id.c_str(),id.size()+1);
                    ImGui::ColorButton("##drag",{material.color.x,material.color.y,material.color.z,1},0,{ImGui::GetTextLineHeight(),ImGui::GetTextLineHeight()});
                    ImGui::SameLine();ImGui::Text("Assign %s",id.c_str());
                    ImGui::EndDragDropSource();
                }
                if(ImGui::BeginItemTooltip()) {
                    ImGui::TextUnformatted(id.c_str());
                    ImGui::TextColored(toVec4(theme::TextDim),"Texture %s  \xc2\xb7  emission %.2f",material.textureId.c_str(),material.emission);
                    ImGui::TextColored(toVec4(theme::TextDim),"Used by %zu  \xc2\xb7  drag onto an entity or double-click to assign",usage([&](const Entity& e){return e.materialId==id;}));
                    ImGui::EndTooltip();
                }
                ImGui::PopID();
            });
            ImGui::EndTabItem();
        }
        if(tab(icon::Image,"Textures",textures.size())) {
            std::vector<size_t> shown;
            for(size_t i=0;i<textures.size();++i) if(visible(textures[i].first)) shown.push_back(i);
            grid(shown.size(),[&](size_t index){
                const auto& [id,source]=textures[shown[index]];
                ImGui::PushID(id.c_str());
                assetTile(id,icon::Image,IM_COL32(140,210,190,255),84*lastScale,false,nullptr);
                probe::item("assets/texture/"+id);
                ImGui::SetItemTooltip("%s\n%s",id.c_str(),source.c_str());
                ImGui::PopID();
            });
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }
    // Overlay the filter at the top-right of the panel.
    ImGui::SetCursorScreenPos({topRight.x-filterWidth,topRight.y});
    ImGui::SetNextItemWidth(filterWidth);
    auto hint=std::string(icon::Search)+"  Filter assets";
    ImGui::InputTextWithHint("##asset-filter",hint.c_str(),assetFilter.data(),assetFilter.size());
    ImGui::End();
    if(action) action();
}
void EditorLayer::drawConsole() {
    auto title=std::string(icon::Terminal)+"  Console"+(unreadErrors?"  ("+std::to_string(unreadErrors)+")":"")+"###Console";
    if(!ImGui::Begin(title.c_str(),&showConsole)) {ImGui::End();return;}
    if(ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::IsWindowHovered()) unreadErrors=0;
    if(ImGui::Button((std::string(icon::Trash)+"  Clear").c_str())) {logs.clear();unreadErrors=0;}
    ImGui::SameLine();ImGui::TextColored(toVec4(theme::TextDim),"%zu messages",logs.size());
    ImGui::Separator();
    ImGui::BeginChild("##log",{0,0});
    if(ImGui::BeginTable("##entries",3,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("time",ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("level",ImGuiTableColumnFlags_WidthFixed);
        ImGui::TableSetupColumn("text",ImGuiTableColumnFlags_WidthStretch);
        for(const auto& entry:logs) {
            ImGui::TableNextRow();ImGui::TableNextColumn();
            int seconds=int(entry.time);
            ImGui::TextColored(toVec4(theme::TextDim),"%02d:%02d",seconds/60,seconds%60);
            ImGui::TableNextColumn();
            ImU32 color=entry.level==Level::Error?theme::Error:entry.level==Level::Warning?theme::Warning:entry.level==Level::Success?theme::Play:theme::TextDim;
            const char* glyph=entry.level==Level::Error?icon::Error:entry.level==Level::Warning?icon::Warning:entry.level==Level::Success?icon::Success:icon::Info;
            ImGui::TextColored(toVec4(color),"%s",glyph);
            ImGui::TableNextColumn();ImGui::TextWrapped("%s",entry.text.c_str());
        }
        ImGui::EndTable();
    }
    if(ImGui::GetScrollY()>=ImGui::GetScrollMaxY()-4) ImGui::SetScrollHereY(1);
    ImGui::EndChild();
    ImGui::End();
}
void EditorLayer::drawHistory() {
    auto title=std::string(icon::History)+"  History###History";
    if(!ImGui::Begin(title.c_str(),&showHistory)) {ImGui::End();return;}
    auto undo=document.undoHistory(),redo=document.redoHistory();
    int steps=0; // Negative: undo that many; positive: redo.
    ImGui::BeginDisabled(!editable());
    if(ImGui::Selectable((std::string(icon::File)+"  "+(sceneFile.empty()?"New scene":"Opened "+sceneFile.filename().string())).c_str(),undo.empty())) steps=-int(undo.size());
    for(size_t i=0;i<undo.size();++i) {
        ImGui::PushID(int(i));
        bool current=i+1==undo.size();
        if(ImGui::Selectable((std::string(current?icon::ChevronRight:"    ")+"  "+undo[i]).c_str(),current) && !current) steps=-int(undo.size()-1-i);
        if(current && document.previewing()) {ImGui::SameLine();ImGui::TextColored(toVec4(theme::Accent),"editing...");}
        ImGui::PopID();
    }
    ImGui::PushStyleColor(ImGuiCol_Text,toVec4(theme::TextDim));
    for(size_t i=0;i<redo.size();++i) {
        ImGui::PushID(int(1000+i));
        if(ImGui::Selectable(("    "+std::string("  ")+redo[i]).c_str())) steps=int(i+1);
        ImGui::PopID();
    }
    ImGui::PopStyleColor();
    ImGui::EndDisabled();
    if(undo.empty() && redo.empty()) {
        ImGui::PushStyleColor(ImGuiCol_Text,toVec4(theme::TextDim));ImGui::TextWrapped("Edits appear here. Click one to jump back.");ImGui::PopStyleColor();
    }
    ImGui::End();
    if(steps<0) attempt([&]{for(int i=0;i<-steps;++i) document.undo();});
    if(steps>0) attempt([&]{for(int i=0;i<steps;++i) document.redo();});
}
}
