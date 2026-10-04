#include "mesh.hpp"
#include <tiny_obj_loader.h>
#include <array>
#include <cmath>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
#include <tuple>
namespace swan {
namespace {
bool finite(glm::vec3 v) { return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z); }
void bounds(MeshData& mesh) {
    if(mesh.vertices.empty()) throw std::invalid_argument("Mesh has no vertices");
    mesh.minimum=mesh.maximum=mesh.vertices.front().position;
    for(const auto& vertex:mesh.vertices) {
        mesh.minimum=glm::min(mesh.minimum,vertex.position);
        mesh.maximum=glm::max(mesh.maximum,vertex.position);
    }
}
}
SharedMesh cubeMesh() {
    static const SharedMesh cube=[] {
        auto mesh=std::make_shared<MeshData>();
        const std::array<glm::vec3,6> normals={glm::vec3(0,0,1),{0,0,-1},{1,0,0},{-1,0,0},{0,1,0},{0,-1,0}};
        const std::array<glm::vec3,6> tangents={glm::vec3(1,0,0),{-1,0,0},{0,0,-1},{0,0,1},{1,0,0},{1,0,0}};
        const std::array<glm::vec2,4> corners={glm::vec2(-1,-1),{1,-1},{1,1},{-1,1}};
        for(uint32_t face=0;face<6;++face) {
            auto n=normals[face],t=tangents[face],b=glm::cross(n,t);
            for(auto uv:corners) mesh->vertices.push_back({(n+t*uv.x+b*uv.y)*0.5f,n});
            for(uint32_t index:{0u,1u,2u,0u,2u,3u}) mesh->indices.push_back(face*4+index);
        }
        bounds(*mesh); return mesh;
    }();
    return cube;
}
SharedMesh loadObjMesh(const std::filesystem::path& path) {
    try {
        constexpr size_t maxBytes=32*1024*1024,maxCorners=1000000;
        std::ifstream input(path,std::ios::binary|std::ios::ate);
        if(!input) throw std::runtime_error("Cannot open OBJ");
        auto size=input.tellg();
        if(size<0 || size>static_cast<std::streamoff>(maxBytes)) throw std::runtime_error("OBJ exceeds 32 MiB");
        std::string text(static_cast<size_t>(size),'\0'); input.seekg(0); input.read(text.data(),size);
        if(!input) throw std::runtime_error("Cannot read OBJ");
        // Check the supported primitive subset before passing data to the loader.
        std::istringstream lines(text); std::string line;
        while(std::getline(lines,line)) {
            std::istringstream fields(line); std::string kind; fields>>kind;
            if(kind=="l" || kind=="p") throw std::runtime_error("OBJ lines/points are unsupported");
            if(kind=="f") {
                size_t count=0; std::string corner;
                while(fields>>corner) { if(corner.starts_with("#")) break; ++count; }
                if(count!=3) throw std::runtime_error("OBJ must contain triangles; triangulate faces before export");
            }
        }
        tinyobj::attrib_t attributes;
        std::vector<tinyobj::shape_t> shapes;
        std::vector<tinyobj::material_t> materials;
        std::string warning,error; std::istringstream source(text);
        // Null material reader prevents external MTL access; scene materials own appearance.
        if(!tinyobj::LoadObj(&attributes,&shapes,&materials,&warning,&error,&source,nullptr,false,false))
            throw std::runtime_error(error);
        auto mesh=std::make_shared<MeshData>();
        std::map<std::tuple<int,int,size_t>,uint32_t> vertices;
        size_t faceSerial=0;
        for(const auto& shape:shapes) {
            size_t offset=0;
            for(auto faceSize:shape.mesh.num_face_vertices) {
                if(faceSize!=3) throw std::runtime_error("OBJ must contain triangles; triangulate faces before export");
                if(mesh->indices.size()+3>maxCorners) throw std::runtime_error("OBJ exceeds 1000000 triangle corners");
                std::array<tinyobj::index_t,3> indices;
                std::array<glm::vec3,3> positions;
                for(size_t i=0;i<3;++i) {
                    indices[i]=shape.mesh.indices.at(offset+i);
                    int index=indices[i].vertex_index;
                    if(index<0 || size_t(index)>=attributes.vertices.size()/3) throw std::runtime_error("OBJ position index out of range");
                    positions[i]={attributes.vertices[3*index],attributes.vertices[3*index+1],attributes.vertices[3*index+2]};
                    if(!finite(positions[i])) throw std::runtime_error("OBJ position is nonfinite");
                }
                auto cross=glm::cross(positions[1]-positions[0],positions[2]-positions[0]);
                float area=glm::length(cross);
                if(!finite(cross) || !std::isfinite(area) || area<1e-10f) throw std::runtime_error("OBJ contains a degenerate triangle");
                auto faceNormal=cross/area;
                for(size_t i=0;i<3;++i) {
                    int normalIndex=indices[i].normal_index;
                    glm::vec3 normal=faceNormal;
                    if(normalIndex>=0) {
                        if(size_t(normalIndex)>=attributes.normals.size()/3) throw std::runtime_error("OBJ normal index out of range");
                        normal={attributes.normals[3*normalIndex],attributes.normals[3*normalIndex+1],attributes.normals[3*normalIndex+2]};
                        if(!finite(normal) || !std::isfinite(glm::length(normal)) || glm::length(normal)<1e-10f) throw std::runtime_error("OBJ contains an invalid normal");
                        normal=glm::normalize(normal);
                    }
                    // Supplied normals share vertices. Missing normals split faces for flat shading.
                    auto key=std::make_tuple(indices[i].vertex_index,normalIndex,normalIndex<0?faceSerial:0);
                    auto [it,inserted]=vertices.try_emplace(key,uint32_t(mesh->vertices.size()));
                    if(inserted) mesh->vertices.push_back({positions[i],normal});
                    mesh->indices.push_back(it->second);
                }
                offset+=3; ++faceSerial;
            }
        }
        if(mesh->indices.empty()) throw std::runtime_error("OBJ contains no triangles");
        bounds(*mesh); return mesh;
    } catch(const std::exception& e) { throw std::runtime_error(path.string()+": "+e.what()); }
}
}
