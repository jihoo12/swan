#include "fuzzy.hpp"
#include <iostream>
#include <stdexcept>
void require(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {try {
    using swan::fuzzyScore;
    require(fuzzyScore("","anything").has_value(),"Empty query must match");
    require(!fuzzyScore("xyz","Save Scene").has_value(),"Missing characters matched");
    require(!fuzzyScore("evas","Save").has_value(),"Out-of-order characters matched");
    require(fuzzyScore("SAVE","save scene").has_value(),"Matching must ignore case");
    require(*fuzzyScore("save","Save Scene")>*fuzzyScore("save","Reset Avatar View Eye"),"Prefix/consecutive match must rank first");
    require(*fuzzyScore("ss","Save Scene")>*fuzzyScore("ss","Glass"),"Word starts must outrank inner letters");
    require(*fuzzyScore("ped","Crystal pedestal")>*fuzzyScore("ped","Speed up"),"Word-start match ranked too low");
    require(fuzzyScore("add cube","Add Cube").has_value(),"Spaces in queries must be ignored");
    require(*fuzzyScore("pe","Select Crystal pedestal")>*fuzzyScore("pe","Open Scene..."),"Mid-word starts must rank below word starts");
    require(*fuzzyScore("cube","Cube")>*fuzzyScore("cube","Cube with a much longer descriptive label"),"Shorter equal matches must rank first");
    std::cout<<"Fuzzy command matching passed\n";
} catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}}
