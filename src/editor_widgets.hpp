#pragma once
// Inspector-style widgets shared by editor panels (implemented in editor_inspector.cpp).
#include <glm/glm.hpp>
namespace swan::ui {
// Two-column property table: dim label on the left, full-width editor on the right.
bool beginProperties(const char* id);
void property(const char* label);
// X/Y/Z drags with colored axis tabs; clicking a tab resets that component.
bool vectorControl(const char* id,glm::vec3& value,float speed,float reset,const char* probePrefix,const char* format="%.2f",float minimum=0);
// Collapsible section header with an icon (open by default).
bool sectionHeader(const char* glyph,const char* title,const char* id);
}
