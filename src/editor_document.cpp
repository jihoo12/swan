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
}
EditorDocument::EditorDocument(SceneDocument document,size_t limit):state{std::move(document),{}},historyLimit(limit) {
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
void EditorDocument::apply(const SceneEdit& edit) {
    requireEditing();
    State next=state;auto& scene=next.document.scene;
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
        } else if constexpr(std::is_same_v<T,SetParent>) {
            auto id=requireEntity(scene,command.key);
            scene.setParent(id,command.parent?std::optional<EntityId>(requireEntity(scene,*command.parent)):std::nullopt);
        } else if constexpr(std::is_same_v<T,CreateEntity>) {
            auto id=scene.create(command.value);
            if(command.parent) scene.setParent(id,requireEntity(scene,*command.parent));
            next.selection=scene.get(id)->key;
        } else if constexpr(std::is_same_v<T,DeleteEntity>) scene.destroy(requireEntity(scene,command.key));
    },edit);
    validate(next.document);
    if(!next.selection.empty() && !scene.get(scene.find(next.selection))) next.selection.clear();
    // Allocate history before changing live authored state; assets remain shared.
    undoStack.push_back(state);
    if(undoStack.size()>historyLimit) undoStack.erase(undoStack.begin());
    redoStack.clear();state=std::move(next);
}
bool EditorDocument::undo() {
    requireEditing();if(undoStack.empty()) return false;
    redoStack.push_back(state);state=std::move(undoStack.back());undoStack.pop_back();return true;
}
bool EditorDocument::redo() {
    requireEditing();if(redoStack.empty()) return false;
    undoStack.push_back(state);state=std::move(redoStack.back());redoStack.pop_back();return true;
}
void EditorDocument::startPlay() {
    requireEditing();validate(state.document);playScene.emplace(state.document);
}
void EditorDocument::stopPlay() {playScene.reset();}
SceneDocument& EditorDocument::runtime() {
    if(!playing()) throw std::logic_error("No editor play session");
    return *playScene;
}
void EditorDocument::save(const std::filesystem::path& path) const {saveScene(path,state.document);}
void EditorDocument::load(const std::filesystem::path& path) {
    requireEditing();auto document=loadScene(path);validate(document);
    state={std::move(document),{}};undoStack.clear();redoStack.clear();
}
}
