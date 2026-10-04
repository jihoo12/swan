#pragma once
#include "input.hpp"
#include <GLFW/glfw3.h>
namespace swan {
// Application shortcuts are bare keys (Shift may remain held while sprinting).
// Ctrl/Alt/Super combinations are reserved rather than falling through to a bare key.
inline void applyActionKey(Input& input,int key,int action,int modifiers) {
    if(action!=GLFW_PRESS || (modifiers&(GLFW_MOD_CONTROL|GLFW_MOD_ALT|GLFW_MOD_SUPER))) return;
    switch(key) {
    case GLFW_KEY_SPACE: input.jump=true; break;
    case GLFW_KEY_P: input.pause=true; break;
    case GLFW_KEY_R: input.reset=true; break;
    case GLFW_KEY_F: input.toggleFlight=true; break;
    case GLFW_KEY_V: input.toggleCamera=true; break;
    case GLFW_KEY_E: input.interact=true; break;
    case GLFW_KEY_F5: input.reload=true; break;
    case GLFW_KEY_F6: input.save=true; break;
    default: break;
    }
}
}
