#include "editor_theme.hpp"
#include <imgui_internal.h>
namespace swan {
ImVec4 toVec4(ImU32 color) {return ImGui::ColorConvertU32ToFloat4(color);}
float contentRight() {
    auto* window=ImGui::GetCurrentWindow();
    return window->ContentRegionRect.Max.x-window->Pos.x;
}
bool loadEditorFonts(const std::filesystem::path& directory) {
    auto& io=ImGui::GetIO();
    auto ui=directory/"ui.ttf",icons=directory/"icons.ttf";
    if(!std::filesystem::exists(ui) || !std::filesystem::exists(icons)) {io.Fonts->AddFontDefault();return false;}
    // Inter ships its own Private Use Area glyphs; exclude them so the merged icon font wins there.
    static const ImWchar privateUse[]={0xE000,0xF8FF,0};
    ImFontConfig text;text.OversampleH=2;text.GlyphExcludeRanges=privateUse;
    if(!io.Fonts->AddFontFromFileTTF(ui.string().c_str(),15.0f,&text)) {io.Fonts->AddFontDefault();return false;}
    // Merge icons into the text font so labels can mix both, e.g. " Save".
    ImFontConfig glyphs;glyphs.MergeMode=true;glyphs.GlyphOffset={0,2.5f};glyphs.GlyphMinAdvanceX=16;glyphs.PixelSnapH=true;
    return io.Fonts->AddFontFromFileTTF(icons.string().c_str(),15.0f,&glyphs)!=nullptr;
}
void applyEditorTheme(float scale) {
    ImGuiStyle style;
    ImGui::StyleColorsDark(&style);
    style.WindowPadding={10,10};style.FramePadding={8,5};style.CellPadding={6,4};style.ItemSpacing={8,6};style.ItemInnerSpacing={6,4};
    style.IndentSpacing=14;style.ScrollbarSize=11;style.GrabMinSize=9;
    style.WindowRounding=8;style.ChildRounding=6;style.FrameRounding=6;style.PopupRounding=8;style.ScrollbarRounding=8;style.GrabRounding=4;style.TabRounding=6;
    style.WindowBorderSize=0;style.ChildBorderSize=0;style.PopupBorderSize=1;style.FrameBorderSize=0;style.TabBorderSize=0;style.TabBarBorderSize=1;style.TabBarOverlineSize=2;
    style.WindowTitleAlign={0.5f,0.5f};style.WindowMenuButtonPosition=ImGuiDir_None;style.SeparatorTextBorderSize=1;style.SeparatorTextPadding={0,4};
    style.DockingSeparatorSize=3;style.DisabledAlpha=0.45f;style.TreeLinesFlags=ImGuiTreeNodeFlags_DrawLinesToNodes;style.TreeLinesSize=1;style.TabCloseButtonMinWidthSelected=0;style.TabCloseButtonMinWidthUnselected=0;
    auto* c=style.Colors;
    auto rgb=[](int r,int g,int b,float a=1){return ImVec4(r/255.0f,g/255.0f,b/255.0f,a);};
    c[ImGuiCol_Text]=rgb(230,231,236);c[ImGuiCol_TextDisabled]=rgb(110,113,126);
    c[ImGuiCol_WindowBg]=rgb(24,25,30);c[ImGuiCol_ChildBg]=rgb(24,25,30,0);c[ImGuiCol_PopupBg]=rgb(30,31,38,0.98f);
    c[ImGuiCol_Border]=rgb(48,50,60);c[ImGuiCol_BorderShadow]=rgb(0,0,0,0);
    c[ImGuiCol_FrameBg]=rgb(36,37,45);c[ImGuiCol_FrameBgHovered]=rgb(44,46,56);c[ImGuiCol_FrameBgActive]=rgb(50,52,64);
    c[ImGuiCol_TitleBg]=rgb(19,20,24);c[ImGuiCol_TitleBgActive]=rgb(19,20,24);c[ImGuiCol_TitleBgCollapsed]=rgb(19,20,24);
    c[ImGuiCol_MenuBarBg]=rgb(19,20,24);
    c[ImGuiCol_ScrollbarBg]=rgb(0,0,0,0);c[ImGuiCol_ScrollbarGrab]=rgb(58,60,72);c[ImGuiCol_ScrollbarGrabHovered]=rgb(72,75,90);c[ImGuiCol_ScrollbarGrabActive]=rgb(124,140,255);
    c[ImGuiCol_CheckMark]=rgb(150,163,255);c[ImGuiCol_SliderGrab]=rgb(124,140,255);c[ImGuiCol_SliderGrabActive]=rgb(150,163,255);
    c[ImGuiCol_Button]=rgb(40,42,51);c[ImGuiCol_ButtonHovered]=rgb(52,54,66);c[ImGuiCol_ButtonActive]=rgb(62,65,80);
    c[ImGuiCol_Header]=rgb(124,140,255,0.22f);c[ImGuiCol_HeaderHovered]=rgb(124,140,255,0.14f);c[ImGuiCol_HeaderActive]=rgb(124,140,255,0.30f);
    c[ImGuiCol_Separator]=rgb(42,44,53);c[ImGuiCol_SeparatorHovered]=rgb(124,140,255,0.6f);c[ImGuiCol_SeparatorActive]=rgb(124,140,255);
    c[ImGuiCol_ResizeGrip]=rgb(0,0,0,0);c[ImGuiCol_ResizeGripHovered]=rgb(124,140,255,0.5f);c[ImGuiCol_ResizeGripActive]=rgb(124,140,255);
    c[ImGuiCol_InputTextCursor]=rgb(150,163,255);
    c[ImGuiCol_Tab]=rgb(19,20,24);c[ImGuiCol_TabHovered]=rgb(36,37,45);c[ImGuiCol_TabSelected]=rgb(24,25,30);c[ImGuiCol_TabSelectedOverline]=rgb(124,140,255);
    c[ImGuiCol_TabDimmed]=rgb(19,20,24);c[ImGuiCol_TabDimmedSelected]=rgb(24,25,30);c[ImGuiCol_TabDimmedSelectedOverline]=rgb(72,78,110);
    c[ImGuiCol_DockingPreview]=rgb(124,140,255,0.45f);c[ImGuiCol_DockingEmptyBg]=rgb(15,16,19);
    c[ImGuiCol_TableHeaderBg]=rgb(30,31,38);c[ImGuiCol_TableBorderStrong]=rgb(42,44,53);c[ImGuiCol_TableBorderLight]=rgb(36,37,45);
    c[ImGuiCol_TableRowBg]=rgb(0,0,0,0);c[ImGuiCol_TableRowBgAlt]=rgb(255,255,255,0.018f);
    c[ImGuiCol_TextSelectedBg]=rgb(124,140,255,0.35f);c[ImGuiCol_DragDropTarget]=rgb(150,163,255);c[ImGuiCol_DragDropTargetBg]=rgb(124,140,255,0.12f);
    c[ImGuiCol_NavCursor]=rgb(124,140,255);c[ImGuiCol_TreeLines]=rgb(58,60,72);c[ImGuiCol_ModalWindowDimBg]=rgb(8,8,12,0.55f);
    style.ScaleAllSizes(scale);style.FontScaleMain=scale;
    ImGui::GetStyle()=style;
}
}
