#include "editor_layer.hpp"
#include "editor_view.hpp"
#include <imgui.h>
#include <cstdio>
namespace swan {
EditorLayer::EditorLayer(SceneDocument initial,const Options& options):document(std::move(initial)),sourcePath(options.scenePath),thirdPerson(options.thirdPerson) {
    view.position={12,9,16};view.yaw=-2.21f;view.pitch=-0.35f;
    std::snprintf(savePath.data(),savePath.size(),"%s",(options.savePath.empty()?(sourcePath.empty()?std::filesystem::path("scene.swan.json"):sourcePath):options.savePath).string().c_str());
}
void EditorLayer::attempt(const std::function<void()>& action) {
    try {action();message.clear();draftKey.clear();materialKey.clear();}catch(const std::exception& error){message=error.what();}
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
    if(ImGui::Button("Add cube")) attempt([&]{document.apply(CreateEntity{editorCube(view), {}});search[0]='\0';});
    ImGui::SameLine();if(ImGui::Button("Delete") && !document.selection().empty()) attempt([&]{document.apply(DeleteEntity{document.selection()});});
    ImGui::EndDisabled();
    ImGui::InputText("Scene file",savePath.data(),savePath.size());
    ImGui::BeginDisabled(savePath[0]=='\0');if(ImGui::Button("Save authored scene")) attempt([&]{document.save(savePath.data());sourcePath=savePath.data();});ImGui::EndDisabled();
    ImGui::BeginDisabled(bool(play)||savePath[0]=='\0');if(ImGui::Button("Open scene")) attempt([&]{document.load(savePath.data());sourcePath=savePath.data();search[0]='\0';});ImGui::EndDisabled();
    ImGui::Separator();ImGui::TextUnformatted("Click: select | Right-click: look");ImGui::TextUnformatted("WASD: move | Esc: release");
    ImGui::Separator();
    ImGui::InputText("Filter",search.data(),search.size());
    const auto& scene=document.document().scene;
    ImGui::BeginChild("Hierarchy",{0,0});
    for(auto id:scene.entities()) {
        const auto* entity=scene.get(id);
        if(search[0] && entity->name.find(search.data())==std::string::npos && entity->key.find(search.data())==std::string::npos) continue;
        size_t depth=0;auto parent=scene.parent(id);
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
        if(draftKey!=entity->key) {draft=entity->transform;properties=*entity;std::snprintf(name.data(),name.size(),"%s",entity->name.c_str());draftKey=entity->key;}
        ImGui::TextWrapped("ID: %s",entity->key.c_str());ImGui::TextWrapped("Mesh: %s",entity->meshId.c_str());
        ImGui::BeginDisabled(bool(play));
        ImGui::InputFloat3("Position",&draft.position.x);ImGui::InputFloat3("Scale",&draft.scale.x);ImGui::InputFloat("Yaw (radians)",&draft.yaw);
        // Apply as one transaction, so typing/dragging does not flood undo history.
        if(ImGui::Button("Apply transform")) {auto key=entity->key;attempt([&]{document.apply(SetTransform{key,draft});});}
        // Commands replace the document; retain values and resolve keys again.
        entity=scene.get(scene.find(document.selection()));
        if(entity) {
            if(ImGui::Button("Focus selection")) {
                auto world=scene.worldTransform(scene.find(entity->key));
                float distance=glm::max(3.0f,glm::length(world.scale)*2.5f);
                view.position=world.position-forward(view.yaw,view.pitch)*distance;
            }
            ImGui::SameLine();
            if(ImGui::Button("Duplicate")) {
                Entity copy=*entity;copy.key.clear();copy.name+=" copy";
                auto parent=scene.parent(scene.find(entity->key));
                std::optional<std::string> parentKey;
                if(parent) parentKey=scene.get(*parent)->key;
                attempt([&]{document.apply(CreateEntity{copy,parentKey});});
            }
        }
        ImGui::Separator();
        ImGui::InputText("Name",name.data(),name.size());
        if(ImGui::BeginCombo("Mesh",properties.meshId.c_str())) {
            for(const auto& [id,asset]:scene.meshes().entries()) {
                (void)asset;if(ImGui::Selectable(id.c_str(),id==properties.meshId)) properties.meshId=id;
            }
            ImGui::EndCombo();
        }
        if(ImGui::BeginCombo("Material",properties.materialId.c_str())) {
            for(const auto& [id,value]:scene.assets().entries()) {
                (void)value;if(ImGui::Selectable(id.c_str(),id==properties.materialId)) properties.materialId=id;
            }
            ImGui::EndCombo();
        }
        ImGui::Checkbox("Solid",&properties.solid);ImGui::Checkbox("Collectible",&properties.collectible);ImGui::Checkbox("Goal",&properties.goal);
        if(ImGui::Button("Apply properties")) {
            auto value=SetEntityProperties{document.selection(),name.data(),properties.meshId,properties.materialId,properties.solid,properties.collectible,properties.goal};
            attempt([&]{document.apply(value);});
        }
        entity=scene.get(scene.find(document.selection()));
        if(entity) {
            auto key=entity->key;
            auto parent=scene.parent(scene.find(key));
            std::string parentKey=parent?scene.get(*parent)->key:"";
            if(ImGui::BeginCombo("Parent",parentKey.empty()?"None":parentKey.c_str())) {
                std::optional<SetParent> change;
                if(ImGui::Selectable("None",parentKey.empty())) change=SetParent{key,{}};
                for(auto id:scene.entities()) {
                    const auto& candidate=*scene.get(id);
                    if(candidate.key!=key && ImGui::Selectable(candidate.key.c_str(),candidate.key==parentKey)) change=SetParent{key,candidate.key};
                }
                ImGui::EndCombo();
                if(change) attempt([&]{document.apply(*change);});
            }
        }
        entity=scene.get(scene.find(document.selection()));
        if(entity) {
            std::string id=entity->materialId;
            if(materialKey!=id) {materialDraft=scene.assets().get(id);materialKey=id;}
            ImGui::Separator();ImGui::TextWrapped("Shared material: %s",id.c_str());
            ImGui::TextWrapped("Changes affect every object using this material.");
            ImGui::ColorEdit3("Color",&materialDraft.color.x);
            ImGui::InputFloat("Emission",&materialDraft.emission);
            ImGui::InputFloat2("UV scale",&materialDraft.uvScale.x);
            if(ImGui::BeginCombo("Texture",materialDraft.textureId.c_str())) {
                for(const auto& [texture,value]:scene.textures().entries()) {
                    (void)value;if(ImGui::Selectable(texture.c_str(),texture==materialDraft.textureId)) materialDraft.textureId=texture;
                }
                ImGui::EndCombo();
            }
            if(ImGui::Button("Apply material")) attempt([&]{document.apply(SetMaterial{id,materialDraft});});
        }
        ImGui::EndDisabled();
    } else ImGui::TextUnformatted("Click an object or select it in the hierarchy.");
    if(!message.empty()) {ImGui::Separator();ImGui::TextWrapped("%s",message.c_str());}
    ImGui::End();
    const auto& io=ImGui::GetIO();
    if(!play && !io.WantCaptureMouse && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && io.MousePos.x>=300 && io.MousePos.x<display.x-300 && io.MousePos.y>=0 && io.MousePos.y<display.y) {
        document.select(pickEntity(scene,view,{io.MousePos.x,io.MousePos.y},{display.x,display.y}));draftKey.clear();
    }
    auto selected=scene.get(scene.find(document.selection()));
    if(!play && selected) {
        auto world=scene.worldTransform(scene.find(selected->key));
        auto clip=view.viewProjection(display.x/display.y)*glm::vec4(world.position,1);
        if(clip.w>0) {
            auto position=(glm::vec2(clip)/clip.w+1.0f)*0.5f*glm::vec2(display.x,display.y);
            if(position.x>300 && position.x<display.x-300) {
                auto* draw=ImGui::GetForegroundDrawList();
                draw->AddCircle({position.x,position.y},12,IM_COL32(110,210,255,255),24,2);
                draw->AddText({position.x+16,position.y},IM_COL32(180,230,255,255),selected->name.empty()?selected->key.c_str():selected->name.c_str());
            }
        }
    }
}
void EditorLayer::handleInput(const Input& input) {
    if(play) {play->handleInput(input);return;}
    view.look(input.look);
    if(input.reload && !sourcePath.empty()) attempt([&]{document.load(sourcePath);});
    if(input.save && savePath[0]) attempt([&]{document.save(savePath.data());sourcePath=savePath.data();});
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
