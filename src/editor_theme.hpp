#pragma once
#include <imgui.h>
#include <filesystem>
namespace swan {
// Semantic colors shared by panels; values are sRGB, matching the UNORM swapchain.
namespace theme {
inline constexpr ImU32 Accent=IM_COL32(124,140,255,255);
inline constexpr ImU32 AccentSoft=IM_COL32(124,140,255,56);
inline constexpr ImU32 AxisX=IM_COL32(232,89,89,255);
inline constexpr ImU32 AxisY=IM_COL32(110,196,98,255);
inline constexpr ImU32 AxisZ=IM_COL32(84,149,245,255);
inline constexpr ImU32 Play=IM_COL32(86,196,136,255);
inline constexpr ImU32 Warning=IM_COL32(236,180,72,255);
inline constexpr ImU32 Error=IM_COL32(240,98,98,255);
inline constexpr ImU32 TextDim=IM_COL32(139,142,156,255);
inline constexpr ImU32 Panel=IM_COL32(24,25,30,255);
inline constexpr ImU32 Surface=IM_COL32(33,34,41,255);
}
// Loads Inter + Lucide (merged) from fontDirectory; falls back to the built-in font when missing.
// Returns false if the bundled fonts could not be loaded.
bool loadEditorFonts(const std::filesystem::path& fontDirectory);
// Rebuilds the complete style for a UI scale (safe to call repeatedly).
void applyEditorTheme(float scale);
ImVec4 toVec4(ImU32 color);
// Window-local X of the content area's right edge, for SameLine() right alignment.
float contentRight();
}
