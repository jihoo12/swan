#pragma once
#include "scene_io.hpp"
#include <variant>
namespace swan {
struct SetTransform { std::string key; Transform value; };
struct SetMaterial { std::string id; Material value; };
struct SetParent { std::string key; std::optional<std::string> parent; };
struct CreateEntity { Entity value; std::optional<std::string> parent; };
struct DeleteEntity { std::string key; };
using SceneEdit=std::variant<SetTransform,SetMaterial,SetParent,CreateEntity,DeleteEntity>;
// Frontends retain keys, not entity handles, across edits and history navigation.
class EditorDocument {
public:
    explicit EditorDocument(SceneDocument document,size_t historyLimit=64);
    const SceneDocument& document() const { return state.document; }
    const std::string& selection() const { return state.selection; }
    void select(std::string key);
    void apply(const SceneEdit& edit);
    bool undo();
    bool redo();
    bool canUndo() const { return !playing() && !undoStack.empty(); }
    bool canRedo() const { return !playing() && !redoStack.empty(); }
    void startPlay();
    void stopPlay();
    bool playing() const { return playScene.has_value(); }
    SceneDocument& runtime();
    void save(const std::filesystem::path& path) const;
    void load(const std::filesystem::path& path);
private:
    struct State { SceneDocument document; std::string selection; };
    void requireEditing() const;
    State state;
    size_t historyLimit;
    std::vector<State> undoStack,redoStack;
    std::optional<SceneDocument> playScene;
};
}
