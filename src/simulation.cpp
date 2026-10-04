#include "simulation.hpp"
#include "garden.hpp"
#include <cmath>
#include <stdexcept>
namespace swan {
namespace {
Input heldOnly(Input input) {
    input.look={};input.jump=input.pause=input.reset=input.toggleFlight=input.interact=false;
    input.reload=input.save=input.toggleCamera=false;
    return input;
}
}
Simulation::Simulation(const SceneDocument& document,SimulationOptions options)
    :game(std::make_unique<Game>(gardenFromScene(document.scene,document.spawn),options.flying)) {
    game->setThirdPerson(options.thirdPerson);
}
Simulation Simulation::load(const std::filesystem::path& scene,SimulationOptions options) {
    return Simulation(loadScene(scene),options);
}
void Simulation::tick(const Input& input) {
    // File actions belong to windowed runs; a headless step never touches disk.
    auto actions=input;actions.reload=actions.save=false;
    game->handleInput(actions);
    game->fixedUpdate(float(tickSeconds),heldOnly(input));
    ++tickCount;
}
uint64_t Simulation::step(double seconds,const Input& input) {
    if(!std::isfinite(seconds) || seconds<0) throw std::invalid_argument("Simulation step must be finite and nonnegative");
    auto count=std::max<uint64_t>(1,uint64_t(std::llround(seconds/tickSeconds)));
    tick(input);
    for(uint64_t i=1;i<count;++i) tick(heldOnly(input));
    return count;
}
}
