#pragma once
#include <filesystem>
namespace swan {
// Prefer an installed `<prefix>/share/swan/<name>` beside the executable, else the build-tree fallback.
std::filesystem::path resourceDirectory(const char* name,const std::filesystem::path& buildFallback);
}
