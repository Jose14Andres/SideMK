#include "../GainComputer.h"

#include <cmath>
#include <iostream>

namespace
{
    bool expectNear (const char* name, float actual, float expected)
    {
        constexpr float tolerance = 1.0e-5f;
        if (std::abs (actual - expected) <= tolerance)
            return true;

        std::cerr << name << ": expected " << expected << ", got " << actual << '\n';
        return false;
    }
}

int main()
{
    bool passed = true;

    // At and below threshold, hard-knee compression must leave the signal unchanged.
    passed &= expectNear ("below threshold",
                          sidemk::computeGainReductionDb (-13.0f, -12.0f, 4.0f), 0.0f);
    passed &= expectNear ("at threshold",
                          sidemk::computeGainReductionDb (-12.0f, -12.0f, 4.0f), 0.0f);

    // 12 dB over threshold at 4:1 yields 9 dB of gain reduction.
    passed &= expectNear ("4:1 ratio",
                          sidemk::computeGainReductionDb (0.0f, -12.0f, 4.0f), -9.0f);

    // A ratio of 1:1 must never change gain, regardless of level above threshold.
    passed &= expectNear ("1:1 ratio",
                          sidemk::computeGainReductionDb (6.0f, -12.0f, 1.0f), 0.0f);

    // Check the slope: each additional 4 dB above threshold adds 3 dB of reduction.
    const auto lower = sidemk::computeGainReductionDb (-8.0f, -12.0f, 4.0f);
    const auto higher = sidemk::computeGainReductionDb (-4.0f, -12.0f, 4.0f);
    passed &= expectNear ("4:1 slope", higher - lower, -3.0f);

    if (! passed)
        return 1;

    std::cout << "GainComputer tests passed\n";
    return 0;
}
