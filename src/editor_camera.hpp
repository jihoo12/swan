#pragma once
#include "camera.hpp"
#include <optional>
namespace swan {
// Viewport navigation: fly (WASD + look), orbit around a pivot, pan, dolly, and animated framing.
// The pivot always lies straight ahead of the camera at pivotDistance().
class EditorCamera {
public:
    EditorCamera();
    const Camera& camera() const { return view; }
    glm::vec3 pivot() const { return view.position+forward(view.yaw,view.pitch)*distance; }
    float pivotDistance() const { return distance; }
    float speed() const { return flySpeed; }
    void setSpeed(float unitsPerSecond);
    // Multiply fly speed by 1.25 per step (mouse wheel while flying).
    void adjustSpeed(float steps);
    // move: x=right, y=up, z=forward, each in [-1,1].
    void fly(glm::vec3 move,bool boost,float dt);
    void look(glm::vec2 radians);
    void orbit(glm::vec2 radians);
    void pan(glm::vec2 pixels,float viewportHeight);
    void dolly(float steps);
    // Smoothly move so a sphere of `radius` around `center` fills the view.
    void frame(glm::vec3 center,float radius);
    void place(glm::vec3 position,float yaw,float pitch);
    void update(float dt);
    bool animating() const { return transition.has_value(); }
private:
    struct Transition { glm::vec3 from,to; float fromDistance,toDistance,elapsed=0; };
    void cancelTransition() { transition.reset(); }
    Camera view;
    float distance=10,flySpeed=6;
    std::optional<Transition> transition;
};
}
