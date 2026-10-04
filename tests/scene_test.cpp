#include "scene.hpp"
#include <iostream>
int main() {
    auto scene=swan::makeScene();
    if(scene.size()<400) return 1;
    int animated=0;
    for(const auto& o:scene) {
        if(o.scale.x<=0 || o.scale.y<=0 || o.scale.z<=0) return 2;
        if(!std::isfinite(o.position.x+o.position.y+o.position.z)) return 3;
        animated+=o.animated;
    }
    if(animated!=1 || std::abs(glm::length(swan::forward(1,0.4f))-1)>0.0001f) return 4;
    std::cout << "Scene invariants passed: " << scene.size() << " objects\n";
}
