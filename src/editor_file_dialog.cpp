#include "editor_file_dialog.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include <imgui.h>
#include <algorithm>
#include <cstdio>
#include <cstdlib>
namespace swan {
namespace {
bool matches(const std::filesystem::path& path,const std::string& extension) {
    auto name=path.filename().string();
    return name.size()>extension.size() && name.ends_with(extension);
}
std::string humanSize(uintmax_t bytes) {
    char text[32];
    if(bytes<1024) std::snprintf(text,sizeof text,"%ju B",bytes);
    else if(bytes<1024*1024) std::snprintf(text,sizeof text,"%.1f KB",double(bytes)/1024);
    else std::snprintf(text,sizeof text,"%.1f MB",double(bytes)/1024/1024);
    return text;
}
}
void FileDialog::open(Mode next,const std::filesystem::path& initial,Accept accept,std::string filter) {
    mode=next;onAccept=std::move(accept);requested=true;error.clear();extension=std::move(filter);
    std::error_code ec;
    auto start=initial.empty()?std::filesystem::current_path():std::filesystem::absolute(initial,ec);
    auto folder=std::filesystem::is_directory(start,ec)?start:start.parent_path();
    if(folder.empty() || !std::filesystem::is_directory(folder,ec)) folder=std::filesystem::current_path();
    std::snprintf(name.data(),name.size(),"%s",std::filesystem::is_directory(start,ec)?"":start.filename().string().c_str());
    if(mode==Mode::Save && !name[0]) std::snprintf(name.data(),name.size(),"%s",extension==".lua"?"behaviour.lua":"untitled.swan.json");
    navigate(folder);
}
void FileDialog::navigate(const std::filesystem::path& next) {
    std::error_code ec;
    auto target=std::filesystem::weakly_canonical(next,ec);
    if(ec || !std::filesystem::is_directory(target,ec)) {error="Not a folder: "+next.string();return;}
    std::vector<Entry> listed;
    for(std::filesystem::directory_iterator it(target,std::filesystem::directory_options::skip_permission_denied,ec),end;!ec && it!=end;it.increment(ec)) {
        auto filename=it->path().filename().string();
        if(filename.starts_with(".")) continue;
        bool folder=it->is_directory(ec);
        if(!folder && !showAll && !matches(it->path(),extension)) continue;
        listed.push_back({it->path(),folder,folder?0:it->file_size(ec)});
    }
    if(ec) {error="Cannot read folder: "+ec.message();return;}
    std::sort(listed.begin(),listed.end(),[](const auto& a,const auto& b){
        return a.directory!=b.directory?a.directory:a.path.filename().string()<b.path.filename().string();
    });
    directory=target;entries=std::move(listed);selected=-1;error.clear();
    std::snprintf(location.data(),location.size(),"%s",directory.string().c_str());
}
void FileDialog::accept(const std::filesystem::path& path) {
    if(path.filename().empty()) {error="Choose a file name";return;}
    std::error_code ec;
    if(mode==Mode::Open && !std::filesystem::is_regular_file(path,ec)) {error="File not found: "+path.filename().string();return;}
    isOpen=false;ImGui::CloseCurrentPopup();
    auto callback=onAccept;
    if(callback) callback(path);
}
void FileDialog::draw(const std::vector<std::filesystem::path>& recent) {
    bool script=extension==".lua";
    const char* title=mode==Mode::Open?(script?"Import Script###file-dialog":"Open Scene###file-dialog"):(script?"New Script###file-dialog":"Save Scene As###file-dialog");
    if(requested) {ImGui::OpenPopup("###file-dialog");requested=false;isOpen=true;focusName=mode==Mode::Save;}
    auto* viewport=ImGui::GetMainViewport();
    float scale=ImGui::GetStyle().FontScaleMain;
    ImGui::SetNextWindowSize({std::min(860*scale,viewport->WorkSize.x-40),std::min(540*scale,viewport->WorkSize.y-40)},ImGuiCond_Appearing);
    ImGui::SetNextWindowPos(viewport->GetCenter(),ImGuiCond_Appearing,{0.5f,0.5f});
    bool keepOpen=true;
    if(!ImGui::BeginPopupModal(title,&keepOpen,ImGuiWindowFlags_NoCollapse)) {isOpen=false;return;}
    if(!keepOpen) {isOpen=false;ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
    // Toolbar: up, editable location, and a filter toggle.
    if(ImGui::Button(icon::ArrowLeft) && directory.has_parent_path()) navigate(directory.parent_path());
    ImGui::SetItemTooltip("Parent folder");
    ImGui::SameLine();
    ImGui::SetNextItemWidth(-ImGui::CalcTextSize("Show all files").x-ImGui::GetFrameHeight()-ImGui::GetStyle().ItemSpacing.x*2);
    if(ImGui::InputText("##location",location.data(),location.size(),ImGuiInputTextFlags_EnterReturnsTrue)) navigate(location.data());
    ImGui::SameLine();
    if(ImGui::Checkbox("Show all files",&showAll)) navigate(directory);
    float footer=ImGui::GetFrameHeightWithSpacing()*2+ImGui::GetStyle().ItemSpacing.y;
    // Places sidebar.
    ImGui::BeginChild("##places",{180*scale,-footer},ImGuiChildFlags_None);
    ImGui::SeparatorText("Places");
    std::error_code ec;
    auto place=[&](const char* glyph,const char* label,const std::filesystem::path& path) {
        if(path.empty() || !std::filesystem::is_directory(path,ec)) return;
        auto text=std::string(glyph)+"  "+label;
        if(ImGui::Selectable(text.c_str(),directory==path)) navigate(path);
        ImGui::SetItemTooltip("%s",path.string().c_str());
    };
    place(icon::Folder,"Working folder",std::filesystem::current_path());
    place(icon::Mountain,"Scenes",std::filesystem::current_path()/"assets/scenes");
    place(icon::FileCode,"Scripts",std::filesystem::current_path()/"assets/scripts");
    if(const char* home=std::getenv("HOME")) place(icon::House,"Home",home);
    if(!recent.empty()) {
        ImGui::SeparatorText("Recent");
        for(const auto& path:recent) {
            auto text=std::string(icon::File)+"  "+path.filename().string();
            ImGui::PushID(path.string().c_str());
            if(ImGui::Selectable(text.c_str())) {navigate(path.parent_path());std::snprintf(name.data(),name.size(),"%s",path.filename().string().c_str());}
            ImGui::SetItemTooltip("%s",path.string().c_str());
            ImGui::PopID();
        }
    }
    ImGui::EndChild();
    ImGui::SameLine();
    // File list.
    ImGui::BeginChild("##files",{0,-footer},ImGuiChildFlags_Borders);
    if(ImGui::BeginTable("##entries",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_ScrollY|ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn("Name",ImGuiTableColumnFlags_WidthStretch);
        ImGui::TableSetupColumn("Size",ImGuiTableColumnFlags_WidthFixed,90*scale);
        ImGui::TableSetupScrollFreeze(0,1);ImGui::TableHeadersRow();
        std::filesystem::path enter;
        for(int i=0;i<int(entries.size());++i) {
            const auto& entry=entries[i];
            ImGui::TableNextRow();ImGui::TableNextColumn();
            auto label=std::string(entry.directory?icon::Folder:icon::FileCode)+"  "+entry.path.filename().string();
            ImGui::PushID(i);
            if(entry.directory) ImGui::PushStyleColor(ImGuiCol_Text,toVec4(IM_COL32(200,205,230,255)));
            if(ImGui::Selectable(label.c_str(),selected==i,ImGuiSelectableFlags_SpanAllColumns|ImGuiSelectableFlags_AllowDoubleClick)) {
                selected=i;
                if(!entry.directory) std::snprintf(name.data(),name.size(),"%s",entry.path.filename().string().c_str());
                if(ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {if(entry.directory) enter=entry.path; else accept(entry.path);}
            }
            if(entry.directory) ImGui::PopStyleColor();
            probe::item("file-dialog/"+entry.path.filename().string());
            ImGui::TableNextColumn();
            if(!entry.directory) ImGui::TextDisabled("%s",humanSize(entry.size).c_str());
            ImGui::PopID();
            if(!isOpen) break;
        }
        ImGui::EndTable();
        if(!enter.empty()) navigate(enter);
    }
    ImGui::EndChild();
    if(!isOpen) {ImGui::EndPopup();return;}
    // Footer: file name, status, and actions.
    ImGui::AlignTextToFramePadding();ImGui::TextUnformatted("File name");ImGui::SameLine();
    ImGui::SetNextItemWidth(-FLT_MIN);
    if(focusName) {ImGui::SetKeyboardFocusHere();focusName=false;}
    bool submit=ImGui::InputText("##name",name.data(),name.size(),ImGuiInputTextFlags_EnterReturnsTrue);
    probe::item("file-dialog/name");
    auto target=directory/std::string(name.data());
    if(!error.empty()) ImGui::TextColored(toVec4(theme::Error),"%s  %s",icon::Error,error.c_str());
    else if(mode==Mode::Save && name[0] && std::filesystem::exists(target,ec)) ImGui::TextColored(toVec4(theme::Warning),"%s  %s will be replaced",icon::Warning,name.data());
    else ImGui::TextDisabled("%s",directory.string().c_str());
    float buttons=ImGui::CalcTextSize("Cancel").x+ImGui::CalcTextSize("Save").x+ImGui::GetStyle().FramePadding.x*4+ImGui::GetStyle().ItemSpacing.x+40*scale;
    ImGui::SameLine(contentRight()-buttons);
    if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {isOpen=false;ImGui::CloseCurrentPopup();ImGui::EndPopup();return;}
    ImGui::SameLine();
    ImGui::PushStyleColor(ImGuiCol_Button,toVec4(theme::Accent));
    ImGui::BeginDisabled(!name[0]);
    if(ImGui::Button(mode==Mode::Open?"  Open  ":"  Save  ") || submit) accept(target);
    ImGui::EndDisabled();ImGui::PopStyleColor();
    probe::item("file-dialog/accept");
    ImGui::EndPopup();
}
}
