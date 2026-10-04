#pragma once
#include "garden.hpp"
#include "input.hpp"
#include "game_layer.hpp"
#include <string>
namespace swan {
// Gameplay has no GLFW/Vulkan dependency; it can run entirely in CPU tests.
class Game final : public GameLayer {
public:
    explicit Game(bool overview=false);
    void handleInput(const Input& input) override;
    void fixedUpdate(float dt,const Input& input) override;
    RenderFrame renderFrame(float interpolation=1) const override;
    std::string status() const override;
    const Scene& scene() const { return garden.scene; }
    const Camera& camera() const { return view; }
    int collected() const { return collectedCount; }
    bool flying() const { return flight; }
    bool isPaused() const { return paused; }
    bool onGround() const { return grounded; }
    // Spawn/respawn also resets controller velocity and interpolation history.
    void spawn(glm::vec3 feet);
private:
    void overviewCamera();
    void collect();
    Garden garden=makeGarden();
    Camera view;
    glm::vec3 feet{0,0.2f,14},previousEye=view.position;
    float verticalVelocity=0,time=0;
    int collectedCount=0;
    bool flight=false,paused=false,grounded=false,jumpPending=false;
};
}
