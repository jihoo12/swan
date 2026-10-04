#include "key_bindings.hpp"
#include "game.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {try {
    swan::Game game;game.setThirdPerson(true);
    auto before=game.renderFrame();
    for(int mods:{GLFW_MOD_CONTROL,GLFW_MOD_CONTROL|GLFW_MOD_SHIFT,GLFW_MOD_ALT,GLFW_MOD_SUPER}) {
        swan::Input input;swan::applyActionKey(input,GLFW_KEY_F,GLFW_PRESS,mods);game.handleInput(input);
        require(!game.flying() && game.isThirdPerson() && game.renderFrame().objects.size()==before.objects.size(),"Modified F changed third-person mode");
        require(game.camera().position==before.camera.position,"Modified F moved camera");
        swan::applyActionKey(input,GLFW_KEY_V,GLFW_PRESS,mods);game.handleInput(input);
        require(game.isThirdPerson(),"Modified V changed viewpoint");
    }
    for(int action:{GLFW_REPEAT,GLFW_RELEASE}) {
        swan::Input input;swan::applyActionKey(input,GLFW_KEY_F,action,0);game.handleInput(input);
        require(!game.flying(),"Repeat/release toggled flight");
    }
    swan::Input flight;swan::applyActionKey(flight,GLFW_KEY_F,GLFW_PRESS,0);game.handleInput(flight);
    require(game.flying() && game.isThirdPerson(),"Bare F failed or discarded third-person preference");
    game.handleInput(flight);
    require(!game.flying() && game.isThirdPerson() && game.renderFrame().objects.size()==before.objects.size(),"Return from flight discarded follow mode");
    swan::Input view;swan::applyActionKey(view,GLFW_KEY_V,GLFW_PRESS,0);game.handleInput(view);
    require(!game.isThirdPerson(),"Bare V did not switch viewpoint");
    swan::Input jump;swan::applyActionKey(jump,GLFW_KEY_SPACE,GLFW_PRESS,GLFW_MOD_SHIFT);
    require(jump.jump,"Shift prevented sprint jump");
    std::cout<<"Modifier routing and third-person flight preference passed\n";
} catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}}
