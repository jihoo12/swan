#include "editor_document.hpp"
#include <cmath>
#include <stdexcept>
#include <type_traits>
namespace swan {
namespace {
EntityId requireEntity(const Scene& scene,const std::string& key) {
    auto id=scene.find(key);
    if(!scene.get(id)) throw std::invalid_argument("Unknown editor entity: "+key);
    return id;
}
void validate(const SceneDocument& document) {
    document.scene.validate();
    for(const auto& [id,material]:document.scene.assets().entries()) {
        (void)id;
        if(!document.scene.textures().contains(material.textureId)) throw std::invalid_argument("Unknown editor texture: "+material.textureId);
    }
    for(int i=0;i<3;++i) if(!std::isfinite(document.spawn[i])) throw std::invalid_argument("Invalid editor spawn");
}
std::string displayName(const Scene& scene,const std::string& key) {
    const auto* entity=scene.get(scene.find(key));
    return entity && !entity->name.empty()?entity->name:key;
}
// Applies one command to a candidate state; the caller validates and commits.
void applyEdit(SceneDocument& document,std::string& selection,const SceneEdit& edit) {
    auto& scene=document.scene;
    std::visit([&](const auto& command) {
        using T=std::decay_t<decltype(command)>;
        if constexpr(std::is_same_v<T,SetTransform>) scene.get(requireEntity(scene,command.key))->transform=command.value;
        else if constexpr(std::is_same_v<T,SetEntityProperties>) {
            auto* entity=scene.get(requireEntity(scene,command.key));
            entity->name=command.name;entity->meshId=command.meshId;entity->materialId=command.materialId;
            entity->solid=command.solid;entity->collectible=command.collectible;entity->goal=command.goal;
        } else if constexpr(std::is_same_v<T,SetMaterial>) {
            if(!scene.assets().contains(command.id)) throw std::invalid_argument("Unknown editor material: "+command.id);
            scene.assets().set(command.id,command.value);
        } else if constexpr(std::is_same_v<T,CreateMaterial>) {
            if(scene.assets().contains(command.id)) throw std::invalid_argument("Material already exists: "+command.id);
            scene.assets().set(command.id,command.value);
        } else if constexpr(std::is_same_v<T,SetParent>) {
            auto id=requireEntity(scene,command.key);
            scene.setParent(id,command.parent?std::optional<EntityId>(requireEntity(scene,*command.parent)):std::nullopt);
        } else if constexpr(std::is_same_v<T,CreateEntity>) {
            auto id=scene.create(command.value);
            if(command.parent) scene.setParent(id,requireEntity(scene,*command.parent));
            selection=scene.get(id)->key;
        } else if constexpr(std::is_same_v<T,DeleteEntity>) {
            scene.destroy(requireEntity(scene,command.key));
            scene.timeline().removeEntity(command.key); // Tracks/events on a deleted entity go with it.
        }
        else if constexpr(std::is_same_v<T,SetEntityScript>) {
            auto* entity=scene.get(requireEntity(scene,command.key));
            entity->scriptId=command.scriptId;entity->properties=command.properties;
        } else if constexpr(std::is_same_v<T,AddScript>) scene.scripts().load(command.id,command.path);
        else if constexpr(std::is_same_v<T,SetEffect>) {
            if(command.value) scene.effects().set(command.id,*command.value);
            else {
                if(!scene.effects().erase(command.id)) throw std::invalid_argument("Unknown editor effect: "+command.id);
                for(auto id:scene.entities()) if(scene.get(id)->effectId==command.id) scene.get(id)->effectId.clear();
                scene.timeline().removeEffect(command.id);
            }
        } else if constexpr(std::is_same_v<T,SetEntityEffect>) {
            if(!command.effectId.empty() && !scene.effects().contains(command.effectId)) throw std::invalid_argument("Unknown editor effect: "+command.effectId);
            scene.get(requireEntity(scene,command.key))->effectId=command.effectId;
        } else if constexpr(std::is_same_v<T,SetTimeline>) scene.timeline()=command.value;
        else if constexpr(std::is_same_v<T,SetEnvironment>) scene.setEnvironment(command.value);
    },edit);
}
}
std::string describe(const SceneEdit& edit,const Scene& scene) {
    return std::visit([&](const auto& command)->std::string {
        using T=std::decay_t<decltype(command)>;
        if constexpr(std::is_same_v<T,SetTransform>) return "Transform "+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,SetEntityProperties>) return "Edit "+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,SetMaterial>) return "Edit material "+command.id;
        else if constexpr(std::is_same_v<T,CreateMaterial>) return "Create material "+command.id;
        else if constexpr(std::is_same_v<T,SetParent>) return "Reparent "+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,CreateEntity>) return "Create "+(command.value.name.empty()?std::string("entity"):command.value.name);
        else if constexpr(std::is_same_v<T,DeleteEntity>) return "Delete "+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,SetEntityScript>) return "Edit script of "+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,AddScript>) return "Add script "+command.id;
        else if constexpr(std::is_same_v<T,SetEffect>) return (command.value?(scene.effects().contains(command.id)?"Edit effect ":"Create effect "):"Delete effect ")+command.id;
        else if constexpr(std::is_same_v<T,SetEntityEffect>) return (command.effectId.empty()?"Remove effect from ":"Set effect of ")+displayName(scene,command.key);
        else if constexpr(std::is_same_v<T,SetTimeline>) return "Edit timeline";
        else return "Edit environment";
    },edit);
}
EditorDocument::EditorDocument(SceneDocument document,size_t limit):state{std::move(document),{},0},historyLimit(limit) {
    if(limit==0 || limit>256) throw std::invalid_argument("Editor history limit must be 1..256");
    validate(state.document);
}
void EditorDocument::requireEditing() const {
    if(playing()) throw std::logic_error("Stop play before editing authored data");
}
void EditorDocument::select(std::string key) {
    if(!key.empty()) requireEntity(state.document.scene,key);
    state.selection=std::move(key);
}
void EditorDocument::commit(State next,std::string label) {
    auto& scene=next.document.scene;
    if(!next.selection.empty() && !scene.get(scene.find(next.selection))) next.selection.clear();
    next.revision=nextRevision++;
    if(groupDepth>0) {
        // Later edits in a group extend the step the first edit created.
        if(groupCommitted) {redoStack.clear();state=std::move(next);return;}
        groupCommitted=true;label=groupLabel;
    }
    // Allocate history before changing live authored state; assets remain shared.
    undoStack.push_back({state,std::move(label)});
    if(undoStack.size()>historyLimit) undoStack.erase(undoStack.begin());
    redoStack.clear();state=std::move(next);
}
void EditorDocument::apply(const SceneEdit& edit,std::string label) {
    apply(std::span<const SceneEdit>(&edit,1),std::move(label));
}
void EditorDocument::apply(std::span<const SceneEdit> edits,std::string label) {
    requireEditing();
    if(edits.empty()) return;
    commitPreview();
    if(label.empty()) label=describe(edits.front(),state.document.scene);
    State next=state;
    for(const auto& edit:edits) applyEdit(next.document,next.selection,edit);
    validate(next.document);
    commit(std::move(next),std::move(label));
}
void EditorDocument::showPreview(const SceneEdit& edit,std::string label) {
    requireEditing();
    // Always derive from committed state, so a preview never compounds on an earlier preview.
    Preview next{state.document,label.empty()?describe(edit,state.document.scene):std::move(label)};
    std::string selection=state.selection;
    applyEdit(next.document,selection,edit);
    validate(next.document);
    preview=std::move(next);
}
bool EditorDocument::commitPreview() {
    if(!preview) return false;
    requireEditing();
    auto value=std::move(*preview);preview.reset();
    // A drag that ends where it started (or a reverted text edit) leaves no history entry.
    if(serializeScene(value.document)==serializeScene(state.document)) return false;
    commit({std::move(value.document),state.selection,0},std::move(value.label));
    return true;
}
void EditorDocument::beginGroup(std::string label) {
    if(groupDepth++==0) {commitPreview();groupCommitted=false;groupLabel=std::move(label);}
}
void EditorDocument::endGroup() {
    if(groupDepth==0) throw std::logic_error("endGroup() without beginGroup()");
    if(--groupDepth==0) groupCommitted=false;
}
bool EditorDocument::undo() {
    requireEditing();
    if(preview) {preview.reset();return true;}
    if(undoStack.empty()) return false;
    groupCommitted=false; // A later edit in an open group starts a new step.
    auto entry=std::move(undoStack.back());undoStack.pop_back();
    redoStack.push_back({std::move(state),entry.label});state=std::move(entry.state);return true;
}
bool EditorDocument::redo() {
    requireEditing();
    if(preview || redoStack.empty()) return false;
    groupCommitted=false;
    auto entry=std::move(redoStack.back());redoStack.pop_back();
    undoStack.push_back({std::move(state),entry.label});state=std::move(entry.state);return true;
}
std::vector<std::string> EditorDocument::undoHistory() const {
    std::vector<std::string> labels;
    for(const auto& entry:undoStack) labels.push_back(entry.label);
    return labels;
}
std::vector<std::string> EditorDocument::redoHistory() const {
    std::vector<std::string> labels;
    for(auto it=redoStack.rbegin();it!=redoStack.rend();++it) labels.push_back(it->label);
    return labels;
}
void EditorDocument::startPlay() {
    requireEditing();commitPreview();validate(state.document);playScene.emplace(state.document);
}
void EditorDocument::stopPlay() {playScene.reset();}
SceneDocument& EditorDocument::runtime() {
    if(!playing()) throw std::logic_error("No editor play session");
    return *playScene;
}
void EditorDocument::save(const std::filesystem::path& path) {
    // Saving writes committed data only; an in-progress drag is not part of the file.
    saveScene(path,state.document);savedRevision=state.revision;
}
void EditorDocument::load(const std::filesystem::path& path) {
    requireEditing();auto document=loadScene(path);validate(document);
    reset(std::move(document));
}
void EditorDocument::reset(SceneDocument document) {
    requireEditing();validate(document);
    preview.reset();state={std::move(document),{},nextRevision++};savedRevision=state.revision;
    undoStack.clear();redoStack.clear();groupCommitted=false;
}
}
