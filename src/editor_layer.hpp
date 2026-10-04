#pragma once
#include "editor_actions.hpp"
#include "editor_camera.hpp"
#include "editor_document.hpp"
#include "editor_file_dialog.hpp"
#include "editor_settings.hpp"
#include "game.hpp"
#include "options.hpp"
#include "script_engine.hpp"
#include <array>
#include <deque>
#include <functional>
#include <memory>
namespace swan {
// Dear ImGui scene editor. Panels live in editor_*.cpp; all scene changes go through EditorDocument.
class EditorLayer final:public GameLayer {
public:
    EditorLayer(SceneDocument document,const Options& options);
    ~EditorLayer() override;
    void initializeGui() override;
    void drawGui(const GuiFrame& frame) override;
    void handleInput(const Input& input) override;
    void fixedUpdate(float dt,const Input& input) override;
    RenderFrame renderFrame(float interpolation) const override;
    std::string status() const override;
    bool allowClose() override;
    bool running() const override { return !quitting; }
    std::optional<bool> cursorCapture() const override;
private:
    enum class Level { Info, Success, Warning, Error };
    struct LogEntry { Level level; std::string text; double time; };
    struct Toast { Level level; std::string text; double expires; };
    enum class Gizmo { Select, Translate, Rotate, Scale };
    enum class Navigation { None, Fly, Orbit, Pan };
    // Frame structure (editor_layer.cpp).
    void registerActions();
    void drawMenuBar();
    void drawToolbar();
    void drawStatusBar(const GuiFrame& frame);
    void drawDockspace();
    void drawModals();
    void drawShortcuts();
    void drawToasts();
    std::vector<PaletteItem> paletteItems();
    // Panels.
    void drawHierarchy();                       // editor_hierarchy.cpp
    void drawInspector();                       // editor_inspector.cpp
    void drawViewport(const GuiFrame& frame);   // editor_viewport.cpp
    void drawViewportOverlay(ImVec2 origin,ImVec2 size,const GuiFrame& frame);
    void drawGizmo(ImVec2 origin,ImVec2 size);
    void navigateViewport(bool hovered,ImVec2 origin,ImVec2 size,float dt);
    void drawAssets();                          // editor_panels.cpp
    void drawConsole();
    void drawHistory();
    void drawScriptSection(const Entity& entity);   // editor_inspector.cpp
    int consoleHistoryStep(ImGuiInputTextCallbackData* data);
    // Operations.
    bool attempt(const std::function<void()>& action);
    void preview(const SceneEdit& edit,std::string label={});
    void log(Level level,std::string text);
    void notify(Level level,std::string text);
    void guardUnsaved(std::function<void()> then);
    void newScene();
    void openScene(const std::filesystem::path& path);
    bool saveScene();
    void saveSceneAs(const std::filesystem::path& path);
    void revertScene();
    void togglePlay();
    void createEntity(const std::string& meshId,std::optional<glm::vec3> position={});
    void duplicateSelection();
    void deleteSelection();
    void focusSelection();
    void reparent(const std::string& key,std::optional<std::string> parent);
    void assignMaterial(const std::string& key,const std::string& materialId);
    void makeMaterialUnique();
    void requestQuit();
    // Scripting (editor_layer.cpp).
    void runConsole(const std::string& code);
    void newScript();
    void importScript();
    void assignScript(const std::string& key,const std::string& scriptId);
    void persistSettings();
    std::string sceneTitle() const;
    std::filesystem::path saveTarget() const;
    const Entity* selectedEntity() const;
    bool editable() const { return !play; }
    EditorDocument document;
    EditorCamera camera;
    EditorSettings settings;
    std::filesystem::path configDirectory,sceneFile,savePath;
    std::string iniPath;
    std::unique_ptr<Game> play;
    bool thirdPerson=false,playCaptured=false;
    ActionRegistry actions;
    CommandPalette palette;
    FileDialog fileDialog;
    // Viewport.
    glm::uvec2 viewportSize{1280,720};
    Gizmo gizmo=Gizmo::Translate;
    bool gizmoLocal=false,gizmoActive=false,gizmoFailed=false;
    Navigation navigation=Navigation::None;
    bool selectArmed=false,pressOverGizmo=false;
    glm::vec2 pressPosition{};
    // Panels and dialogs.
    bool showHierarchy=true,showInspector=true,showAssets=true,showConsole=true,showHistory=true,showShortcuts=false,showDemo=false;
    bool resetLayout=false,focusName=false,openUnsaved=false,quitting=false,closeApproved=false;
    std::function<void()> afterUnsaved;
    std::array<char,128> hierarchyFilter{},assetFilter{};
    std::array<char,256> nameBuffer{};
    // Lua console: a trusted engine bound to this document as `doc`, created on first use.
    std::unique_ptr<ScriptEngine> console;
    std::string consoleInput,newPropertyName;
    std::vector<std::string> consoleHistory;
    int consoleHistoryIndex=-1,newPropertyType=0;
    bool focusConsole=false;
    std::string nameKey,scrollToKey,assetTab="Meshes";
    std::deque<LogEntry> logs;
    std::vector<Toast> toasts;
    size_t unreadErrors=0;
    double time=0;
    float lastScale=0;
};
}
