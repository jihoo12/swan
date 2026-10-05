#include "fx_player.hpp"
#include "garden.hpp"
#include "script_api.hpp"
#include "script_runtime.hpp"
#include "simulation.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <unistd.h>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
bool contains(const std::vector<std::string>& lines,const std::string& text) {
    for(const auto& line:lines) if(line.find(text)!=std::string::npos) return true;
    return false;
}
swan::Garden scriptedGarden(const std::map<std::string,std::string>& scripts) {
    swan::Scene scene;
    for(const auto& [id,code]:scripts) scene.scripts().set(id,code);
    swan::Entity floor;floor.key="floor";floor.solid=true;floor.transform={{0,-0.5f,0},{40,1,40},0};scene.create(floor);
    return swan::gardenFromScene(std::move(scene),{0,0.2f,0});
}
void tick(swan::Game& game,int ticks) {for(int i=0;i<ticks;++i) game.fixedUpdate(1.0f/120,{});}
void engineSandbox() {
    std::vector<std::string> printed;
    swan::ScriptEngine engine(swan::ScriptEngine::Mode::Sandbox,[&](const std::string& line){printed.push_back(line);},{8u<<20,200'000});
    auto check=engine.run("return io, os, require, dofile, loadfile, debug, package, string.dump","=test");
    require(check.ok && check.values==std::vector<std::string>(8,"nil"),"Sandbox exposes unsafe libraries");
    require(!engine.run("return load('\\27Lua')()","=test").ok,"Sandbox accepted a binary chunk");
    require(engine.run("print('hello', 1, true, vec3(1, 2, 3))","=test").ok && printed.back()=="hello\t1\ttrue\tvec3(1, 2, 3)","print sink");
    auto loop=engine.run("while true do end","=test");
    require(!loop.ok && loop.error.find("instruction budget")!=std::string::npos,"Infinite loop not stopped");
    auto memory=engine.run("local t = {} for i = 1, 1e7 do t[i] = string.rep('x', 64) .. i end","=test");
    require(!memory.ok,"Memory cap not enforced");
    require(engine.run("return 1 + 1","=test").ok,"Engine unusable after errors");
    auto math=engine.run("local v = (vec3(1, 2, 3) + vec3{1, 1, 1}) * 2; return v.x, v:length() > 0, vec3(1,0,0):cross(vec3(0,1,0)).z, -vec2(1, 2) == vec2(-1, -2)","=test",true);
    require(math.ok && math.values[0]=="4.0" && math.values[1]=="true" && math.values[2]=="1.0" && math.values[3]=="true","vec math");
    auto bad=engine.run("return vec3(1, 2)","=test");
    require(!bad.ok && bad.error.find("vec3 takes")!=std::string::npos,"vec3 argument validation");
    auto echo=engine.run("1 + 2","=repl",true);
    require(echo.ok && echo.values.front()=="3","REPL echo");
}
void behaviours() {
    auto garden=scriptedGarden({
        {"spin","local S = { properties = { speed = 1 } } function S:update(dt) self.entity.yaw = self.entity.yaw + self.speed * dt end return S"},
        {"broken","local B = {} function B:update(dt) error('boom') end return B"},
        {"syntax","return {"},
        {"loop","local L = {} function L:update() while true do end end return L"},
        {"spawner","local P = {} function P:start() self.child = game.spawn{ name = 'Child', script = 'spin', position = vec3(0, 2, 0), properties = { speed = 4 } } end\n"
                   "function P:update() if game.time > 0.5 and self.child and self.child.alive then self.child:destroy(); game.message('child gone') end end return P"},
        {"pickup","local P = {} function P:collected() game.message('picked ' .. self.entity.key) end return P"},
    });
    auto add=[&](std::string key,std::string script,swan::ScriptProperties props={},bool collectible=false) {
        swan::Entity e;e.key=key;e.scriptId=script;e.properties=props;e.collectible=collectible;e.transform.position={0,1,-1};
        return garden.scene.create(e);
    };
    add("fast","spin",{{"speed",2.0}});
    add("default","spin");
    add("broken","broken");add("syntax","syntax");add("loop","loop");add("spawner","spawner");
    add("gem","pickup",{},true);
    garden=swan::gardenFromScene(garden.scene,garden.spawn);
    swan::Game game(std::move(garden),false);
    tick(game,120);
    auto& scene=game.scene();
    float fast=scene.get(scene.find("fast"))->transform.yaw,slow=scene.get(scene.find("default"))->transform.yaw;
    require(std::abs(fast-2)<0.05f && std::abs(slow-1)<0.05f,"Property overrides/defaults or update timing");
    auto messages=game.takeMessages();
    require(contains(messages,"broken") && contains(messages,"boom"),"Runtime error not reported");
    require(contains(messages,"syntax"),"Syntax error not reported");
    require(contains(messages,"instruction budget"),"Runaway update not stopped");
    require(contains(messages,"child gone") && !scene.get(scene.find("Child")),"spawn/destroy");
    require(game.scripts()->failureCount()==3,"Unexpected failure count");
    tick(game,10); // Failed instances stay stopped; the game keeps running.
    require(game.takeMessages().empty(),"Stopped scripts reported again");
    game.handleInput(swan::Input{.interact=true});
    require(game.collected()==1 && contains(game.takeMessages(),"picked gem"),"collected() hook");
}
void simulation() {
    auto scene=swan::loadScene(std::string(SWAN_TEST_ASSET_DIR)+"/scenes/scripted-garden.swan.json");
    swan::Simulation a(scene),b(scene);
    swan::Input walk;walk.move={0.3f,1};
    a.step(1.5,walk);b.step(1.5,walk);
    require(a.ticks()==180 && a.playerPosition()==b.playerPosition(),"Simulation is not deterministic");
    require(glm::length(a.playerPosition()-scene.spawn)>3,"Simulation did not move the player");
    require(a.frame().objects.size()>=scene.scene.size(),"Render snapshot incomplete");
    auto pedestal=a.scene().get(a.scene().find("pedestal"))->transform.scale;
    a.step(0.4);
    require(a.scene().get(a.scene().find("pedestal"))->transform.scale!=pedestal,"Scene behaviours did not run headlessly");
    bool rejected=false;try {a.step(-1);} catch(const std::invalid_argument&) {rejected=true;}
    require(rejected,"Negative step accepted");
}
void toolApi() {
    auto garden=swan::makeGarden();
    swan::EditorDocument document({garden.scene,garden.spawn});
    std::vector<std::string> printed;
    swan::ScriptEngine engine(swan::ScriptEngine::Mode::Tool,[&](const std::string& line){printed.push_back(line);});
    swan::bindScriptApi(engine,{"first"});
    swan::bindDocument(engine,"doc",document);
    auto created=engine.run(R"(
        assert(swan.args[1] == "first")
        local key = doc:create{ name = "Scripted", position = {1, 2, 3}, properties = { hp = 3, tag = "x" } }
        doc:set(key, { yaw = 0.5, material = "default", scale = vec3(2, 2, 2) })
        local e = doc:entity(key)
        assert(e.position == vec3(1, 2, 3) and e.yaw == 0.5 and e.scale.x == 2 and e.properties.hp == 3)
        doc:transaction("three", function() for i = 1, 3 do doc:create{ name = "n" .. i } end end)
        local ok, err = pcall(doc.transaction, doc, "fails", function() doc:create{ name = "lost" } error("stop") end)
        assert(not ok and tostring(err):find("stop"))
        local ok2, err2 = pcall(function() return doc.entities() end)
        assert(not ok2 and tostring(err2):find("':'"))
        return key, #doc:history().undo
    )","=tool");
    if(!created.ok) throw std::runtime_error("Tool script failed: "+created.error);
    require(created.values[1]=="3","Transactions must be single undo steps and roll back on error");
    for(auto id:document.document().scene.entities()) require(document.document().scene.get(id)->name!="lost","Failed transaction left edits");
    require(document.undo() && document.document().scene.get(document.document().scene.find("entity-0")),"Undo of transaction");
}
void sceneFormat() {
    auto directory=std::filesystem::temp_directory_path()/("swan-script-"+std::to_string(getpid()));
    std::filesystem::create_directories(directory/"scripts");
    {std::ofstream(directory/"scripts/spin.lua")<<"return {}";}
    swan::SceneDocument document;
    document.scene.scripts().load("spin",directory/"scripts/spin.lua");
    swan::Entity e;e.key="a";e.scriptId="spin";e.properties={{"speed",2.5},{"loop",true},{"label","hi"}};document.scene.create(e);
    swan::saveScene(directory/"scene.json",document);
    auto loaded=swan::loadScene(directory/"scene.json");
    const auto& entity=*loaded.scene.get(loaded.scene.find("a"));
    require(entity.scriptId=="spin" && entity.properties==e.properties,"Script fields did not round-trip");
    require(*loaded.scene.scripts().get("spin").code=="return {}","Script code not loaded");
    {std::ofstream(directory/"scripts/spin.lua")<<"return { changed = true }";}
    loaded.scene.scripts().reloadSources();
    require(loaded.scene.scripts().get("spin").code->find("changed")!=std::string::npos,"reloadSources() did not reread");
    bool rejected=false;
    try {swan::parseScene(R"({"format":"swan-scene","version":5,"spawn":[0,0,0],"materials":{},"entities":[{"id":"a","name":"","mesh":"builtin:cube","material":"default","transform":{"position":[0,0,0],"scale":[1,1,1],"yaw":0},"script":"x"}]})");}
    catch(const std::exception&) {rejected=true;}
    require(rejected,"Version 5 accepted entity scripts");
    swan::SceneDocument memory;memory.scene.scripts().set("inline","return {}");
    rejected=false;try {swan::serializeScene(memory);} catch(const std::exception&) {rejected=true;}
    require(rejected,"In-memory script serialized without a source");
    std::filesystem::remove_all(directory);
}
void fxApi() {
    swan::SceneDocument scene;
    swan::Entity floor;floor.key="floor";floor.transform={{0,-0.5f,0},{10,1,10},0};scene.scene.create(floor);
    swan::EditorDocument document(scene);
    std::vector<std::string> printed;
    swan::ScriptEngine engine(swan::ScriptEngine::Mode::Tool,[&](const std::string& line){printed.push_back(line);});
    swan::bindScriptApi(engine);
    swan::bindDocument(engine,"doc",document);
    auto result=engine.run(R"(
        doc:set_effect("spark", { emitters = { { name = "burst", bursts = { { 0, 40 } }, lifetime = 0.5, speed = { 1, 2 }, spread = 180,
                                                 color_over_life = { { 0, { 1, 0.5, 0 } }, { 1, { 1, 0, 0, 0 } } }, space = "world" } } })
        local e = doc:effect("spark")
        assert(e.emitters[1].name == "burst" and e.emitters[1].bursts[1].count == 40, "effect read-back")
        local ok, err = pcall(doc.set_effect, doc, "bad", { emitters = { { sise = 1 } } })
        assert(not ok and tostring(err):find("unknown field 'sise'"), "schema error should name the field: " .. tostring(err))
        doc:set_effect("glow", { emitters = { { rate = 50, loop = true, lifetime = 1, speed = 0 } } })
        local key = doc:create{ name = "Emitter", effect = "glow", position = vec3(0, 1, 0) }
        assert(doc:entity(key).effect == "glow", "entity effect field")
        doc:set_track{ entity = key, property = "position", keys = { { 0, vec3(0, 1, 0) }, { 1, vec3(4, 1, 0), "smooth" } } }
        doc:set_track{ entity = key, property = "position", keys = { { 0, vec3(0, 1, 0) }, { 2, vec3(4, 1, 0), "smooth" } } }
        doc:add_event{ time = 0.5, effect = "spark", entity = key }
        doc:set_camera{ position = vec3(0, 3, 8), target = vec3(0, 1, 0), fov = 45 }
        doc:set_environment{ bloom = 0.8, tonemap = "aces" }
        doc:set_track{ environment = true, property = "exposure", keys = { { 0, 1 }, { 1, 2 } } }
        local t = doc:timeline()
        assert(#t.tracks == 2 and t.tracks[1].keys[2][1] == 2, "set_track replaces the matching track")
        assert(#t.events == 1 and t.camera.fov[1][2] == 45, "events and camera")
        assert(doc:environment().tonemap == "aces" and doc:environment().bloom == 0.8, "environment")
        local preview = swan.preview(doc, { seed = 3 })
        assert(not preview.can_render, "no renderer bound in tests")
        preview:seek(0.75)
        local stats = preview:stats()
        local spark
        for _, emitter in ipairs(stats.emitters) do if emitter.effect == "spark" then spark = emitter end end
        assert(spark and spark.spawned == 40 and spark.entity == key, "timeline event played the effect at the entity")
        assert(preview:entity(key).position.x > 0 and preview.duration == 2, "track applied in preview")
        assert(#preview:particles(5) == 5 and preview:camera().fov == 45, "particle snapshot and timeline camera")
        local before = preview.particle_count
        preview:seek(0.75)
        assert(preview.particle_count == before, "seek to the same time is stable")
        preview:seek(0.2); preview:seek(0.75)
        assert(preview.particle_count == before, "seeking back then forward reproduces the run")
        local rendered, message = pcall(preview.render, preview, "/nonexistent/x.png")
        assert(not rendered and tostring(message):find("swan script"), "render without a GPU explains how to get one")
        doc:delete_effect("spark")
        assert(#doc:timeline().events == 0, "deleting an effect removes its events")
        doc:undo()
        assert(#doc:timeline().events == 1, "undo restores them")
        doc:set_track{ entity = key, property = "position" }
        assert(#doc:timeline().tracks == 1, "set_track without keys removes the track")
        return #doc:history().undo
    )","=fx");
    if(!result.ok) throw std::runtime_error("FX automation failed: "+result.error);
    // Behaviour scripts can play effects and swap an entity's attached effect.
    auto withScripts=document.document();
    withScripts.scene.scripts().set("caster",R"(
        local C = {}
        function C:update(dt)
          if not self.cast and game.time > 0.25 then
            self.cast = true
            game.effect("spark", { position = vec3(1, 2, 3), seed = 7 })
            self.entity.effect = nil
          end
        end
        return C)");
    swan::Entity caster;caster.key="caster";caster.scriptId="caster";caster.effectId="glow";withScripts.scene.create(caster);
    swan::FxPlayer player(withScripts);
    player.seek(0.3);
    bool played=false;
    auto casterSpawned=[&]{uint64_t n=0;for(const auto& e:player.stats().emitters) if(e.entity=="caster") n+=e.spawned;return n;};
    for(const auto& e:player.stats().emitters) played|=e.effect=="spark" && e.entity.empty() && e.spawned==40;
    require(played,"game.effect() did not play the effect");
    auto before=casterSpawned();
    player.seek(0.6);
    require(player.scene().get(player.scene().find("caster"))->effectId.empty() && casterSpawned()==before,"entity.effect = nil did not stop emission");
    require(player.takeMessages().empty(),"Behaviour reported errors");
}
int main() {
    try {
        engineSandbox();std::cout<<"sandbox, limits, vec math: ok\n";
        behaviours();std::cout<<"behaviours, error isolation, spawn/destroy, collected: ok\n";
        simulation();std::cout<<"headless simulation: ok\n";
        toolApi();std::cout<<"automation API and transactions: ok\n";
        sceneFormat();std::cout<<"scene version 6 scripts: ok\n";
        fxApi();std::cout<<"effects, timeline, preview, and game.effect API: ok\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
