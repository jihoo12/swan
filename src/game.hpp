#pragma once
#include "garden.hpp"
#include "render_pose.hpp"
#include "camera_rig.hpp"
#include "input.hpp"
#include "game_layer.hpp"
#include <string>
#include <filesystem>
#include <memory>
#include <vector>
namespace swan {
// Gameplay has no GLFW/Vulkan dependency; it can run entirely in CPU tests.
class ScriptRuntime;
class Game final : public GameLayer {
public:
    explicit Game(bool overview=false);
    Game(Garden initial,bool overview,std::filesystem::path source={},std::filesystem::path save={});
    ~Game() override;
    // Behaviour scripts hold references into this object's runtime scene.
    Game(const Game&)=delete;
    Game& operator=(const Game&)=delete;
    void reloadScene();
    void saveDefinition(const std::filesystem::path& path) const;
    void handleInput(const Input& input) override;
    void fixedUpdate(float dt,const Input& input) override;
    RenderFrame renderFrame(float interpolation=1) const override;
    std::string status() const override;
    const Scene& scene() const { return garden.scene; }
    Camera camera() const { return presentationCamera(1); }
    void setThirdPerson(bool enabled) { thirdPerson=enabled; }
    bool isThirdPerson() const { return thirdPerson; }
    int collected() const { return collectedCount; }
    int collectibleCount() const { return int(garden.shards.size()); }
    bool restored() const { return !garden.shards.empty() && collectedCount==int(garden.shards.size()); }
    glm::vec3 playerFeet() const { return feet; }
    float elapsed() const { return time; }
    // Script output and errors since the last call (oldest first, at most 256 kept).
    std::vector<std::string> takeMessages() override;
    const ScriptRuntime* scripts() const { return scriptRuntime.get(); }
    bool flying() const { return flight; }
    bool isPaused() const { return paused; }
    bool onGround() const { return grounded; }
    // Spawn/respawn also resets controller velocity and interpolation history.
    void spawn(glm::vec3 feet);
private:
    Camera presentationCamera(float alpha) const;
    ThirdPersonRig cameraRig;
    bool thirdPerson=false;
    float playerYaw=0,previousPlayerYaw=0;
    void overviewCamera();
    void collect();
    void replaceDefinition(Garden initial);
    Garden definition,garden;
    std::unique_ptr<ScriptRuntime> scriptRuntime;
    std::vector<std::string> messages;
    RenderPose previousPose;
    float previousTime=0;
    std::filesystem::path sourcePath,savePath;
    std::string fileMessage;
    Camera view;
    glm::vec3 feet{0,0.2f,14},previousEye=view.position;
    float verticalVelocity=0,time=0;
    int collectedCount=0;
    bool flight=false,paused=false,grounded=false,jumpPending=false;
};
}
