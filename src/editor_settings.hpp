#pragma once
#include <filesystem>
#include <vector>
namespace swan {
// Per-user editor preferences; never stored in scene files.
struct EditorSettings {
    static constexpr size_t maxRecent=10;
    std::vector<std::filesystem::path> recentScenes; // Most recent first, absolute paths.
    float cameraSpeed=6,uiScale=1;
    bool snap=false,showStats=true,showGrid=false;
    float snapTranslate=0.5f,snapRotate=15,snapScale=0.1f;
    void addRecent(const std::filesystem::path& scene);
    void removeRecent(const std::filesystem::path& scene);
};
// $SWAN_CONFIG_HOME, else $XDG_CONFIG_HOME/swan, else ~/.config/swan.
std::filesystem::path editorConfigDirectory();
// Missing or malformed files yield defaults; out-of-range values are clamped field by field.
EditorSettings loadEditorSettings(const std::filesystem::path& file);
// Creates parent directories and replaces the file atomically.
void saveEditorSettings(const std::filesystem::path& file,const EditorSettings& settings);
}
