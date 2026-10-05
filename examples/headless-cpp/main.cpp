// A program outside the engine tree using Swan's headless SDK: edit a scene through undoable
// commands, run it without a window (behaviour scripts included), drive it with Lua, and preview
// particle effects.
#include <swan/swan.hpp>
#include <iostream>
int main(int argc,char** argv) {
    try {
        std::filesystem::path scene=argc>1?argv[1]:SWAN_SCENES "/scripted-garden.swan.json";
        // 1. Authoring: transactional edits with undo, exactly what the editor uses.
        swan::EditorDocument document(swan::loadScene(scene));
        swan::Entity marker;marker.key="marker";marker.name="Marker";marker.transform.position={0,3,0};
        document.apply(swan::CreateEntity{marker,{}});
        std::cout<<"edited: "<<document.undoLabel()<<", "<<document.document().scene.size()<<" entities\n";
        // 2. Simulation: fixed 120 Hz ticks, scripted input, no GPU.
        swan::Simulation simulation(document.document());
        swan::Input walk;walk.move={0,1};
        simulation.step(2.0,walk);
        auto feet=simulation.playerPosition();
        std::cout<<"after "<<simulation.time()<<" s: player at ("<<feet.x<<", "<<feet.y<<", "<<feet.z<<"), "
                 <<simulation.collected()<<"/"<<simulation.collectibleCount()<<" collected\n";
        for(const auto& line:simulation.takeMessages()) std::cout<<"script: "<<line<<'\n';
        // 3. Scripting: the same Lua API as `swan script`, bound to this document.
        swan::ScriptEngine lua(swan::ScriptEngine::Mode::Tool,[](const std::string& line){std::cout<<"lua: "<<line<<'\n';});
        swan::bindScriptApi(lua);
        swan::bindDocument(lua,"doc",document);
        auto result=lua.run(R"(
            local count = 0
            for _, e in ipairs(doc:entities()) do if e.script then count = count + 1 end end
            print(("%d scripted entities; marker at %s"):format(count, tostring(doc:entity("marker").position)))
            return count
        )","=example");
        if(!result.ok) {std::cerr<<result.error<<'\n';return 1;}
        // 4. Effects: attach a particle effect and preview it deterministically (no GPU).
        document.apply(swan::SetEffect{"sparkle",swan::parseEffect(R"({"emitters":[{"rate":40,"loop":true,"lifetime":1,"speed":[1,2],"spread":45}]})")});
        document.apply(swan::SetEntityEffect{"marker","sparkle"});
        swan::FxPlayer preview(document.document());
        preview.seek(1.5);
        std::cout<<"effects: "<<preview.stats().particles<<" live particles after "<<preview.time()<<" s\n";
        return result.values.front()=="0" || preview.stats().particles==0;
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
