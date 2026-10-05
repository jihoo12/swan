#pragma once
#include "fx.hpp"
#include "timeline.hpp"
#include <string>
#include <string_view>
namespace swan {
// JSON text of one effect (`{"emitters": [...]}`) or a timeline, in the scene file's schema.
// Parsing validates structure and reports the offending field; serialization omits defaults.
EffectDef parseEffect(std::string_view json);
std::string serializeEffect(const EffectDef& effect);
Timeline parseTimeline(std::string_view json);
std::string serializeTimeline(const Timeline& timeline);
}
