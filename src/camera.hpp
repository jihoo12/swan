#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
namespace swan {
inline glm::vec3 forward(float yaw,float pitch) {
    return {std::cos(yaw)*std::cos(pitch),std::sin(pitch),std::sin(yaw)*std::cos(pitch)};
}
struct Camera {
    glm::vec3 position{0,1.9f,14};
    float yaw=-1.5707963f,pitch=-0.08f;
    float fov=65,nearPlane=0.1f,farPlane=150;
    void look(glm::vec2 delta) {
        yaw=std::remainder(yaw+delta.x,6.2831853f);
        pitch=std::clamp(pitch+delta.y,-1.5f,1.5f);
    }
    glm::mat4 viewProjection(float aspect) const {
        auto projection=glm::perspective(glm::radians(fov),aspect,nearPlane,farPlane);
        projection[1][1]*=-1;
        return projection*glm::lookAt(position,position+forward(yaw,pitch),glm::vec3(0,1,0));
    }
};
}
