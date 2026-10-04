#pragma once
#include <optional>
#include <string_view>
namespace swan {
// Case-insensitive subsequence score for search UIs; higher is better, nullopt when not every
// query character appears in order. Rewards prefix, word-start, and consecutive matches.
std::optional<int> fuzzyScore(std::string_view query,std::string_view text);
}
