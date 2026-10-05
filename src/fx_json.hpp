#pragma once
// Internal: JSON conversion of effects and timelines (scene files and the Lua API share it).
// Not installed; public callers use the string functions in fx_io.hpp.
#include "fx.hpp"
#include "scene.hpp"
#include "timeline.hpp"
#include <nlohmann/json.hpp>
#include <charconv>
#include <cstdlib>
namespace swan {
// The shortest decimal that reads back as the same float ("0.85", not "0.8500000238418579"), so
// saved scenes stay readable and stable.
inline double jsonNumber(float value) {
    char text[32];
    auto [end,error]=std::to_chars(text,text+sizeof text-1,value);
    if(error!=std::errc{}) return value;
    *end='\0';
    return std::strtod(text,nullptr);
}
// Indented JSON whose short arrays (vectors, keyframes, ranges) stay on one line.
std::string dumpJson(const nlohmann::json& value);
EffectDef effectFromJson(const nlohmann::json& value);
nlohmann::json effectToJson(const EffectDef& effect);
Timeline timelineFromJson(const nlohmann::json& value);
nlohmann::json timelineToJson(const Timeline& timeline);
// Fields override `base`; serialization writes every field.
Environment environmentFromJson(const nlohmann::json& value,Environment base={});
nlohmann::json environmentToJson(const Environment& environment);
}
