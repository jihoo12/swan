#pragma once
#include "scene_io.hpp"
#include <span>
#include <variant>
namespace swan {
struct SetTransform { std::string key; Transform value; };
struct SetEntityProperties { std::string key,name,meshId,materialId; bool solid=false,collectible=false,goal=false; };
struct SetMaterial { std::string id; Material value; };
struct CreateMaterial { std::string id; Material value; };
struct SetParent { std::string key; std::optional<std::string> parent; };
struct CreateEntity { Entity value; std::optional<std::string> parent; };
struct DeleteEntity { std::string key; };
using SceneEdit=std::variant<SetTransform,SetEntityProperties,SetMaterial,CreateMaterial,SetParent,CreateEntity,DeleteEntity>;
// Human-readable history label, e.g. "Move Crystal pedestal", resolved against the pre-edit scene.
std::string describe(const SceneEdit& edit,const Scene& scene);
// Frontends retain keys, not entity handles, across edits and history navigation.
class EditorDocument {
public:
    explicit EditorDocument(SceneDocument document,size_t historyLimit=64);
    // The visible document: the uncommitted preview while one exists, else the committed state.
    const SceneDocument& document() const { return preview ? preview->document : state.document; }
    const std::string& selection() const { return state.selection; }
    void select(std::string key);
    // One undoable transaction; an empty label uses describe(). Commits any pending preview first.
    void apply(const SceneEdit& edit,std::string label={});
    void apply(std::span<const SceneEdit> edits,std::string label);
    // Continuous edits (drags, typing) replace one uncommitted preview, then commit as one step.
    void showPreview(const SceneEdit& edit,std::string label={});
    bool previewing() const { return preview.has_value(); }
    // Returns false when nothing was pending or the preview matched the committed state.
    bool commitPreview();
    void cancelPreview() { preview.reset(); }
    // Undo first discards an uncommitted preview, then walks committed history.
    bool undo();
    bool redo();
    bool canUndo() const { return !playing() && (previewing() || !undoStack.empty()); }
    bool canRedo() const { return !playing() && !previewing() && !redoStack.empty(); }
    std::string undoLabel() const { return undoStack.empty() ? std::string() : undoStack.back().label; }
    std::string redoLabel() const { return redoStack.empty() ? std::string() : redoStack.back().label; }
    // Oldest first: labels of committed edits that undo/redo would revert/reapply.
    std::vector<std::string> undoHistory() const;
    std::vector<std::string> redoHistory() const;
    // True when the committed document differs from the last saved/loaded revision.
    bool modified() const { return state.revision!=savedRevision; }
    void startPlay();
    void stopPlay();
    bool playing() const { return playScene.has_value(); }
    SceneDocument& runtime();
    void save(const std::filesystem::path& path);
    void load(const std::filesystem::path& path);
    // Replace the whole document (e.g. File > New); clears history and counts as unmodified.
    void reset(SceneDocument document);
private:
    struct State { SceneDocument document; std::string selection; uint64_t revision=0; };
    struct Entry { State state; std::string label; };
    struct Preview { SceneDocument document; std::string label; };
    void requireEditing() const;
    void commit(State next,std::string label);
    State state;
    size_t historyLimit;
    uint64_t nextRevision=1,savedRevision=0;
    std::vector<Entry> undoStack,redoStack;
    std::optional<Preview> preview;
    std::optional<SceneDocument> playScene;
};
}
