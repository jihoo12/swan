#include "garden.hpp"
#include "camera.hpp"
#include <iostream>
#include <stdexcept>
#include <limits>
void require(bool value,const char* message) { if(!value) throw std::runtime_error(message); }
int main() {
    try {
        auto garden=swan::makeGarden();
        require(garden.scene.size()==442,"Garden entity count changed");
        int animated=0,collectible=0;
        for(auto id:garden.scene.entities()) {
            const auto& e=*garden.scene.get(id);
            require(e.transform.scale.x>0 && e.transform.scale.y>0 && e.transform.scale.z>0,"Invalid geometry");
            require(std::isfinite(e.transform.position.x+e.transform.position.y+e.transform.position.z),"Nonfinite position");
            animated+=e.animation.has_value(); collectible+=e.collectible;
        }
        require(animated==6 && collectible==5,"Garden components missing");
        require(std::abs(glm::length(swan::forward(1,0.4f))-1)<0.0001f,"Camera direction not normalized");
        swan::Scene scene;
        auto first=scene.create({});
        require(scene.destroy(first),"Cannot destroy entity");
        auto replacement=scene.create({});
        require(first.index==replacement.index && first.generation!=replacement.generation,"Slot generation not advanced");
        require(!scene.get(first) && !scene.destroy(first),"Stale handle accesses replacement");
        require(scene.get(replacement) && scene.size()==1,"Replacement missing");
        require(!scene.get({99999,0}),"Invalid handle accepted");
        swan::Entity invalid; invalid.transform.scale.y=0;
        bool rejected=false;
        try { scene.create(invalid); } catch(const std::invalid_argument&) { rejected=true; }
        require(rejected && scene.size()==1,"Invalid entity corrupted scene");
        std::cout << "Scene lifecycle and garden invariants passed\n";
    } catch(const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
