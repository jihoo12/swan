#pragma once
#include <glm/glm.hpp>
namespace swan {
// Held values are sampled once per rendered frame. Actions are consumed once.
struct Input {
    glm::vec2 move{},look{};
    float vertical=0;
    bool sprint=false;
    bool jump=false,pause=false,reset=false,toggleFlight=false,interact=false;
    bool reload=false,save=false,toggleCamera=false;
};
}
