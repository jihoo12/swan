#include "editor_settings.hpp"
#include <algorithm>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    auto directory=std::filesystem::temp_directory_path()/("swan-settings-"+std::to_string(getpid()));
    try {
        auto file=directory/"nested/editor.json";
        auto defaults=swan::loadEditorSettings(file);
        require(defaults.recentScenes.empty() && defaults.cameraSpeed==6 && defaults.fpsLimit==60,"Missing file must give defaults");
        swan::EditorSettings settings;
        settings.cameraSpeed=12;settings.snap=true;settings.snapRotate=45;settings.uiScale=1.25f;settings.fpsLimit=144;
        for(int i=0;i<14;++i) settings.addRecent(directory/("scene-"+std::to_string(i)+".json"));
        settings.addRecent(directory/"scene-3.json");
        require(settings.recentScenes.size()==swan::EditorSettings::maxRecent,"Recent list not bounded");
        require(settings.recentScenes.front().filename()=="scene-3.json","Reopened file must move to the front");
        require(std::count(settings.recentScenes.begin(),settings.recentScenes.end(),settings.recentScenes.front())==1,"Recent list has duplicates");
        swan::saveEditorSettings(file,settings);
        auto loaded=swan::loadEditorSettings(file);
        require(loaded.cameraSpeed==12 && loaded.snap && loaded.snapRotate==45 && loaded.uiScale==1.25f && loaded.fpsLimit==144,"Settings did not round-trip");
        require(loaded.recentScenes==settings.recentScenes,"Recent scenes did not round-trip");
        {std::ofstream output(file);output<<R"({"camera_speed":-5,"ui_scale":"big","snap_rotate":1e9,"fps_limit":5,"recent_scenes":[3,"a.json"]})";}
        auto clamped=swan::loadEditorSettings(file);
        require(clamped.cameraSpeed==0.5f && clamped.uiScale==1 && clamped.snapRotate==180 && clamped.fpsLimit==15,"Invalid values not clamped/ignored");
        {std::ofstream output(file);output<<R"({"fps_limit":-3})";}
        require(swan::loadEditorSettings(file).fpsLimit==0,"Nonpositive frame-rate limit must mean unlimited");
        require(clamped.recentScenes.size()==1,"Non-string recent entries accepted");
        {std::ofstream output(file);output<<"{broken";}
        require(swan::loadEditorSettings(file).cameraSpeed==6,"Malformed file must give defaults");
        setenv("SWAN_CONFIG_HOME",directory.c_str(),1);
        require(swan::editorConfigDirectory()==directory,"SWAN_CONFIG_HOME ignored");
        unsetenv("SWAN_CONFIG_HOME");setenv("XDG_CONFIG_HOME","/tmp/xdg",1);
        require(swan::editorConfigDirectory()=="/tmp/xdg/swan","XDG_CONFIG_HOME ignored");
        std::filesystem::remove_all(directory);
        std::cout<<"Editor settings persistence, clamping and recent files passed\n";
    } catch(const std::exception& error) {std::filesystem::remove_all(directory);std::cerr<<error.what()<<'\n';return 1;}
}
