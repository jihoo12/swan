#include "visibility.hpp"
#include <cmath>
namespace swan {
Frustum::Frustum(const glm::mat4& matrix) {
    glm::dvec4 rows[4];
    for(int row=0;row<4;++row) for(int col=0;col<4;++col) rows[row][col]=matrix[col][row];
    planes={rows[3]+rows[0],rows[3]-rows[0],rows[3]+rows[1],rows[3]-rows[1],rows[2],rows[3]-rows[2]};
}
bool Frustum::intersects(const MeshData& mesh,const Transform& world) const {
    // Treat unexpected data conservatively: culling must never hide bad bounds.
    for(int i=0;i<3;++i) if(!std::isfinite(mesh.minimum[i]) || !std::isfinite(mesh.maximum[i]) ||
        mesh.minimum[i]>mesh.maximum[i] || !std::isfinite(world.position[i]) ||
        !std::isfinite(world.scale[i]) || world.scale[i]<=0) return true;
    if(!std::isfinite(world.yaw)) return true;
    glm::dvec3 center=(glm::dvec3(mesh.minimum)+glm::dvec3(mesh.maximum))*0.5;
    glm::dvec3 half=(glm::dvec3(mesh.maximum)-glm::dvec3(mesh.minimum))*0.5*glm::dvec3(world.scale);
    center*=glm::dvec3(world.scale);
    double c=std::cos(double(world.yaw)),s=std::sin(double(world.yaw));
    center=glm::dvec3(world.position)+glm::dvec3(c*center.x+s*center.z,center.y,-s*center.x+c*center.z);
    glm::dvec3 axes[3]={{c,0,-s},{0,1,0},{s,0,c}};
    for(const auto& plane:planes) {
        if(!std::isfinite(plane.x) || !std::isfinite(plane.y) || !std::isfinite(plane.z) || !std::isfinite(plane.w)) return true;
        glm::dvec3 normal(plane);
        double distance=glm::dot(normal,center)+plane.w;
        double radius=0;
        for(int i=0;i<3;++i) radius+=std::abs(glm::dot(normal,axes[i]))*half[i];
        // Relative tolerance absorbs float transform/projection rounding at edges.
        double tolerance=1e-5*(std::abs(distance)+radius+std::abs(plane.w)+1);
        if(distance+radius < -tolerance) return false;
    }
    return true;
}
}
