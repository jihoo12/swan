#pragma once
#include "editor_document.hpp"
#include "fx_capture.hpp"
#include "script_engine.hpp"
#include <filesystem>
#include <string>
#include <vector>
namespace swan {
// Installs the `swan` table: open/new/simulate/preview/validate, plus `swan.args` and
// `swan.version`. With a renderer, Preview:render() and render_sheet() write PNG images.
void bindScriptApi(ScriptEngine& engine,std::vector<std::string> args={},ImageRenderer renderer={});
// Exposes an existing document (e.g. the editor's) as a global; the document must outlive the
// engine. Every call is a validated EditorDocument command, so edits are undoable.
void bindDocument(ScriptEngine& engine,const std::string& global,EditorDocument& document,std::filesystem::path path={});
// `swan script FILE [ARGS...]`: run a trusted automation script without a window. Returns an exit
// code: 1 on error, the script's numeric return value if any, else 0.
int runScriptFile(const std::filesystem::path& file,std::vector<std::string> args,ImageRenderer renderer={});
}
