#pragma once
#include "options.hpp"
#include "game_layer.hpp"
#include <utility>
namespace swan {
// Application coordinator. Renderer and gameplay own their respective resources.
class Engine {
public:
    explicit Engine(Options options):options(std::move(options)) {}
    void run(GameLayer& game);
private:
    Options options;
};
}
