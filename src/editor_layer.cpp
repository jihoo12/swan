#include "editor_layer.hpp"
#include "editor_icons.hpp"
#include "editor_probe.hpp"
#include "editor_theme.hpp"
#include "editor_view.hpp"
#include "resources.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>
#include <algorithm>
#include <cstdio>
#include <iostream>
namespace swan {
namespace {
constexpr const char* HierarchyWindow="###Hierarchy";
constexpr const char* InspectorWindow="###Inspector";
constexpr const char* ViewportWindow="###Viewport";
constexpr const char* AssetsWindow="###Assets";
constexpr const char* ConsoleWindow="###Console";
constexpr const char* HistoryWindow="###History";
// Segmented toolbar button: accent background while active.
bool toolButton(const char* glyph,bool active,const char* tooltip,const char* probeId=nullptr) {
    if(active) {
        ImGui::PushStyleColor(ImGuiCol_Button,toVec4(theme::AccentSoft));
        ImGui::PushStyleColor(ImGuiCol_Text,toVec4(IM_COL32(178,188,255,255)));
    }
    bool pressed=ImGui::Button(glyph);
    if(active) ImGui::PopStyleColor(2);
    if(probeId) probe::item(probeId);
    ImGui::SetItemTooltip("%s",tooltip);
    return pressed;
}
}
EditorLayer::EditorLayer(SceneDocument initial,const Options& options)
    :document(std::move(initial)),sceneFile(options.scenePath),savePath(options.savePath),thirdPerson(options.thirdPerson) {
    configDirectory=editorConfigDirectory();
    settings=loadEditorSettings(configDirectory/"editor.json");
    camera.setSpeed(settings.cameraSpeed);
    if(!sceneFile.empty()) settings.addRecent(sceneFile);
    registerActions();
    log(Level::Info,sceneFile.empty()?"Started with an empty scene":"Opened "+sceneFile.string());
}
EditorLayer::~EditorLayer() {persistSettings();}
void EditorLayer::persistSettings() {
    settings.cameraSpeed=camera.speed();
    try {saveEditorSettings(configDirectory/"editor.json",settings);}
    catch(const std::exception& error) {std::cerr<<"Swan: cannot save editor settings: "<<error.what()<<'\n';}
}
void EditorLayer::initializeGui() {
    auto& io=ImGui::GetIO();
    std::error_code ec;std::filesystem::create_directories(configDirectory,ec);
    // Docking layout persists per user, never in scene files.
    iniPath=(configDirectory/"imgui.ini").string();io.IniFilename=iniPath.c_str();
    io.ConfigWindowsMoveFromTitleBarOnly=true;io.ConfigDragClickToInputText=true;io.ConfigDockingWithShift=false;
    if(!loadEditorFonts(resourceDirectory("fonts",SWAN_FONT_DIR))) log(Level::Warning,"Editor fonts not found; using the built-in font");
}
// ---------------------------------------------------------------------------------------------
// Frame
void EditorLayer::drawGui(const GuiFrame& frame) {
    time+=frame.deltaTime;
    float scale=settings.uiScale*std::max(frame.contentScale,1.0f);
    if(scale!=lastScale) {applyEditorTheme(scale);lastScale=scale;}
    probe::beginFrame();
    ImGuizmo::BeginFrame();
    camera.update(frame.deltaTime);
    if(playCaptured && ImGui::IsKeyPressed(ImGuiKey_Escape,false)) playCaptured=false;
    else actions.dispatchShortcuts();
    drawMenuBar();
    drawToolbar();
    drawStatusBar(frame);
    drawDockspace();
    if(showHierarchy) drawHierarchy();
    if(showInspector) drawInspector();
    drawViewport(frame);
    if(showAssets) drawAssets();
    if(showConsole) drawConsole();
    if(showHistory) drawHistory();
    palette.draw(actions,paletteItems());
    fileDialog.draw(settings.recentScenes);
    drawModals();
    drawShortcuts();
    drawToasts();
    if(showDemo) ImGui::ShowDemoWindow(&showDemo);
    // Previews live only while a widget or gizmo is held; releasing commits one undo step.
    if(document.previewing() && !ImGui::IsAnyItemActive() && !gizmoActive) attempt([&]{document.commitPreview();});
    probe::value("selection",document.selection());
    probe::value("modified",document.modified()?"true":"false");
    probe::value("mode",play?"play":"authoring");
    probe::value("undo",document.undoLabel());
    probe::endFrame();
}
void EditorLayer::registerActions() {
    auto editing=[this]{return editable();};
    auto hasSelection=[this]{return editable() && selectedEntity();};
    auto tools=[this]{return editable() && navigation==Navigation::None;};
    auto add=[&](EditorAction action){actions.add(std::move(action));};
    // File
    add({"file.new","New Scene",icon::FilePlus,"File",ImGuiMod_Ctrl|ImGuiKey_N,0,[this]{guardUnsaved([this]{newScene();});},editing});
    add({"file.open","Open Scene...",icon::FolderOpen,"File",ImGuiMod_Ctrl|ImGuiKey_O,0,[this]{
        guardUnsaved([this]{fileDialog.open(FileDialog::Mode::Open,sceneFile.empty()?std::filesystem::current_path()/"assets/scenes":sceneFile,[this](const auto& path){openScene(path);});});
    },editing});
    add({"file.save","Save",icon::Save,"File",ImGuiMod_Ctrl|ImGuiKey_S,0,[this]{saveScene();}});
    add({"file.save-as","Save As...",icon::Save,"File",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_S,0,[this]{
        fileDialog.open(FileDialog::Mode::Save,saveTarget().empty()?std::filesystem::current_path()/"untitled.swan.json":saveTarget(),[this](const auto& path){saveSceneAs(path);});
    }});
    add({"file.revert","Revert to Saved",icon::Refresh,"File",0,0,[this]{revertScene();},[this]{return editable() && !sceneFile.empty();}});
    add({"file.quit","Quit",icon::LogOut,"File",ImGuiMod_Ctrl|ImGuiKey_Q,0,[this]{requestQuit();}});
    // Edit
    add({"edit.undo","Undo",icon::Undo,"Edit",ImGuiMod_Ctrl|ImGuiKey_Z,0,[this]{
        auto label=document.previewing()?std::string("edit in progress"):document.undoLabel();
        if(attempt([&]{document.undo();})) notify(Level::Info,"Undo: "+label);
    },[this]{return document.canUndo();},{},true});
    add({"edit.redo","Redo",icon::Redo,"Edit",ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_Z,ImGuiMod_Ctrl|ImGuiKey_Y,[this]{
        auto label=document.redoLabel();
        if(attempt([&]{document.redo();})) notify(Level::Info,"Redo: "+label);
    },[this]{return document.canRedo();},{},true});
    add({"edit.duplicate","Duplicate",icon::Copy,"Edit",ImGuiMod_Ctrl|ImGuiKey_D,0,[this]{duplicateSelection();},hasSelection});
    add({"edit.delete","Delete",icon::Trash,"Edit",ImGuiKey_Delete,0,[this]{deleteSelection();},hasSelection});
    add({"edit.rename","Rename",icon::Pencil,"Edit",ImGuiKey_F2,0,[this]{showInspector=true;focusName=true;},hasSelection});
    add({"edit.deselect","Deselect",icon::Close,"Edit",ImGuiKey_Escape,0,[this]{document.select({});},[this]{return editable() && !document.selection().empty();}});
    add({"edit.make-unique","Make Material Unique",icon::Palette,"Edit",0,0,[this]{makeMaterialUnique();},hasSelection});
    // Create: one action per mesh asset is offered by the menu and palette.
    add({"create.cube","Add Cube",icon::Box,"Create",ImGuiMod_Shift|ImGuiKey_A,0,[this]{createEntity("builtin:cube");},editing});
    // View
    add({"view.focus","Frame Selection",icon::Focus,"View",ImGuiKey_F,0,[this]{focusSelection();},[this]{return !play && selectedEntity() && navigation==Navigation::None;}});
    add({"view.frame-all","Frame Scene",icon::Frame,"View",ImGuiKey_Home,0,[this]{
        const auto& scene=document.document().scene;
        glm::vec3 low(1e30f),high(-1e30f);
        for(auto id:scene.entities()) for(auto corner:worldBounds(scene,id)) {low=glm::min(low,corner);high=glm::max(high,corner);}
        if(scene.size()) camera.frame((low+high)*0.5f,glm::length(high-low)*0.5f);
    },[this]{return !play && document.document().scene.size()>0;}});
    add({"view.hierarchy","Hierarchy",icon::Tree,"View",0,0,[this]{showHierarchy=!showHierarchy;},{},[this]{return showHierarchy;}});
    add({"view.inspector","Inspector",icon::Sliders,"View",0,0,[this]{showInspector=!showInspector;},{},[this]{return showInspector;}});
    add({"view.assets","Assets",icon::Package,"View",0,0,[this]{showAssets=!showAssets;},{},[this]{return showAssets;}});
    add({"view.console","Console",icon::Terminal,"View",0,0,[this]{showConsole=!showConsole;},{},[this]{return showConsole;}});
    add({"view.history","History",icon::History,"View",0,0,[this]{showHistory=!showHistory;},{},[this]{return showHistory;}});
    add({"view.stats","Viewport Statistics",icon::Gauge,"View",0,0,[this]{settings.showStats=!settings.showStats;},{},[this]{return settings.showStats;}});
    add({"view.grid","Ground Grid",icon::Grid,"View",0,0,[this]{settings.showGrid=!settings.showGrid;},{},[this]{return settings.showGrid;}});
    add({"view.zoom-in","Increase UI Size",icon::Plus,"View",ImGuiMod_Ctrl|ImGuiKey_Equal,0,[this]{settings.uiScale=std::min(2.0f,settings.uiScale+0.1f);}});
    add({"view.zoom-out","Decrease UI Size","","View",ImGuiMod_Ctrl|ImGuiKey_Minus,0,[this]{settings.uiScale=std::max(0.75f,settings.uiScale-0.1f);}});
    add({"view.zoom-reset","Reset UI Size","","View",ImGuiMod_Ctrl|ImGuiKey_0,0,[this]{settings.uiScale=1;}});
    add({"view.reset-layout","Reset Layout",icon::Layout,"View",0,0,[this]{resetLayout=true;}});
    // Tools
    add({"tool.select","Select Tool",icon::Pointer,"Tools",ImGuiKey_Q,0,[this]{gizmo=Gizmo::Select;},tools,[this]{return gizmo==Gizmo::Select;}});
    add({"tool.move","Move Tool",icon::Move,"Tools",ImGuiKey_W,0,[this]{gizmo=Gizmo::Translate;},tools,[this]{return gizmo==Gizmo::Translate;}});
    add({"tool.rotate","Rotate Tool",icon::Rotate,"Tools",ImGuiKey_E,0,[this]{gizmo=Gizmo::Rotate;},tools,[this]{return gizmo==Gizmo::Rotate;}});
    add({"tool.scale","Scale Tool",icon::Scale,"Tools",ImGuiKey_R,0,[this]{gizmo=Gizmo::Scale;},tools,[this]{return gizmo==Gizmo::Scale;}});
    add({"tool.space","Toggle Local/World Space",icon::Globe,"Tools",ImGuiKey_X,0,[this]{gizmoLocal=!gizmoLocal;},tools,[this]{return gizmoLocal;}});
    add({"tool.snap","Toggle Snapping",icon::Magnet,"Tools",0,0,[this]{settings.snap=!settings.snap;},{},[this]{return settings.snap;}});
    // Play
    add({"play.toggle","Play / Stop",icon::Play,"Play",ImGuiKey_F5,ImGuiMod_Ctrl|ImGuiKey_P,[this]{togglePlay();}});
    // Help
    add({"help.palette","Command Palette",icon::Command,"Help",ImGuiMod_Ctrl|ImGuiKey_K,ImGuiMod_Ctrl|ImGuiMod_Shift|ImGuiKey_P,[this]{palette.open();}});
    add({"help.shortcuts","Keyboard Shortcuts",icon::Keyboard,"Help",ImGuiKey_F1,0,[this]{showShortcuts=!showShortcuts;},{},[this]{return showShortcuts;}});
    add({"help.demo","Dear ImGui Demo",icon::Monitor,"Help",0,0,[this]{showDemo=!showDemo;},{},[this]{return showDemo;}});
}
std::vector<PaletteItem> EditorLayer::paletteItems() {
    std::vector<PaletteItem> items;
    const auto& scene=document.document().scene;
    if(!play) {
        for(auto id:scene.entities()) {
            const auto& entity=*scene.get(id);
            auto key=entity.key;
            items.push_back({"Select "+(entity.name.empty()?key:entity.name),key,icon::Locate,[this,key]{
                if(attempt([&]{document.select(key);})) {scrollToKey=key;focusSelection();}
            }});
        }
        for(const auto& [id,mesh]:scene.meshes().entries()) {
            (void)mesh;auto meshId=id;
            items.push_back({"Create "+meshId,"mesh",icon::Plus,[this,meshId]{createEntity(meshId);}});
        }
    }
    for(const auto& path:settings.recentScenes) {
        auto target=path;
        items.push_back({"Open recent "+path.filename().string(),path.parent_path().string(),icon::File,[this,target]{guardUnsaved([this,target]{openScene(target);});}});
    }
    return items;
}
void EditorLayer::drawMenuBar() {
    if(!ImGui::BeginMainMenuBar()) return;
    ImGui::TextColored(toVec4(theme::Accent),"%s",icon::Sparkles);ImGui::SameLine(0,4);
    ImGui::TextUnformatted("Swan");ImGui::SameLine(0,16);
    if(ImGui::BeginMenu("File")) {
        actions.menuItem("file.new");actions.menuItem("file.open");
        if(ImGui::BeginMenu((std::string(icon::History)+"  Open Recent").c_str(),!settings.recentScenes.empty() && editable())) {
            std::optional<std::filesystem::path> chosen;
            for(const auto& path:settings.recentScenes) if(ImGui::MenuItem(path.filename().string().c_str(),path.parent_path().string().c_str())) chosen=path;
            ImGui::Separator();
            if(ImGui::MenuItem("Clear Recent")) {settings.recentScenes.clear();persistSettings();}
            ImGui::EndMenu();
            if(chosen) guardUnsaved([this,path=*chosen]{openScene(path);});
        }
        ImGui::Separator();actions.menuItem("file.save");actions.menuItem("file.save-as");actions.menuItem("file.revert");
        ImGui::Separator();actions.menuItem("file.quit");
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Edit")) {
        const auto* undo=actions.find("edit.undo");const auto* redo=actions.find("edit.redo");
        auto undoText=std::string(icon::Undo)+"  Undo"+(document.undoLabel().empty()?"":" "+document.undoLabel());
        auto redoText=std::string(icon::Redo)+"  Redo"+(document.redoLabel().empty()?"":" "+document.redoLabel());
        if(ImGui::MenuItem(undoText.c_str(),shortcutText(undo->shortcut).c_str(),false,actions.isEnabled(*undo))) undo->run();
        if(ImGui::MenuItem(redoText.c_str(),shortcutText(redo->shortcut).c_str(),false,actions.isEnabled(*redo))) redo->run();
        ImGui::Separator();
        actions.menuItem("edit.duplicate");actions.menuItem("edit.delete");actions.menuItem("edit.rename");actions.menuItem("edit.make-unique");
        ImGui::Separator();actions.menuItem("edit.deselect");
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Create")) {
        actions.menuItem("create.cube");
        ImGui::BeginDisabled(!editable());
        const auto& meshes=document.document().scene.meshes().entries();
        if(meshes.size()>1) ImGui::SeparatorText("Scene meshes");
        // Create after the loop: an edit replaces the scene, invalidating this map's iterators.
        std::optional<std::string> chosen;
        for(const auto& [id,mesh]:meshes) {
            (void)mesh;if(id=="builtin:cube") continue;
            if(ImGui::MenuItem((std::string(icon::Shapes)+"  "+id).c_str())) chosen=id;
        }
        ImGui::EndDisabled();
        if(chosen) createEntity(*chosen);
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("View")) {
        for(const char* id:{"view.hierarchy","view.inspector","view.assets","view.console","view.history"}) actions.menuItem(id);
        ImGui::Separator();actions.menuItem("view.focus");actions.menuItem("view.frame-all");actions.menuItem("view.stats");actions.menuItem("view.grid");
        ImGui::Separator();actions.menuItem("view.zoom-in");actions.menuItem("view.zoom-out");actions.menuItem("view.zoom-reset");
        ImGui::Separator();actions.menuItem("view.reset-layout");
        ImGui::EndMenu();
    }
    if(ImGui::BeginMenu("Help")) {
        actions.menuItem("help.palette");actions.menuItem("help.shortcuts");ImGui::Separator();actions.menuItem("help.demo");
        ImGui::EndMenu();
    }
    // Centered document title, VS Code style.
    auto title=sceneTitle()+(document.modified()?"  \xe2\x80\xa2":"");
    float width=ImGui::CalcTextSize(title.c_str()).x;
    ImGui::SameLine(std::max(ImGui::GetCursorPosX()+16,(ImGui::GetWindowWidth()-width)*0.5f));
    ImGui::TextColored(toVec4(theme::TextDim),"%s",title.c_str());
    ImGui::EndMainMenuBar();
}
void EditorLayer::drawToolbar() {
    auto* viewport=ImGui::GetMainViewport();
    float height=ImGui::GetFrameHeight()+ImGui::GetStyle().WindowPadding.y*1.2f;
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{ImGui::GetStyle().WindowPadding.x,ImGui::GetStyle().WindowPadding.y*0.6f});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,toVec4(IM_COL32(19,20,24,255)));
    ImGuiWindowFlags flags=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_NoScrollWithMouse;
    if(ImGui::BeginViewportSideBar("##toolbar",viewport,ImGuiDir_Up,height,flags)) {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,{4,0});
        ImGui::BeginDisabled(!editable());
        if(toolButton(icon::Pointer,gizmo==Gizmo::Select,"Select (Q)","toolbar/select")) {gizmo=Gizmo::Select;}
        ImGui::SameLine();
        if(toolButton(icon::Move,gizmo==Gizmo::Translate,"Move (W)","toolbar/move")) {gizmo=Gizmo::Translate;}
        ImGui::SameLine();
        if(toolButton(icon::Rotate,gizmo==Gizmo::Rotate,"Rotate around Y (E)","toolbar/rotate")) {gizmo=Gizmo::Rotate;}
        ImGui::SameLine();
        if(toolButton(icon::Scale,gizmo==Gizmo::Scale,"Scale (R)","toolbar/scale")) {gizmo=Gizmo::Scale;}
        ImGui::SameLine(0,14);
        if(toolButton(gizmoLocal?icon::Box:icon::Globe,false,gizmoLocal?"Local space (X)":"World space (X)")) {gizmoLocal=!gizmoLocal;}
        ImGui::SameLine();
        if(toolButton(icon::Magnet,settings.snap,"Snapping (hold Ctrl to invert)","toolbar/snap")) settings.snap=!settings.snap;
        ImGui::SameLine();
        if(ImGui::ArrowButton("##snap-options",ImGuiDir_Down)) ImGui::OpenPopup("##snap");
        ImGui::SetItemTooltip("Snap increments");
        if(ImGui::BeginPopup("##snap")) {
            ImGui::SeparatorText("Snap increments");
            ImGui::SetNextItemWidth(120);ImGui::DragFloat("Move",&settings.snapTranslate,0.01f,0.01f,100,"%.2f m");
            ImGui::SetNextItemWidth(120);ImGui::DragFloat("Rotate",&settings.snapRotate,0.5f,1,180,"%.0f\xc2\xb0");
            ImGui::SetNextItemWidth(120);ImGui::DragFloat("Scale",&settings.snapScale,0.01f,0.01f,10,"%.2f");
            ImGui::EndPopup();
        }
        ImGui::EndDisabled();
        // Centered play control.
        float playWidth=ImGui::CalcTextSize("  Stop  ").x+ImGui::GetFontSize()+ImGui::GetStyle().FramePadding.x*2;
        ImGui::SameLine((ImGui::GetWindowWidth()-playWidth)*0.5f);
        ImGui::PushStyleColor(ImGuiCol_Button,toVec4(play?IM_COL32(150,60,60,255):IM_COL32(40,96,72,255)));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered,toVec4(play?IM_COL32(176,72,72,255):IM_COL32(48,116,86,255)));
        auto label=std::string(play?icon::Stop:icon::Play)+(play?"  Stop":"  Play");
        if(ImGui::Button(label.c_str(),{playWidth,0})) togglePlay();
        probe::item("toolbar/play");
        ImGui::SetItemTooltip(play?"Stop and discard runtime changes (F5)":"Play the scene in the viewport (F5)");
        ImGui::PopStyleColor(2);
        // Right: palette launcher styled as a search field.
        auto hint=std::string(icon::Search)+"  Search commands";
        auto keys=shortcutText(ImGuiMod_Ctrl|ImGuiKey_K);
        float searchWidth=std::max(220.0f*lastScale,ImGui::CalcTextSize((hint+keys).c_str()).x+40);
        ImGui::SameLine(ImGui::GetWindowWidth()-searchWidth-ImGui::GetStyle().WindowPadding.x);
        ImGui::PushStyleColor(ImGuiCol_Button,toVec4(theme::Surface));
        ImGui::PushStyleVar(ImGuiStyleVar_ButtonTextAlign,{0,0.5f});
        if(ImGui::Button(hint.c_str(),{searchWidth,0})) palette.open();
        ImGui::PopStyleVar();ImGui::PopStyleColor();
        auto min=ImGui::GetItemRectMin(),max=ImGui::GetItemRectMax();
        ImGui::GetWindowDrawList()->AddText({max.x-ImGui::CalcTextSize(keys.c_str()).x-10,min.y+ImGui::GetStyle().FramePadding.y},theme::TextDim,keys.c_str());
        ImGui::PopStyleVar();
    }
    ImGui::End();
    ImGui::PopStyleColor();ImGui::PopStyleVar();
}
void EditorLayer::drawStatusBar(const GuiFrame& frame) {
    auto* viewport=ImGui::GetMainViewport();
    float height=ImGui::GetFrameHeight();
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding,{10,3});
    ImGui::PushStyleColor(ImGuiCol_WindowBg,toVec4(IM_COL32(19,20,24,255)));
    ImGuiWindowFlags flags=ImGuiWindowFlags_NoScrollbar|ImGuiWindowFlags_NoSavedSettings|ImGuiWindowFlags_MenuBar;
    if(ImGui::BeginViewportSideBar("##status",viewport,ImGuiDir_Down,height,flags)) {
        if(ImGui::BeginMenuBar()) {
            auto chip=[](ImU32 color,const char* text) {
                auto size=ImGui::CalcTextSize(text);auto pos=ImGui::GetCursorScreenPos();
                float h=ImGui::GetTextLineHeight();
                ImGui::GetWindowDrawList()->AddRectFilled({pos.x-6,pos.y-1},{pos.x+size.x+6,pos.y+h+1},color&0x40ffffff,h);
                ImGui::TextColored(toVec4(color),"%s",text);
            };
            ImGui::Dummy({2,0});ImGui::SameLine();
            chip(play?theme::Play:theme::Accent,play?"PLAYING":"EDITING");ImGui::SameLine(0,18);
            ImGui::TextColored(toVec4(theme::TextDim),"%s %s%s",icon::File,sceneTitle().c_str(),document.modified()?" (modified)":"");
            if(const auto* entity=selectedEntity()) {
                ImGui::SameLine(0,18);
                ImGui::TextColored(toVec4(theme::TextDim),"%s %s",icon::Pointer,(entity->name.empty()?entity->key:entity->name).c_str());
            }
            if(!logs.empty()) {
                const auto& last=logs.back();
                ImGui::SameLine(0,18);
                ImGui::TextColored(toVec4(last.level==Level::Error?theme::Error:last.level==Level::Warning?theme::Warning:theme::TextDim),"%s",last.text.c_str());
            }
            char stats[160];
            std::snprintf(stats,sizeof stats,"%zu entities   %llu drawn   %.0f FPS",document.document().scene.size(),(unsigned long long)frame.visibleObjects,frame.fps);
            ImGui::SameLine(ImGui::GetWindowWidth()-ImGui::CalcTextSize(stats).x-14);
            ImGui::TextColored(toVec4(theme::TextDim),"%s",stats);
            ImGui::EndMenuBar();
        }
    }
    ImGui::End();
    ImGui::PopStyleColor();ImGui::PopStyleVar();
}
void EditorLayer::drawDockspace() {
    auto* viewport=ImGui::GetMainViewport();
    ImGuiID id=ImGui::GetID("SwanDockspace");
    if(resetLayout || !ImGui::DockBuilderGetNode(id) || ImGui::DockBuilderGetNode(id)->IsEmpty()) {
        resetLayout=false;
        ImGui::DockBuilderRemoveNode(id);
        ImGui::DockBuilderAddNode(id,ImGuiDockNodeFlags_DockSpace);
        ImGui::DockBuilderSetNodeSize(id,viewport->WorkSize);
        ImGuiID center=id;
        ImGuiID left=ImGui::DockBuilderSplitNode(center,ImGuiDir_Left,0.2f,nullptr,&center);
        ImGuiID right=ImGui::DockBuilderSplitNode(center,ImGuiDir_Right,0.27f,nullptr,&center);
        ImGuiID bottom=ImGui::DockBuilderSplitNode(center,ImGuiDir_Down,0.28f,nullptr,&center);
        ImGuiID leftBottom=ImGui::DockBuilderSplitNode(left,ImGuiDir_Down,0.32f,nullptr,&left);
        ImGui::DockBuilderDockWindow(HierarchyWindow,left);
        ImGui::DockBuilderDockWindow(HistoryWindow,leftBottom);
        ImGui::DockBuilderDockWindow(InspectorWindow,right);
        ImGui::DockBuilderDockWindow(AssetsWindow,bottom);
        ImGui::DockBuilderDockWindow(ConsoleWindow,bottom);
        ImGui::DockBuilderDockWindow(ViewportWindow,center);
        if(auto* node=ImGui::DockBuilderGetNode(center)) node->LocalFlags|=ImGuiDockNodeFlags_HiddenTabBar;
        ImGui::DockBuilderFinish(id);
        showHierarchy=showInspector=showAssets=showConsole=showHistory=true;
    }
    // Panels close from their tab (shown on hover) or the View menu; no per-node close/menu buttons.
    ImGui::DockSpaceOverViewport(id,viewport,ImGuiDockNodeFlags_NoCloseButton|ImGuiDockNodeFlags_NoWindowMenuButton);
}
void EditorLayer::drawModals() {
    if(openUnsaved) {ImGui::OpenPopup("Unsaved Changes###unsaved");openUnsaved=false;}
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_Appearing,{0.5f,0.5f});
    if(ImGui::BeginPopupModal("Unsaved Changes###unsaved",nullptr,ImGuiWindowFlags_AlwaysAutoResize|ImGuiWindowFlags_NoMove)) {
        ImGui::Dummy({380*lastScale,0});
        ImGui::TextColored(toVec4(theme::Warning),"%s",icon::Warning);ImGui::SameLine();
        ImGui::Text("Save changes to \"%s\"?",sceneTitle().c_str());
        ImGui::TextColored(toVec4(theme::TextDim),"Your changes will be lost if you don't save them.");
        ImGui::Spacing();ImGui::Spacing();
        std::function<void()> next;
        if(ImGui::Button("Don't Save")) {next=std::move(afterUnsaved);ImGui::CloseCurrentPopup();}
        probe::item("modal/discard");
        float right=ImGui::CalcTextSize("Cancel").x+ImGui::CalcTextSize("Save").x+ImGui::GetStyle().FramePadding.x*4+ImGui::GetStyle().ItemSpacing.x+16;
        ImGui::SameLine(contentRight()-right);
        if(ImGui::Button("Cancel") || ImGui::IsKeyPressed(ImGuiKey_Escape)) {afterUnsaved=nullptr;ImGui::CloseCurrentPopup();}
        probe::item("modal/cancel");
        ImGui::SameLine();
        ImGui::PushStyleColor(ImGuiCol_Button,toVec4(theme::Accent));
        if(ImGui::Button("  Save  ") || ImGui::IsKeyPressed(ImGuiKey_Enter)) {
            ImGui::CloseCurrentPopup();
            if(saveTarget().empty()) {
                auto then=std::move(afterUnsaved);
                fileDialog.open(FileDialog::Mode::Save,std::filesystem::current_path()/"untitled.swan.json",[this,then](const auto& path){
                    saveSceneAs(path);if(!document.modified() && then) then();
                });
            } else if(saveScene()) next=std::move(afterUnsaved);
        }
        ImGui::PopStyleColor();
        probe::item("modal/save");
        ImGui::EndPopup();
        if(next) next();
    }
}
void EditorLayer::drawShortcuts() {
    if(!showShortcuts) return;
    ImGui::SetNextWindowSize({560*lastScale,560*lastScale},ImGuiCond_FirstUseEver);
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(),ImGuiCond_FirstUseEver,{0.5f,0.5f});
    auto title=std::string(icon::Keyboard)+"  Keyboard Shortcuts###shortcuts";
    if(ImGui::Begin(title.c_str(),&showShortcuts,ImGuiWindowFlags_NoDocking|ImGuiWindowFlags_NoCollapse)) {
        auto row=[](const std::string& label,const std::string& keys) {
            ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::TextUnformatted(label.c_str());
            ImGui::TableNextColumn();ImGui::TextColored(toVec4(theme::TextDim),"%s",keys.c_str());
        };
        if(ImGui::BeginTable("##keys",2,ImGuiTableFlags_RowBg|ImGuiTableFlags_SizingStretchProp)) {
            std::string category;
            for(const auto& action:actions.all()) {
                if(!action.shortcut) continue;
                if(action.category!=category) {
                    category=action.category;ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Spacing();
                    ImGui::TextColored(toVec4(theme::Accent),"%s",category.c_str());
                }
                auto keys=shortcutText(action.shortcut)+(action.alternate?"   or   "+shortcutText(action.alternate):"");
                row(action.label,keys);
            }
            ImGui::TableNextRow();ImGui::TableNextColumn();ImGui::Spacing();ImGui::TextColored(toVec4(theme::Accent),"Viewport");
            row("Fly (look around)","Hold Right Mouse");
            row("Move while flying","W A S D, Q / E down / up, Shift faster");
            row("Change fly speed","Mouse Wheel while flying");
            row("Orbit around pivot","Alt + Left Mouse");
            row("Pan","Middle Mouse");
            row("Zoom","Mouse Wheel");
            row("Select","Left Click");
            row("Assign material / place mesh","Drag from Assets");
            row("Snap while transforming","Hold Ctrl");
            row("Release mouse in Play","Esc");
            ImGui::EndTable();
        }
    }
    ImGui::End();
}
void EditorLayer::drawToasts() {
    std::erase_if(toasts,[&](const Toast& toast){return toast.expires<time;});
    if(toasts.empty()) return;
    auto* viewport=ImGui::GetMainViewport();
    float y=viewport->WorkPos.y+viewport->WorkSize.y-16;
    auto* draw=ImGui::GetForegroundDrawList();
    for(auto it=toasts.rbegin();it!=toasts.rend();++it) {
        float alpha=std::clamp(float(it->expires-time)/0.35f,0.0f,1.0f);
        ImU32 color=it->level==Level::Error?theme::Error:it->level==Level::Warning?theme::Warning:it->level==Level::Success?theme::Play:theme::Accent;
        const char* glyph=it->level==Level::Error?icon::Error:it->level==Level::Warning?icon::Warning:it->level==Level::Success?icon::Success:icon::Info;
        auto text=std::string(glyph)+"   "+it->text;
        auto size=ImGui::CalcTextSize(text.c_str(),nullptr,false,420*lastScale);
        ImVec2 max{viewport->WorkPos.x+viewport->WorkSize.x-16,y},min{max.x-size.x-28,y-size.y-18};
        auto fade=[&](ImU32 c){return (c&0x00ffffff)|(ImU32(float(c>>24)*alpha)<<24);};
        draw->AddRectFilled({min.x+2,min.y+3},{max.x+2,max.y+3},fade(IM_COL32(0,0,0,90)),8.0f);
        draw->AddRectFilled(min,max,fade(IM_COL32(36,37,46,250)),8.0f);
        draw->AddRectFilled(min,{min.x+3,max.y},fade(color),8.0f,ImDrawFlags_RoundCornersLeft);
        draw->AddText(nullptr,0,{min.x+14,min.y+9},fade(IM_COL32(230,231,236,255)),text.c_str(),nullptr,420*lastScale);
        y=min.y-8;
    }
}
// ---------------------------------------------------------------------------------------------
// Operations
bool EditorLayer::attempt(const std::function<void()>& action) {
    try {action();return true;}
    catch(const std::exception& error) {notify(Level::Error,error.what());return false;}
}
void EditorLayer::preview(const SceneEdit& edit,std::string label) {
    // Invalid intermediate values (e.g. a zero scale while typing) keep the last valid preview.
    try {document.showPreview(edit,std::move(label));}
    catch(const std::exception& error) {if(logs.empty() || logs.back().text!=error.what()) log(Level::Warning,error.what());}
}
void EditorLayer::log(Level level,std::string text) {
    if(level==Level::Error) ++unreadErrors;
    logs.push_back({level,std::move(text),time});
    if(logs.size()>500) logs.pop_front();
}
void EditorLayer::notify(Level level,std::string text) {
    toasts.push_back({level,text,time+(level==Level::Error?5.0:2.6)});
    if(toasts.size()>4) toasts.erase(toasts.begin());
    log(level,std::move(text));
}
void EditorLayer::guardUnsaved(std::function<void()> then) {
    if(!document.modified()) {then();return;}
    afterUnsaved=std::move(then);openUnsaved=true;
}
void EditorLayer::newScene() {
    if(attempt([&]{document.reset({});})) {sceneFile.clear();savePath.clear();notify(Level::Info,"New scene");}
}
void EditorLayer::openScene(const std::filesystem::path& path) {
    if(!attempt([&]{document.load(path);})) {settings.removeRecent(std::filesystem::absolute(path));return;}
    sceneFile=path;savePath.clear();settings.addRecent(path);persistSettings();
    notify(Level::Success,"Opened "+path.filename().string());
    focusSelection();
}
std::filesystem::path EditorLayer::saveTarget() const {return savePath.empty()?sceneFile:savePath;}
bool EditorLayer::saveScene() {
    auto target=saveTarget();
    if(target.empty()) {actions.run("file.save-as");return false;}
    if(!attempt([&]{document.save(target);})) return false;
    if(savePath.empty()) sceneFile=target;
    settings.addRecent(target);persistSettings();
    notify(Level::Success,"Saved "+target.filename().string());
    return true;
}
void EditorLayer::saveSceneAs(const std::filesystem::path& path) {
    savePath.clear();sceneFile=path;saveScene();
}
void EditorLayer::revertScene() {
    guardUnsaved([this]{if(attempt([&]{document.load(sceneFile);})) notify(Level::Info,"Reverted to "+sceneFile.filename().string());});
}
void EditorLayer::togglePlay() {
    if(play) {
        play.reset();document.stopPlay();playCaptured=false;
        notify(Level::Info,"Stopped; runtime changes discarded");return;
    }
    attempt([&]{
        document.startPlay();
        try {
            auto runtime=document.runtime();
            auto game=std::make_unique<Game>(gardenFromScene(std::move(runtime.scene),runtime.spawn),false);
            game->setThirdPerson(thirdPerson);play=std::move(game);
        } catch(...) {document.stopPlay();throw;}
        navigation=Navigation::None;
        notify(Level::Info,"Playing: click the viewport to control, Esc releases the mouse");
    });
}
void EditorLayer::createEntity(const std::string& meshId,std::optional<glm::vec3> position) {
    auto entity=editorCube(camera.camera());
    entity.meshId=meshId;
    entity.name=meshId=="builtin:cube"?"Cube":meshId;
    if(position) entity.transform.position=*position;
    if(attempt([&]{document.apply(CreateEntity{entity,{}});})) scrollToKey=document.selection();
}
void EditorLayer::duplicateSelection() {
    const auto* entity=selectedEntity();
    if(!entity) return;
    const auto& scene=document.document().scene;
    Entity copy=*entity;copy.key.clear();copy.name+=" copy";
    std::optional<std::string> parentKey;
    if(auto parent=scene.parent(scene.find(entity->key))) parentKey=scene.get(*parent)->key;
    if(attempt([&]{document.apply(CreateEntity{copy,parentKey},"Duplicate "+(entity->name.empty()?entity->key:entity->name));})) scrollToKey=document.selection();
}
void EditorLayer::deleteSelection() {
    if(!selectedEntity()) return;
    auto key=document.selection();
    attempt([&]{document.apply(DeleteEntity{key});});
}
void EditorLayer::focusSelection() {
    const auto& scene=document.document().scene;
    auto id=scene.find(document.selection());
    if(!scene.get(id)) return;
    glm::vec3 low(1e30f),high(-1e30f);
    for(auto corner:worldBounds(scene,id)) {low=glm::min(low,corner);high=glm::max(high,corner);}
    camera.frame((low+high)*0.5f,glm::length(high-low)*0.5f);
}
void EditorLayer::reparent(const std::string& key,std::optional<std::string> parent) {
    attempt([&]{
        const auto& scene=document.document().scene;
        auto id=scene.find(key);
        const auto* entity=scene.get(id);
        if(!entity) return;
        std::vector<SceneEdit> edits{SetParent{key,parent}};
        // Keep the object where it is on screen; animated objects keep their authored local motion.
        if(!entity->animation) {
            auto world=scene.worldTransform(id);
            edits.push_back(SetTransform{key,parent?relativeTransform(scene.worldTransform(scene.find(*parent)),world):world});
        }
        document.apply(edits,(parent?"Parent ":"Unparent ")+(entity->name.empty()?key:entity->name));
    });
}
void EditorLayer::assignMaterial(const std::string& key,const std::string& materialId) {
    const auto& scene=document.document().scene;
    const auto* entity=scene.get(scene.find(key));
    if(!entity || entity->materialId==materialId) return;
    auto edit=SetEntityProperties{key,entity->name,entity->meshId,materialId,entity->solid,entity->collectible,entity->goal};
    if(attempt([&]{document.apply(edit,"Assign "+materialId+" to "+(entity->name.empty()?key:entity->name));}))
        notify(Level::Success,"Assigned material "+materialId);
}
void EditorLayer::makeMaterialUnique() {
    const auto* entity=selectedEntity();
    if(!entity) return;
    const auto& scene=document.document().scene;
    auto id=uniqueMaterialId(scene.assets(),entity->materialId);
    std::vector<SceneEdit> edits{CreateMaterial{id,scene.assets().get(entity->materialId)},
        SetEntityProperties{entity->key,entity->name,entity->meshId,id,entity->solid,entity->collectible,entity->goal}};
    if(attempt([&]{document.apply(edits,"Make material unique ("+id+")");})) notify(Level::Success,"Created material "+id);
}
void EditorLayer::requestQuit() {guardUnsaved([this]{closeApproved=true;quitting=true;});}
bool EditorLayer::allowClose() {
    if(closeApproved || !document.modified()) return true;
    guardUnsaved([this]{closeApproved=true;quitting=true;});
    return false;
}
std::string EditorLayer::sceneTitle() const {
    auto target=saveTarget();
    return target.empty()?"Untitled scene":target.filename().string();
}
const Entity* EditorLayer::selectedEntity() const {
    const auto& scene=document.document().scene;
    return scene.get(scene.find(document.selection()));
}
// ---------------------------------------------------------------------------------------------
// Runtime integration
void EditorLayer::handleInput(const Input& input) {
    if(!play) return;
    Input forwarded=input;forwarded.reload=forwarded.save=false;
    if(!playCaptured) forwarded.look={};
    play->handleInput(forwarded);
}
void EditorLayer::fixedUpdate(float dt,const Input& input) {
    if(!play) return;
    Input forwarded=input;forwarded.reload=forwarded.save=false;
    play->fixedUpdate(dt,forwarded);
}
RenderFrame EditorLayer::renderFrame(float interpolation) const {
    RenderFrame frame;
    if(play) frame=play->renderFrame(interpolation);
    else {
        frame.camera=camera.camera();
        const auto& scene=document.document().scene;
        for(auto id:scene.entities()) {
            const auto& entity=*scene.get(id);const auto& material=scene.assets().get(entity.materialId);
            frame.objects.push_back({scene.worldTransform(id),material,scene.meshes().get(entity.meshId).data,scene.textures().get(material.textureId).data});
        }
    }
    frame.targetSize=viewportSize;
    return frame;
}
std::string EditorLayer::status() const {
    if(play) return "EDITOR | PLAY | "+play->status();
    return "EDITOR | AUTHORING | "+sceneTitle()+(document.modified()?" *":"");
}
std::optional<bool> EditorLayer::cursorCapture() const {
    return (play && playCaptured) || navigation==Navigation::Fly || navigation==Navigation::Orbit;
}
}
