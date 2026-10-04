#pragma once
#include <imgui.h>
#include <string>
#include <string_view>
// UI test hook: when SWAN_EDITOR_PROBE names a file, the editor publishes screen rectangles of
// tagged widgets and a few state values as JSON, so GUI tests locate widgets instead of
// hard-coding coordinates. Disabled (and nearly free) otherwise.
namespace swan::probe {
bool enabled();
void beginFrame();
void item(std::string_view id);                       // Rectangle of the last submitted item.
void rect(std::string_view id,ImVec2 min,ImVec2 max);
void value(std::string_view id,std::string text);
void endFrame();                                      // Writes the file atomically when it changed.
}
