#include "fuzzy.hpp"
#include <cctype>
namespace swan {
std::optional<int> fuzzyScore(std::string_view query,std::string_view text) {
    auto lower=[](char c){return char(std::tolower(static_cast<unsigned char>(c)));};
    int score=0;size_t position=0;bool previousMatched=false,first=true;
    for(char raw:query) {
        if(raw==' ') {previousMatched=false;continue;}
        char wanted=lower(raw);
        size_t start=position;
        while(position<text.size() && lower(text[position])!=wanted) ++position;
        if(position==text.size()) return std::nullopt;
        bool wordStart=position==0 || !std::isalnum(static_cast<unsigned char>(text[position-1]))
            || (std::isupper(static_cast<unsigned char>(text[position])) && std::islower(static_cast<unsigned char>(text[position-1])));
        score+=1;
        if(position==0) score+=12;
        else if(wordStart) score+=8;
        else if(first) score-=6; // A query rarely starts in the middle of a word.
        if(previousMatched && position==start) score+=6;
        score-=int(std::min<size_t>(position-start,4));
        previousMatched=true;first=false;++position;
    }
    // Prefer shorter candidates among equal matches.
    return score-int(std::min<size_t>(text.size()/8,6));
}
}
