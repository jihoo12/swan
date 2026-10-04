#include "resources.hpp"
namespace swan {
std::filesystem::path resourceDirectory(const char* name,const std::filesystem::path& buildFallback) {
    std::error_code ec;
    auto executable=std::filesystem::read_symlink("/proc/self/exe",ec);
    auto installed=executable.parent_path()/"../share/swan"/name;
    return (!ec && std::filesystem::exists(installed))?installed:buildFallback;
}
}
