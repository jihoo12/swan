#pragma once
#include <array>
#include <filesystem>
#include <functional>
#include <string>
#include <vector>
namespace swan {
// Modal scene browser. open() only records a request; draw() opens the popup in its own ID scope.
class FileDialog {
public:
    enum class Mode { Open, Save };
    using Accept=std::function<void(const std::filesystem::path&)>;
    // `extension` filters the listing (".json" scenes, ".lua" scripts) and names the dialog.
    void open(Mode mode,const std::filesystem::path& initial,Accept accept,std::string extension=".json");
    void draw(const std::vector<std::filesystem::path>& recent);
    bool visible() const { return isOpen; }
private:
    struct Entry { std::filesystem::path path; bool directory=false; uintmax_t size=0; };
    void navigate(const std::filesystem::path& directory);
    void accept(const std::filesystem::path& path);
    Mode mode=Mode::Open;
    Accept onAccept;
    bool requested=false,isOpen=false,showAll=false,focusName=false;
    std::filesystem::path directory;
    std::vector<Entry> entries;
    std::array<char,512> location{},name{};
    std::string error,extension=".json";
    int selected=-1;
};
}
