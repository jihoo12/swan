#include "editor_camera.hpp"
#include <cmath>
namespace swan {
namespace {constexpr float transitionSeconds=0.28f;}
EditorCamera::EditorCamera() {view.position={12,9,16};view.yaw=-2.21f;view.pitch=-0.35f;view.farPlane=400;}
void EditorCamera::setSpeed(float value) {flySpeed=std::isfinite(value)?std::clamp(value,0.5f,200.0f):6.0f;}
void EditorCamera::adjustSpeed(float steps) {setSpeed(flySpeed*std::pow(1.25f,steps));}
void EditorCamera::fly(glm::vec3 move,bool boost,float dt) {
    if(move==glm::vec3(0)) return;
    cancelTransition();
    auto ahead=forward(view.yaw,view.pitch),right=glm::normalize(glm::cross(forward(view.yaw,0),glm::vec3(0,1,0)));
    auto direction=right*move.x+glm::vec3(0,1,0)*move.y+ahead*move.z;
    if(glm::length(direction)>1) direction=glm::normalize(direction);
    view.position+=direction*flySpeed*(boost?3.0f:1.0f)*dt;
}
void EditorCamera::look(glm::vec2 radians) {cancelTransition();view.look(radians);}
void EditorCamera::orbit(glm::vec2 radians) {
    cancelTransition();
    auto center=pivot();view.look(radians);
    view.position=center-forward(view.yaw,view.pitch)*distance;
}
void EditorCamera::pan(glm::vec2 pixels,float viewportHeight) {
    if(viewportHeight<=0) return;
    cancelTransition();
    auto ahead=forward(view.yaw,view.pitch),right=glm::normalize(glm::cross(ahead,glm::vec3(0,1,0))),up=glm::cross(right,ahead);
    // Keep the point under the cursor at the pivot depth fixed to the cursor.
    float worldPerPixel=2*distance*std::tan(glm::radians(view.fov)*0.5f)/viewportHeight;
    view.position+=(-right*pixels.x+up*pixels.y)*worldPerPixel;
}
void EditorCamera::dolly(float steps) {
    cancelTransition();
    auto center=pivot();
    float next=distance*std::pow(0.85f,steps);
    if(next<0.25f) {
        // Push through: move the pivot forward instead of collapsing onto it.
        view.position+=forward(view.yaw,view.pitch)*(distance-0.25f+0.5f*std::abs(steps));
        distance=0.25f;return;
    }
    distance=std::min(next,500.0f);
    view.position=center-forward(view.yaw,view.pitch)*distance;
}
void EditorCamera::frame(glm::vec3 center,float radius) {
    radius=std::isfinite(radius)?std::max(radius,0.25f):1.0f;
    float target=std::max(1.5f,radius/std::sin(glm::radians(view.fov)*0.5f)*1.15f);
    transition=Transition{view.position,center-forward(view.yaw,view.pitch)*target,distance,target};
}
void EditorCamera::place(glm::vec3 position,float yaw,float pitch) {
    cancelTransition();view.position=position;view.yaw=yaw;view.pitch=std::clamp(pitch,-1.5f,1.5f);
}
void EditorCamera::update(float dt) {
    if(!transition) return;
    transition->elapsed+=std::max(dt,0.0f);
    float t=std::min(transition->elapsed/transitionSeconds,1.0f),eased=1-std::pow(1-t,3.0f);
    view.position=glm::mix(transition->from,transition->to,eased);
    distance=glm::mix(transition->fromDistance,transition->toDistance,eased);
    if(t>=1) transition.reset();
}
}
