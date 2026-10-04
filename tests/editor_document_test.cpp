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
        editor.select(key);
        auto before=*editor.document().scene.get(editor.document().scene.find(key));
        auto properties=swan::SetEntityProperties{key,"Renamed object",before.meshId,before.materialId,before.solid,before.collectible,before.goal};
        editor.apply(properties);
        require(editor.document().scene.get(editor.document().scene.find(key))->name=="Renamed object","Rename failed");
        auto invalid=properties;invalid.meshId="missing";
        rejects([&]{editor.apply(invalid);});
        require(editor.undo() && editor.document().scene.get(editor.document().scene.find(key))->name==before.name,"Property undo failed");
        require(editor.redo(),"Property redo failed");
        auto copy=*editor.document().scene.get(editor.document().scene.find(key));copy.key.clear();
        editor.apply(swan::CreateEntity{copy,{}});
        require(editor.selection()!=key && editor.document().scene.get(editor.document().scene.find(editor.selection()))->name==copy.name,"Duplicate identity failed");
        require(editor.undo() && editor.selection()==key,"Duplicate undo failed");
        editor.startPlay();auto authoredCount=editor.document().scene.size();
        editor.runtime().scene.destroy(editor.runtime().scene.find(key));editor.runtime().spawn.x=99;
        require(editor.document().scene.size()==authoredCount && editor.document().spawn.x!=99,"Play mutated authored data");
        rejects([&]{editor.apply(swan::DeleteEntity{key});});rejects([&]{editor.undo();});rejects([&]{editor.startPlay();});
        editor.save(path);require(swan::loadScene(path).scene.size()==authoredCount,"Save captured transient play state");
        editor.stopPlay();rejects([&]{editor.runtime();});editor.startPlay();
        require(editor.runtime().scene.size()==authoredCount,"New play session retained mutations");editor.stopPlay();
        {std::ofstream output(path);output<<"broken";}
        rejects([&]{editor.load(path);});require(editor.document().scene.size()==authoredCount,"Failed load changed authored data");
        editor.save(path);
        auto saved=swan::loadScene(path);
        require(saved.scene.get(saved.scene.find(key))->name=="Renamed object","Properties not persisted");
        editor.load(path);
        require(!editor.canUndo() && !editor.canRedo() && editor.selection().empty(),"Load retained old history");
        // Live previews: many intermediate values, one committed history step.
        swan::EditorDocument live({garden.scene,garden.spawn});
        require(!live.modified(),"Fresh document reported as modified");
        auto start=live.document().scene.get(live.document().scene.find(key))->transform;
        for(int i=1;i<=5;++i) {auto step=start;step.position.y+=float(i);live.showPreview(swan::SetTransform{key,step});}
        require(live.previewing() && live.document().scene.get(live.document().scene.find(key))->transform.position.y==start.position.y+5,"Preview not visible");
        require(!live.modified() && live.undoHistory().empty(),"Preview must not touch committed history");
        auto bad2=start;bad2.scale.y=0;rejects([&]{live.showPreview(swan::SetTransform{key,bad2});});
        require(live.document().scene.get(live.document().scene.find(key))->transform.position.y==start.position.y+5,"Invalid preview replaced the last valid one");
        require(live.commitPreview() && !live.previewing() && live.undoHistory().size()==1 && live.modified(),"Commit did not create one step");
        require(live.undoLabel().starts_with("Transform"),"Default history label missing");
        live.showPreview(swan::SetTransform{key,live.document().scene.get(live.document().scene.find(key))->transform});
        require(!live.commitPreview() && live.undoHistory().size()==1,"No-op preview created history");
        live.showPreview(swan::SetTransform{key,start});
        require(live.undo() && !live.previewing() && live.undoHistory().size()==1,"Undo must first discard a pending preview");
        require(live.undo() && !live.modified(),"Undo back to the saved revision must clear modified");
        require(live.redo() && live.modified() && live.redoHistory().empty(),"Redo did not restore modified state");
        // Multi-command transactions are atomic and undo as one step.
        auto materialId=std::string("unique-test");
        auto current=*live.document().scene.get(live.document().scene.find(key));
        std::vector<swan::SceneEdit> batch{swan::CreateMaterial{materialId,live.document().scene.assets().get(current.materialId)},
            swan::SetEntityProperties{key,current.name,current.meshId,materialId,current.solid,current.collectible,current.goal}};
        live.apply(batch,"Make unique");
        require(live.undoLabel()=="Make unique" && live.document().scene.get(live.document().scene.find(key))->materialId==materialId,"Batch edit failed");
        rejects([&]{live.apply(swan::CreateMaterial{materialId,{}});});
        std::vector<swan::SceneEdit> broken{swan::CreateMaterial{"never",{}},swan::DeleteEntity{"missing"}};
        rejects([&]{live.apply(broken,"broken");});
        require(!live.document().scene.assets().contains("never"),"Failed batch left partial changes");
        require(live.undo() && !live.document().scene.assets().contains(materialId),"Batch undo was not atomic");
        live.save(path);require(!live.modified(),"Save did not clear modified");
        live.reset({});require(live.document().scene.size()==0 && !live.canUndo() && !live.modified(),"Reset did not start a clean document");
        std::filesystem::remove(path);std::cout<<"Atomic edits, previews, batches, bounded undo/redo, selection and play isolation passed\n";
    } catch(const std::exception& error) {std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;}
}
