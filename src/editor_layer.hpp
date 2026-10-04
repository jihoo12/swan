#pragma once
#include "editor_document.hpp"
#include "game.hpp"
#include <array>
#include <functional>
#include "options.hpp"
#include <memory>
namespace swan {
class EditorLayer final:public GameLayer {
public:
    EditorLayer(SceneDocument document,const Options& options);
    void drawGui() override;
    void handleInput(const Input& input) override;
    void fixedUpdate(float dt,const Input& input) override;
    RenderFrame renderFrame(float interpolation) const override;
    std::string status() const override;
private:
    void attempt(const std::function<void()>& action);
    EditorDocument document;
    Camera view;
    std::unique_ptr<Game> play;
    std::array<char,1024> savePath{};
    std::filesystem::path sourcePath;
    bool thirdPerson=false;
    std::array<char,256> name{},search{};
    Entity properties;
    Material materialDraft;
    std::string materialKey;
    std::string draftKey,message;
    Transform draft;
};
}
