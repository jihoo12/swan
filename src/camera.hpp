#pragma once
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>
namespace swan {
inline glm::vec3 forward(float yaw,float pitch) {
    return {std::cos(yaw)*std::cos(pitch),std::sin(pitch),std::sin(yaw)*std::cos(pitch)};
}
struct Ray { glm::vec3 origin{},direction{0,0,-1}; };
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
    // World-space ray through `pixel` of a `size` image rendered with this camera, measured in
    // pixels from the top-left corner (pixel centers at +0.5), starting on the near plane.
    Ray pixelRay(glm::vec2 pixel,glm::vec2 size) const {
        auto inverse=glm::inverse(viewProjection(size.x/size.y));
        glm::vec2 ndc=(pixel/size)*2.0f-1.0f; // Vulkan clip space: -1 is the top row.
        auto unproject=[&](float depth){auto p=inverse*glm::vec4(ndc,depth,1);return glm::vec3(p)/p.w;};
        auto nearPoint=unproject(0),farPoint=unproject(1);
        return {nearPoint,glm::normalize(farPoint-nearPoint)};
    }
};
}
