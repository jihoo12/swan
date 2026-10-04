#include "editor_document.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <iostream>
namespace {
glm::vec3 vector(const nlohmann::json& value) {
    if(!value.is_array() || value.size()!=3) throw std::invalid_argument("Expected three-component vector");
    return {value.at(0).get<float>(),value.at(1).get<float>(),value.at(2).get<float>()};
}
}
int main(int argc,char** argv) {
    try {
        if(argc!=4) {std::cerr<<"Usage: swan-scene INPUT OUTPUT EDITS.json\n";return 1;}
        swan::EditorDocument document(swan::loadScene(argv[1]));
        std::ifstream input(argv[3],std::ios::binary|std::ios::ate);
        if(!input || input.tellg()>1024*1024) throw std::invalid_argument("Edits must be a readable file under 1 MiB");
        input.seekg(0);nlohmann::json commands;input>>commands;
        if(!commands.is_array() || commands.size()>256) throw std::invalid_argument("Expected up to 256 edit commands");
        for(const auto& command:commands) {
            if(!command.is_object()) throw std::invalid_argument("Edit command must be an object");
            auto operation=command.at("op").get<std::string>();
            for(auto it=command.begin();it!=command.end();++it) {
                bool allowed=it.key()=="op" ||
                    ((operation=="transform" || operation=="parent" || operation=="delete") && it.key()=="id") ||
                    (operation=="transform" && (it.key()=="position" || it.key()=="scale" || it.key()=="yaw")) ||
                    (operation=="parent" && it.key()=="parent");
                if(!allowed) throw std::invalid_argument("Unknown edit field: "+it.key());
            }
            if(operation=="transform") document.apply(swan::SetTransform{command.at("id").get<std::string>(),
                {vector(command.at("position")),vector(command.at("scale")),command.at("yaw").get<float>()}});
            else if(operation=="parent") document.apply(swan::SetParent{command.at("id").get<std::string>(),
                command.at("parent").is_null()?std::nullopt:std::optional<std::string>(command.at("parent").get<std::string>())});
            else if(operation=="delete") document.apply(swan::DeleteEntity{command.at("id").get<std::string>()});
            else if(operation=="undo") {if(!document.undo()) throw std::invalid_argument("Nothing to undo");}
            else if(operation=="redo") {if(!document.redo()) throw std::invalid_argument("Nothing to redo");}
            else throw std::invalid_argument("Unknown edit operation: "+operation);
        }
        document.save(argv[2]);std::cout<<"Saved edited scene: "<<argv[2]<<'\n';
    } catch(const std::exception& error) {std::cerr<<"Scene edit: "<<error.what()<<'\n';return 1;}
}
