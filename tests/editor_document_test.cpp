#include "editor_document.hpp"
#include "garden.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
template<class F> void rejects(F f) {bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed,"Invalid editor operation accepted");}
int main() {
    auto path=std::filesystem::temp_directory_path()/std::filesystem::path("swan-editor-"+std::to_string(getpid())+".json");
    try {
        auto garden=swan::makeGarden();swan::EditorDocument editor({garden.scene,garden.spawn},2);
        auto key=garden.scene.get(garden.core)->key;editor.select(key);
        auto original=editor.document().scene.get(editor.document().scene.find(key))->transform;
        auto moved=original;moved.position.x=5;editor.apply(swan::SetTransform{key,moved});
        require(editor.canUndo() && editor.document().scene.worldTransform(editor.document().scene.find(key)).position.x==5,"Transform edit failed");
        auto bad=moved;bad.scale.x=-1;rejects([&]{editor.apply(swan::SetTransform{key,bad});});
        require(editor.document().scene.worldTransform(editor.document().scene.find(key)).position.x==5,"Failed edit changed document");
        require(editor.undo() && editor.document().scene.worldTransform(editor.document().scene.find(key)).position.x==original.position.x,"Undo failed");
        require(!editor.canUndo() && editor.canRedo(),"Failed edit added history");
        rejects([&]{editor.apply(swan::SetMaterial{"default",{{1,1,1},0,"missing"}});});
        require(editor.canRedo(),"Failed edit destroyed redo history");
        require(editor.redo(),"Redo failed");
        editor.apply(swan::DeleteEntity{key});require(editor.selection().empty(),"Deleted selection retained");
        require(editor.undo() && editor.selection()==key,"Undo did not restore selection/entity");
        swan::Entity parent;parent.key="group";editor.apply(swan::CreateEntity{parent,{}});
        require(editor.selection()=="group" && !editor.canRedo(),"Creation/redo branch incorrect");
        editor.apply(swan::SetParent{key,"group"});
        rejects([&]{editor.apply(swan::SetParent{"group",key});});
        auto nonuniform=parent.transform;nonuniform.scale.x=2;
        rejects([&]{editor.apply(swan::SetTransform{"group",nonuniform});});
        require(editor.document().scene.parent(editor.document().scene.find(key)).has_value(),"Failed parent edit changed tree");
        auto material=editor.document().scene.assets().get("default");material.color={.1f,.2f,.3f};
        editor.apply(swan::SetMaterial{"default",material});
        require(editor.document().scene.assets().get("default").color==material.color,"Material edit failed");
        require(editor.undo() && editor.undo() && !editor.undo(),"History bound not enforced");
        editor.startPlay();auto authoredCount=editor.document().scene.size();
        editor.runtime().scene.destroy(editor.runtime().scene.find(key));editor.runtime().spawn.x=99;
        require(editor.document().scene.size()==authoredCount && editor.document().spawn.x!=99,"Play mutated authored data");
        rejects([&]{editor.apply(swan::DeleteEntity{key});});rejects([&]{editor.undo();});rejects([&]{editor.startPlay();});
        editor.save(path);require(swan::loadScene(path).scene.size()==authoredCount,"Save captured transient play state");
        editor.stopPlay();rejects([&]{editor.runtime();});editor.startPlay();
        require(editor.runtime().scene.size()==authoredCount,"New play session retained mutations");editor.stopPlay();
        {std::ofstream output(path);output<<"broken";}
        rejects([&]{editor.load(path);});require(editor.document().scene.size()==authoredCount,"Failed load changed authored data");
        editor.save(path);editor.load(path);
        require(!editor.canUndo() && !editor.canRedo() && editor.selection().empty(),"Load retained old history");
        std::filesystem::remove(path);std::cout<<"Atomic edits, bounded undo/redo, selection and play isolation passed\n";
    } catch(const std::exception& error) {std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;}
}
