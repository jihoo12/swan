#pragma once
#include <algorithm>
#include <cmath>
namespace swan {
class FixedStep {
public:
    static constexpr double interval=1.0/120.0;
    template<class Update> int advance(double elapsed,Update update) {
        if(!std::isfinite(elapsed) || elapsed<0) return 0;
        accumulator+=std::min(elapsed,0.25);
        int steps=0;
        while(accumulator+1e-12>=interval && steps<30) {
            update(float(interval)); accumulator-=interval; ++steps;
        }
        accumulator=std::max(accumulator,0.0);
        return steps;
    }
    double remainder() const { return accumulator; }
private:
    double accumulator=0;
};
}
