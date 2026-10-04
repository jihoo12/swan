#pragma once
#include <imgui.h>
#include <array>
#include <functional>
#include <string>
#include <vector>
namespace swan {
// One editor command: menus, keyboard shortcuts, and the command palette all run the same action.
struct EditorAction {
    std::string id,label,icon,category;
    ImGuiKeyChord shortcut=0,alternate=0;
    std::function<void()> run;
    std::function<bool()> enabled;   // Empty: always enabled.
    std::function<bool()> checked;   // Non-empty: shown as a toggle.
    bool repeat=false;               // Fire again while the shortcut is held (undo/redo).
};
std::string shortcutText(ImGuiKeyChord chord);
class ActionRegistry {
public:
    void add(EditorAction action);
    const EditorAction* find(const std::string& id) const;
    bool isEnabled(const EditorAction& action) const { return !action.enabled || action.enabled(); }
    bool run(const std::string& id);
    // Plain-key shortcuts are skipped while a text field has focus; Ctrl chords still apply.
    void dispatchShortcuts();
    void menuItem(const std::string& id);
    const std::vector<EditorAction>& all() const { return actions; }
private:
    std::vector<EditorAction> actions;
};
// Extra searchable entries supplied per frame (entities, recent files, ...).
struct PaletteItem { std::string label,detail,icon; std::function<void()> run; };
// Ctrl+K launcher: fuzzy search over actions and contextual items, run with Enter.
class CommandPalette {
public:
    void open() { requested=true; }
    void draw(ActionRegistry& actions,const std::vector<PaletteItem>& extra);
private:
    bool requested=false,focusInput=false;
    std::array<char,256> query{};
    int selected=0;
};
}
