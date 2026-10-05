#pragma once
#include "fx_capture.hpp"
#include <memory>
#include <string>
#include <vector>
namespace swan {
class VulkanRenderer;
// Creates a headless Vulkan renderer on first use, so scripts that never render need no GPU.
class LazyRenderer {
public:
    explicit LazyRenderer(bool validation=false);
    ~LazyRenderer();
    ImageRenderer renderer();
    // Prints GPU statistics to stderr and throws if validation reported errors (no-op if unused).
    void finish();
private:
    bool validation;
    std::shared_ptr<std::unique_ptr<VulkanRenderer>> instance;
};
// `swan render SCENE [OPTIONS]`; returns the process exit code.
int runRenderCommand(const std::vector<std::string>& args);
}
