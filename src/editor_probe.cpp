#include "editor_probe.hpp"
#include <nlohmann/json.hpp>
#include <cstdlib>
#include <filesystem>
#include <fstream>
namespace swan::probe {
namespace {
struct State { std::string path; nlohmann::json current,written; };
State& state() {
    static State value=[]{State s;if(const char* path=std::getenv("SWAN_EDITOR_PROBE")) s.path=path;return s;}();
    return value;
}
}
bool enabled() {return !state().path.empty();}
void beginFrame() {if(enabled()) state().current={{"rects",nlohmann::json::object()},{"values",nlohmann::json::object()}};}
void rect(std::string_view id,ImVec2 min,ImVec2 max) {
    if(enabled()) state().current["rects"][std::string(id)]={min.x,min.y,max.x,max.y};
}
void item(std::string_view id) {if(enabled()) rect(id,ImGui::GetItemRectMin(),ImGui::GetItemRectMax());}
void value(std::string_view id,std::string text) {if(enabled()) state().current["values"][std::string(id)]=std::move(text);}
void endFrame() {
    auto& s=state();
    if(!enabled() || s.current==s.written) return;
    auto temporary=s.path+".tmp";
    {std::ofstream output(temporary,std::ios::trunc);output<<s.current.dump();}
    std::error_code ec;std::filesystem::rename(temporary,s.path,ec);
    if(!ec) s.written=s.current;
}
}
