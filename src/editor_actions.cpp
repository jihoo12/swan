#include "editor_actions.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "fuzzy.hpp"
#include <imgui_internal.h>
#include <algorithm>
namespace swan {
std::string shortcutText(ImGuiKeyChord chord) {return chord?ImGui::GetKeyChordName(chord):"";}
void ActionRegistry::add(EditorAction action) {actions.push_back(std::move(action));}
const EditorAction* ActionRegistry::find(const std::string& id) const {
    auto found=std::find_if(actions.begin(),actions.end(),[&](const auto& a){return a.id==id;});
    return found==actions.end()?nullptr:&*found;
}
bool ActionRegistry::run(const std::string& id) {
    const auto* action=find(id);
    if(!action || !isEnabled(*action)) return false;
    action->run();return true;
}
void ActionRegistry::dispatchShortcuts() {
    auto& io=ImGui::GetIO();
    // Modal dialogs and open popups (menus, combos, the palette) own the keyboard.
    if(ImGui::IsPopupOpen(nullptr,ImGuiPopupFlags_AnyPopupId|ImGuiPopupFlags_AnyPopupLevel)) return;
    for(const auto& action:actions) {
        for(auto chord:{action.shortcut,action.alternate}) {
            if(!chord) continue;
            bool command=chord&(ImGuiMod_Ctrl|ImGuiMod_Alt|ImGuiMod_Super);
            auto key=ImGuiKey(chord&~ImGuiMod_Mask_);
            bool function=key>=ImGuiKey_F1 && key<=ImGuiKey_F12;
            if(io.WantTextInput && !command && !function) continue;
            if(!isEnabled(action)) continue;
            ImGuiInputFlags flags=ImGuiInputFlags_RouteGlobal|(action.repeat?ImGuiInputFlags_Repeat:0);
            if(ImGui::Shortcut(chord,flags)) {action.run();break;}
        }
    }
}
void ActionRegistry::menuItem(const std::string& id) {
    const auto* action=find(id);
    if(!action) return;
    auto label=action->icon.empty()?"      "+action->label:action->icon+"  "+action->label;
    bool selected=action->checked && action->checked();
    if(ImGui::MenuItem(label.c_str(),shortcutText(action->shortcut).c_str(),selected,isEnabled(*action))) action->run();
}
void CommandPalette::draw(ActionRegistry& actions,const std::vector<PaletteItem>& extra) {
    if(requested) {ImGui::OpenPopup("##palette");query[0]='\0';selected=0;focusInput=true;requested=false;}
    const auto* viewport=ImGui::GetMainViewport();
    float width=std::min(620.0f*ImGui::GetStyle().FontScaleMain,viewport->WorkSize.x-40);
    ImGui::SetNextWindowPos({viewport->WorkPos.x+viewport->WorkSize.x*0.5f,viewport->WorkPos.y+viewport->WorkSize.y*0.12f},ImGuiCond_Always,{0.5f,0});
    ImGui::SetNextWindowSize({width,0});
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,10});
    if(!ImGui::BeginPopup("##palette",ImGuiWindowFlags_NoMove)) {ImGui::PopStyleVar();return;}
    struct Candidate { int score; std::string label,detail,icon; std::function<void()> run; bool enabled; };
    std::vector<Candidate> matches;
    std::string_view text=query.data();
    for(const auto& action:actions.all()) {
        auto haystack=action.category+" "+action.label;
        if(auto score=fuzzyScore(text,action.label)) matches.push_back({*score,action.label,shortcutText(action.shortcut),action.icon,action.run,actions.isEnabled(action)});
        else if(auto categoryScore=fuzzyScore(text,haystack)) matches.push_back({*categoryScore,action.label,shortcutText(action.shortcut),action.icon,action.run,actions.isEnabled(action)});
    }
    for(const auto& item:extra) if(auto score=fuzzyScore(text,item.label)) matches.push_back({*score,item.label,item.detail,item.icon,item.run,true});
    std::stable_sort(matches.begin(),matches.end(),[](const auto& a,const auto& b){return a.enabled!=b.enabled?a.enabled:a.score>b.score;});
    if(matches.size()>12) matches.resize(12);
    selected=matches.empty()?0:std::clamp(selected,0,int(matches.size())-1);
    if(ImGui::IsKeyPressed(ImGuiKey_DownArrow)) selected=matches.empty()?0:(selected+1)%int(matches.size());
    if(ImGui::IsKeyPressed(ImGuiKey_UpArrow)) selected=matches.empty()?0:(selected+int(matches.size())-1)%int(matches.size());
    ImGui::PushStyleColor(ImGuiCol_FrameBg,toVec4(theme::Panel));
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(focusInput) {ImGui::SetKeyboardFocusHere();focusInput=false;}
    std::string hint=std::string(icon::Search)+"  Type a command or search the scene...";
    bool submit=ImGui::InputTextWithHint("##query",hint.c_str(),query.data(),query.size(),ImGuiInputTextFlags_EnterReturnsTrue);
    probe::item("palette/input");
    if(ImGui::IsItemEdited()) selected=0;
    ImGui::PopStyleColor();
    ImGui::Spacing();
    std::function<void()> chosen;
    if(matches.empty()) ImGui::TextDisabled("No matching commands");
    for(int i=0;i<int(matches.size());++i) {
        const auto& match=matches[i];
        ImGui::PushID(i);ImGui::BeginDisabled(!match.enabled);
        auto label=(match.icon.empty()?std::string("   "):match.icon)+"   "+match.label;
        if(ImGui::Selectable(label.c_str(),i==selected,ImGuiSelectableFlags_AllowOverlap) && match.enabled) chosen=match.run;
        if(i==selected) ImGui::SetScrollHereY();
        if(!match.detail.empty()) {
            float detailWidth=ImGui::CalcTextSize(match.detail.c_str()).x;
            ImGui::SameLine(contentRight()-detailWidth);
            ImGui::TextColored(toVec4(theme::TextDim),"%s",match.detail.c_str());
        }
        ImGui::EndDisabled();ImGui::PopID();
    }
    if(submit && !matches.empty() && matches[selected].enabled) chosen=matches[selected].run;
    if(chosen) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();ImGui::PopStyleVar();
    // Run after closing, so actions that open their own popups (dialogs) are not nested in ours.
    if(chosen) chosen();
}
}
