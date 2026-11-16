#include "shadow.hpp"
#include <cmath>
#include <iostream>
#include <stdexcept>
#include <vector>
void require(bool value,const std::string& message) {if(!value) throw std::runtime_error(message);}
glm::vec3 project(const glm::mat4& m,glm::vec3 p) {auto c=m*glm::vec4(p,1);return glm::vec3(c)/c.w;}
swan::RenderObject box(glm::vec3 position,glm::vec3 scale) {swan::RenderObject o;o.transform.position=position;o.transform.scale=scale;return o;}
bool inside(glm::vec3 p) {return std::abs(p.x)<=1 && std::abs(p.y)<=1 && p.z>=0 && p.z<=1;}
int main() {
    try {
        swan::Camera camera;
        swan::Environment environment;
        std::vector<swan::RenderObject> objects{box({0,-0.5f,0},{60,1,60}),box({3,1,-4},{1,2,1})};
        constexpr uint32_t resolution=2048;
        auto shadow=fitSunShadow(camera,16.0f/9,environment,objects,resolution);
        // Points the camera sees within the shadow distance land inside the map.
        glm::vec3 front=swan::forward(camera.yaw,camera.pitch);
        for(float d:{1.0f,10.0f,environment.shadowDistance*0.95f}) {
            auto p=project(shadow.viewProjection,camera.position+front*d);
            require(inside(p),"Visible point at distance "+std::to_string(d)+" is outside the shadow map");
        }
        require(shadow.texelWorld>0 && shadow.texelWorld<0.1f,"Shadow texel size is implausible");
        // A tall caster far toward the sun, outside the view sphere, still fits the depth range.
        glm::vec3 toSun=glm::normalize(environment.sunDirection);
        glm::vec3 target=camera.position+front*10.0f;
        objects.push_back(box(target+toSun*120.0f,{1,1,1}));
        auto extended=fitSunShadow(camera,16.0f/9,environment,objects,resolution);
        auto caster=project(extended.viewProjection,target+toSun*120.0f);
        require(inside(caster),"Distant caster toward the sun was clipped from the shadow depth range");
        require(project(extended.viewProjection,target).z>caster.z,"Receiver is not farther from the sun than its caster");
        // Snapping: the world origin moves across the map by whole texels as the camera moves.
        auto origin=[&](glm::vec3 position){
            auto c=camera;c.position=position;
            return project(fitSunShadow(c,16.0f/9,environment,objects,resolution).viewProjection,glm::vec3(0));};
        auto a=origin(camera.position),b=origin(camera.position+glm::vec3(0.37f,0,0.11f));
        for(int axis=0;axis<2;++axis) {
            float texels=(b[axis]-a[axis])*0.5f*float(resolution);
            require(std::abs(texels-std::round(texels))<1e-2f,"Shadow projection moved by a fraction of a texel");
        }
        // A sun straight overhead must not produce a degenerate light view.
        environment.sunDirection={0,1,0};
        auto overhead=fitSunShadow(camera,1,environment,objects,resolution);
        for(int c=0;c<4;++c) for(int r=0;r<4;++r) require(std::isfinite(overhead.viewProjection[c][r]),"Overhead sun produced a non-finite matrix");
        require(inside(project(overhead.viewProjection,camera.position+front*5.0f)),"Overhead sun misses nearby geometry");
        bool threw=false;
        try {fitSunShadow(camera,1,environment,objects,0);} catch(const std::invalid_argument&) {threw=true;}
        require(threw,"Zero shadow resolution accepted");
        std::cout<<"sun shadow fitting: ok\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
