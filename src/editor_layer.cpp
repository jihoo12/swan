#include "editor_layer.hpp"
#include <imgui.h>
#include <cstdio>
namespace swan {
EditorLayer::EditorLayer(SceneDocument initial,const Options& options):document(std::move(initial)),sourcePath(options.scenePath),thirdPerson(options.thirdPerson) {
    view.position={12,9,16};view.yaw=-2.21f;view.pitch=-0.35f;
    std::snprintf(savePath.data(),savePath.size(),"%s",options.savePath.string().c_str());
}
void EditorLayer::attempt(const std::function<void()>& action) {
    try {action();message.clear();draftKey.clear();}catch(const std::exception& error){message=error.what();}
}
void EditorLayer::drawGui() {
    auto display=ImGui::GetIO().DisplaySize;
    ImGui::SetNextWindowPos({0,0},ImGuiCond_Always);ImGui::SetNextWindowSize({300,display.y},ImGuiCond_Always);
    ImGui::Begin("Swan | Scene",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    if(!play) {
        if(ImGui::Button("Play")) attempt([&]{
            auto game=std::make_unique<Game>(gardenFromScene(document.document().scene,document.document().spawn),false);
            game->setThirdPerson(thirdPerson);document.startPlay();play=std::move(game);
        });
    } else if(ImGui::Button("Stop")) {play.reset();document.stopPlay();}
    ImGui::SameLine();ImGui::TextUnformatted(play?"Runtime":"Authoring");
    ImGui::BeginDisabled(bool(play));
    ImGui::BeginDisabled(!document.canUndo());if(ImGui::Button("Undo")) attempt([&]{document.undo();});ImGui::EndDisabled();
    ImGui::SameLine();ImGui::BeginDisabled(!document.canRedo());if(ImGui::Button("Redo")) attempt([&]{document.redo();});ImGui::EndDisabled();
    if(ImGui::Button("Add cube")) attempt([&]{document.apply(CreateEntity{Entity{}, {}});});
    ImGui::SameLine();if(ImGui::Button("Delete") && !document.selection().empty()) attempt([&]{document.apply(DeleteEntity{document.selection()});});
    ImGui::EndDisabled();
    ImGui::InputText("Save path",savePath.data(),savePath.size());
    ImGui::BeginDisabled(savePath[0]=='\0');if(ImGui::Button("Save authored scene")) attempt([&]{document.save(savePath.data());});ImGui::EndDisabled();
    ImGui::BeginDisabled(bool(play)||sourcePath.empty());if(ImGui::Button("Reload source")) attempt([&]{document.load(sourcePath);});ImGui::EndDisabled();
    ImGui::Separator();ImGui::TextUnformatted("Right-click viewport: look");ImGui::TextUnformatted("WASD: move | Esc: release");
    ImGui::Separator();
    const auto& scene=document.document().scene;
    ImGui::BeginChild("Hierarchy",{0,0});
    for(auto id:scene.entities()) {
        const auto* entity=scene.get(id);size_t depth=0;auto parent=scene.parent(id);
        while(parent) {++depth;parent=scene.parent(*parent);}
        ImGui::PushID(entity->key.c_str());if(depth) ImGui::Indent(float(depth)*12);
        std::string label=entity->name.empty()?entity->key:entity->name;
        if(ImGui::Selectable(label.c_str(),document.selection()==entity->key)) {document.select(entity->key);draftKey.clear();}
        if(depth) ImGui::Unindent(float(depth)*12);ImGui::PopID();
    }
    ImGui::EndChild();ImGui::End();
    ImGui::SetNextWindowPos({display.x-300,0},ImGuiCond_Always);ImGui::SetNextWindowSize({300,display.y},ImGuiCond_Always);
    ImGui::Begin("Inspector",nullptr,ImGuiWindowFlags_NoMove|ImGuiWindowFlags_NoResize|ImGuiWindowFlags_NoCollapse);
    auto* entity=scene.get(scene.find(document.selection()));
    if(entity) {
        if(draftKey!=entity->key) {draft=entity->transform;draftKey=entity->key;}
        ImGui::TextWrapped("ID: %s",entity->key.c_str());ImGui::TextWrapped("Mesh: %s",entity->meshId.c_str());
        ImGui::BeginDisabled(bool(play));
        ImGui::InputFloat3("Position",&draft.position.x);ImGui::InputFloat3("Scale",&draft.scale.x);ImGui::InputFloat("Yaw (radians)",&draft.yaw);
        // Apply as one transaction, so typing/dragging does not flood undo history.
        if(ImGui::Button("Apply transform")) {auto key=entity->key;attempt([&]{document.apply(SetTransform{key,draft});});}
        // Resolve again: commands replace the document and invalidate entity pointers.
        entity=scene.get(scene.find(document.selection()));
        if(entity && ImGui::BeginCombo("Material",entity->materialId.c_str())) {
            for(const auto& [id,material]:scene.assets().entries()) { (void)material;ImGui::TextUnformatted(id.c_str()); }
            ImGui::EndCombo();
        }
        ImGui::EndDisabled();
        ImGui::TextUnformatted("Materials are read-only in this first UI.");
    } else ImGui::TextUnformatted("Select an entity in the hierarchy.");
    if(!message.empty()) {ImGui::Separator();ImGui::TextWrapped("%s",message.c_str());}
    ImGui::End();
}
void EditorLayer::handleInput(const Input& input) {
    if(play) {play->handleInput(input);return;}
    view.look(input.look);
    if(input.reload && !sourcePath.empty()) attempt([&]{document.load(sourcePath);});
    if(input.save && savePath[0]) attempt([&]{document.save(savePath.data());});
}
void EditorLayer::fixedUpdate(float dt,const Input& input) {
    if(play) {play->fixedUpdate(dt,input);return;}
    auto direction=forward(view.yaw,0)*input.move.y+glm::cross(forward(view.yaw,0),glm::vec3(0,1,0))*input.move.x;
    direction.y=input.vertical;if(glm::length(direction)>1) direction=glm::normalize(direction);
    view.position+=direction*dt*(input.sprint?12.0f:5.0f);
}
RenderFrame EditorLayer::renderFrame(float interpolation) const {
    if(play) return play->renderFrame(interpolation);
    RenderFrame frame;frame.camera=view;const auto& scene=document.document().scene;
    for(auto id:scene.entities()) {const auto& entity=*scene.get(id);const auto& material=scene.assets().get(entity.materialId);
        frame.objects.push_back({scene.worldTransform(id),material,scene.meshes().get(entity.meshId).data,scene.textures().get(material.textureId).data});}
    return frame;
}
std::string EditorLayer::status() const {return play?"EDITOR | PLAY | "+play->status():"EDITOR | AUTHORING";}
}
