#include "editor_settings.hpp"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <unistd.h>
namespace swan {
namespace {
float clampFinite(float value,float low,float high,float fallback) {return std::isfinite(value)?std::clamp(value,low,high):fallback;}
}
void EditorSettings::addRecent(const std::filesystem::path& scene) {
    std::error_code ec;auto absolute=std::filesystem::absolute(scene,ec).lexically_normal();
    if(ec) return;
    removeRecent(absolute);
    recentScenes.insert(recentScenes.begin(),absolute);
    if(recentScenes.size()>maxRecent) recentScenes.resize(maxRecent);
}
void EditorSettings::removeRecent(const std::filesystem::path& scene) {
    std::erase(recentScenes,scene);
}
std::filesystem::path editorConfigDirectory() {
    if(const char* path=std::getenv("SWAN_CONFIG_HOME");path && *path) return path;
    if(const char* path=std::getenv("XDG_CONFIG_HOME");path && *path) return std::filesystem::path(path)/"swan";
    if(const char* home=std::getenv("HOME");home && *home) return std::filesystem::path(home)/".config/swan";
    return std::filesystem::current_path()/".swan";
}
EditorSettings loadEditorSettings(const std::filesystem::path& file) {
    EditorSettings settings;
    std::ifstream input(file);
    if(!input) return settings;
    auto root=nlohmann::json::parse(input,nullptr,false);
    if(!root.is_object()) return settings;
    auto number=[&](const char* key,float& value,float low,float high) {
        if(root.contains(key) && root[key].is_number()) value=clampFinite(root[key].get<float>(),low,high,value);
    };
    auto boolean=[&](const char* key,bool& value) {if(root.contains(key) && root[key].is_boolean()) value=root[key].get<bool>();};
    number("camera_speed",settings.cameraSpeed,0.5f,200);number("ui_scale",settings.uiScale,0.75f,2);
    number("snap_translate",settings.snapTranslate,0.01f,100);number("snap_rotate",settings.snapRotate,1,180);number("snap_scale",settings.snapScale,0.01f,10);
    boolean("snap",settings.snap);boolean("show_stats",settings.showStats);boolean("show_grid",settings.showGrid);
    if(root.contains("recent_scenes") && root["recent_scenes"].is_array())
        for(const auto& item:root["recent_scenes"]) if(item.is_string() && settings.recentScenes.size()<EditorSettings::maxRecent) settings.recentScenes.emplace_back(item.get<std::string>());
    return settings;
}
void saveEditorSettings(const std::filesystem::path& file,const EditorSettings& settings) {
    nlohmann::json root={{"camera_speed",settings.cameraSpeed},{"ui_scale",settings.uiScale},{"snap",settings.snap},
        {"snap_translate",settings.snapTranslate},{"snap_rotate",settings.snapRotate},{"snap_scale",settings.snapScale},
        {"show_stats",settings.showStats},{"show_grid",settings.showGrid},{"recent_scenes",nlohmann::json::array()}};
    for(const auto& path:settings.recentScenes) root["recent_scenes"].push_back(path.string());
    if(file.has_parent_path()) std::filesystem::create_directories(file.parent_path());
    auto temporary=file;temporary+=".tmp-"+std::to_string(getpid());
    {
        std::ofstream output(temporary,std::ios::trunc);
        output<<root.dump(2)<<'\n';
        if(!output) throw std::runtime_error("Cannot write editor settings: "+temporary.string());
    }
    std::filesystem::rename(temporary,file);
}
}
