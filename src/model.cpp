#include "model.hpp"
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <assimp/GltfMaterial.h>
#include <glm/gtc/matrix_inverse.hpp>
#include <algorithm>
#include <fstream>
#include <functional>
#include <map>
#include <cmath>
#include <stdexcept>
namespace swan {
namespace {
glm::mat4 matrix(const aiMatrix4x4& m) {
    return {{m.a1,m.b1,m.c1,m.d1},{m.a2,m.b2,m.c2,m.d2},{m.a3,m.b3,m.c3,m.d3},{m.a4,m.b4,m.c4,m.d4}};
}
bool finite(glm::vec3 v) {return std::isfinite(v.x)&&std::isfinite(v.y)&&std::isfinite(v.z);}
bool hasTexture(const aiMaterial& material,aiTextureType type) {return material.GetTextureCount(type)>0;}
// Base color texture: embedded (GLB / data URI) or an external file next to the model.
SharedTexture baseColorTexture(const aiScene& scene,const aiMaterial& material,const std::filesystem::path& model,ImportedMaterial& out) {
    aiString path;unsigned uvIndex=0;
    aiTextureType type=hasTexture(material,aiTextureType_BASE_COLOR)?aiTextureType_BASE_COLOR:aiTextureType_DIFFUSE;
    if(material.GetTexture(type,0,&path,nullptr,&uvIndex)!=AI_SUCCESS) return nullptr;
    if(uvIndex!=0) {out.unsupported.push_back("base color texture uses TEXCOORD_"+std::to_string(uvIndex));return nullptr;}
    std::string name=path.C_Str();
    if(const auto* embedded=scene.GetEmbeddedTexture(path.C_Str())) {
        if(embedded->mHeight!=0) {out.unsupported.push_back("uncompressed embedded texture");return nullptr;}
        std::span<const uint8_t> bytes(reinterpret_cast<const uint8_t*>(embedded->pcData),embedded->mWidth);
        if(!isPng(bytes)) {out.unsupported.push_back(std::string("base color texture format '")+embedded->achFormatHint+"' (only PNG)");return nullptr;}
        return decodePngTexture(bytes);
    }
    auto file=model.parent_path()/std::filesystem::path(name);
    std::ifstream input(file,std::ios::binary);
    if(!input) throw std::invalid_argument("Cannot open glTF texture: "+file.string());
    char signature[8]{};input.read(signature,8);
    if(!isPng({reinterpret_cast<const uint8_t*>(signature),size_t(input.gcount())})) {
        out.unsupported.push_back("base color texture '"+name+"' is not PNG");return nullptr;
    }
    return loadPngTexture(file);
}
SharedImportedMaterial convertMaterial(const aiScene& scene,const aiMaterial& material,const std::filesystem::path& model) {
    auto out=std::make_shared<ImportedMaterial>();
    aiString name;if(material.Get(AI_MATKEY_NAME,name)==AI_SUCCESS) out->name=name.C_Str();
    aiColor4D base(1,1,1,1);
    if(material.Get(AI_MATKEY_BASE_COLOR,base)!=AI_SUCCESS) material.Get(AI_MATKEY_COLOR_DIFFUSE,base);
    out->color={base.r,base.g,base.b};
    if(!finite(out->color) || base.r<0 || base.g<0 || base.b<0) throw std::invalid_argument("Invalid glTF base color factor");
    aiString alphaMode;
    if(base.a<1 || (material.Get(AI_MATKEY_GLTF_ALPHAMODE,alphaMode)==AI_SUCCESS && std::string(alphaMode.C_Str())!="OPAQUE"))
        out->unsupported.push_back("transparency (drawn opaque)");
    aiColor3D emissive(0,0,0);material.Get(AI_MATKEY_COLOR_EMISSIVE,emissive);
    float strength=1;material.Get(AI_MATKEY_EMISSIVE_INTENSITY,strength);
    float glow=std::max({emissive.r,emissive.g,emissive.b})*strength;
    if(!std::isfinite(glow) || glow<0) throw std::invalid_argument("Invalid glTF emissive factor");
    // Swan emission scales the surface color; match the brightest emissive channel.
    if(glow>0) out->emission=std::min(64.0f,glow/std::max(std::max({out->color.r,out->color.g,out->color.b}),0.05f));
    out->texture=baseColorTexture(scene,material,model,*out);
    if(hasTexture(material,aiTextureType_NORMALS)) out->unsupported.push_back("normal texture");
    if(hasTexture(material,aiTextureType_EMISSIVE)) out->unsupported.push_back("emissive texture");
    if(hasTexture(material,aiTextureType_METALNESS) || hasTexture(material,aiTextureType_DIFFUSE_ROUGHNESS) || hasTexture(material,aiTextureType_GLTF_METALLIC_ROUGHNESS))
        out->unsupported.push_back("metallic-roughness texture");
    if(hasTexture(material,aiTextureType_LIGHTMAP) || hasTexture(material,aiTextureType_AMBIENT_OCCLUSION)) out->unsupported.push_back("occlusion texture");
    return out;
}
}
StaticModel loadStaticGltf(const std::filesystem::path& path) {
    try {
        auto extension=path.extension().string();
        if(extension!=".gltf" && extension!=".glb") throw std::invalid_argument("Static importer requires .gltf or .glb");
        if(std::filesystem::file_size(path)>32*1024*1024) throw std::invalid_argument("glTF source exceeds 32 MiB");
        Assimp::Importer importer;
        auto* scene=importer.ReadFile(path.string(),aiProcess_Triangulate|aiProcess_GenNormals|aiProcess_ValidateDataStructure);
        if(!scene || !scene->mRootNode) throw std::runtime_error(importer.GetErrorString());
        if(scene->mNumAnimations) throw std::invalid_argument("Animated glTF is unsupported by the static importer");
        for(unsigned i=0;i<scene->mNumMeshes;++i)
            if(scene->mMeshes[i]->HasBones() || scene->mMeshes[i]->mNumAnimMeshes)
                throw std::invalid_argument("Skinning/morph targets are unsupported by the static importer");
        StaticModel model;size_t corners=0,visited=0;
        std::map<unsigned,SharedImportedMaterial> materials;
        auto materialFor=[&](unsigned index) {
            if(index>=scene->mNumMaterials) throw std::invalid_argument("Invalid glTF material index");
            auto found=materials.find(index);
            if(found==materials.end()) found=materials.emplace(index,convertMaterial(*scene,*scene->mMaterials[index],path)).first;
            return found->second;
        };
        std::function<void(const aiNode*,glm::mat4,size_t)> visit=[&](const aiNode* node,glm::mat4 parent,size_t depth) {
            if(depth>64 || ++visited>10000) throw std::invalid_argument("glTF node hierarchy exceeds limits");
            auto world=parent*matrix(node->mTransformation);
            for(int col=0;col<4;++col) for(int row=0;row<4;++row)
                if(!std::isfinite(world[col][row])) throw std::invalid_argument("Nonfinite glTF node matrix");
            if(std::abs(world[0][3])>1e-6f || std::abs(world[1][3])>1e-6f || std::abs(world[2][3])>1e-6f || std::abs(world[3][3]-1)>1e-6f)
                throw std::invalid_argument("glTF nodes require affine transforms");
            float determinant=glm::determinant(glm::mat3(world));
            if(!std::isfinite(determinant) || std::abs(determinant)<1e-10f) throw std::invalid_argument("Singular glTF node transform");
            auto normalMatrix=glm::transpose(glm::inverse(glm::mat3(world)));
            for(unsigned part=0;part<node->mNumMeshes;++part) {
                if(model.parts.size()>=256) throw std::invalid_argument("glTF exceeds 256 node primitives");
                auto* source=scene->mMeshes[node->mMeshes[part]];
                if(source->mPrimitiveTypes!=aiPrimitiveType_TRIANGLE || !source->HasNormals() || source->mNumFaces==0)
                    throw std::invalid_argument("glTF requires triangle primitives");
                corners+=size_t(source->mNumFaces)*3;
                if(corners>1000000 || source->mNumVertices>1000000) throw std::invalid_argument("glTF exceeds geometry limits");
                auto mesh=std::make_shared<MeshData>();mesh->vertices.reserve(source->mNumVertices);
                for(unsigned i=0;i<source->mNumVertices;++i) {
                    auto p=source->mVertices[i],n=source->mNormals[i];
                    Vertex vertex;vertex.position=glm::vec3(world*glm::vec4(p.x,p.y,p.z,1));
                    vertex.normal=normalMatrix*glm::vec3(n.x,n.y,n.z);
                    if(!finite(vertex.position) || !finite(vertex.normal) || (!std::isfinite(glm::length(vertex.normal)) || glm::length(vertex.normal)<1e-10f))
                        throw std::invalid_argument("Invalid glTF vertex/normal");
                    vertex.normal=glm::normalize(vertex.normal);
                    if(source->HasTextureCoords(0)) {
                        auto uv=source->mTextureCoords[0][i];
                        // Assimp's glTF importer converts UVs to its bottom-left convention.
                        vertex.uv={uv.x,1-uv.y};
                        if(!std::isfinite(vertex.uv.x) || !std::isfinite(vertex.uv.y)) throw std::invalid_argument("Nonfinite glTF UV");
                    }
                    mesh->vertices.push_back(vertex);
                }
                for(unsigned face=0;face<source->mNumFaces;++face) {
                    const auto& f=source->mFaces[face];
                    if(f.mNumIndices!=3) throw std::invalid_argument("Nontriangle glTF face");
                    for(unsigned j=0;j<3;++j) if(f.mIndices[j]>=mesh->vertices.size()) throw std::invalid_argument("Invalid glTF index");
                    auto a=mesh->vertices[f.mIndices[0]].position,b=mesh->vertices[f.mIndices[1]].position,c=mesh->vertices[f.mIndices[2]].position;
                    auto area=glm::length(glm::cross(b-a,c-a));
                    if(!std::isfinite(area) || area<1e-10f) throw std::invalid_argument("Degenerate glTF triangle");
                    mesh->indices.insert(mesh->indices.end(),{f.mIndices[0],f.mIndices[determinant<0?2:1],f.mIndices[determinant<0?1:2]});
                }
                mesh->minimum=mesh->maximum=mesh->vertices.front().position;
                for(const auto& vertex:mesh->vertices) {mesh->minimum=glm::min(mesh->minimum,vertex.position);mesh->maximum=glm::max(mesh->maximum,vertex.position);}
                if(!source->HasTextureCoords(0)) {
                    auto material=materialFor(source->mMaterialIndex);
                    if(material->texture) {
                        auto copy=std::make_shared<ImportedMaterial>(*material);copy->texture=nullptr;
                        copy->unsupported.push_back("base color texture on a primitive without TEXCOORD_0");
                        model.materials.push_back(std::move(copy));
                    } else model.materials.push_back(std::move(material));
                } else model.materials.push_back(materialFor(source->mMaterialIndex));
                model.parts.push_back(std::move(mesh));
            }
            for(unsigned child=0;child<node->mNumChildren;++child) visit(node->mChildren[child],world,depth+1);
        };
        visit(scene->mRootNode,glm::mat4(1),0);
        if(model.parts.empty()) throw std::invalid_argument("glTF has no triangle geometry");
        return model;
    } catch(const std::exception& error) {throw std::runtime_error(path.string()+": "+error.what());}
}
}
