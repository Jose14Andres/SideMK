// Pruebas de la curva del modo pump. Sin JUCE: compila con cualquier C++17.
//
//   cl /nologo /std:c++17 /EHsc /I. tests\PumpCurveTests.cpp /Fe:PumpCurveTests.exe
//   .\PumpCurveTests.exe

#include "../PumpCurve.h"

#include <cmath>
#include <cstdio>

namespace
{
    int failures = 0;

    void expectNear (const char* name, double actual, double expected, double tolerance = 1.0e-9)
    {
        if (std::abs (actual - expected) > tolerance)
        {
            std::printf ("FAIL %s: got %.12f, expected %.12f\n", name, actual, expected);
            ++failures;
        }
    }

    void expectTrue (const char* name, bool condition)
    {
        if (! condition)
        {
            std::printf ("FAIL %s\n", name);
            ++failures;
        }
    }
} // namespace

int main()
{
    using namespace sidemk::pump;
    const double k = kDefaultCurveK;

    // --- wrapPhase / phaseFromPpq ---
    expectNear ("wrap 0",        wrapPhase (0.0),   0.0);
    expectNear ("wrap 1",        wrapPhase (1.0),   0.0);
    expectNear ("wrap 2.25",     wrapPhase (2.25),  0.25);
    expectNear ("wrap -0.25",    wrapPhase (-0.25), 0.75);
    expectTrue ("wrap tiny neg", wrapPhase (-1.0e-17) < 1.0);

    expectNear ("ppq 1/4 on beat",   phaseFromPpq (17.0, 1.0),       0.0);
    expectNear ("ppq 1/4 half beat", phaseFromPpq (16.5, 1.0),       0.5);
    expectNear ("ppq 1/16",          phaseFromPpq (0.375, 0.25),     0.5);
    expectNear ("ppq bar 3/4",       phaseFromPpq (10.5, 3.0, 9.0),  0.5);
    expectNear ("ppq zero division", phaseFromPpq (3.0, 0.0),        0.0);

    // --- dropFraction ---
    expectNear ("drop 120 bpm 1/4", dropFraction (120.0, 1.0),  0.002);      // 1 ms of 500 ms
    expectNear ("drop 128 bpm 1/4", dropFraction (128.0, 1.0),  1.0 / 468.75);
    expectNear ("drop capped",      dropFraction (120.0, 1.0, 10000.0), 0.5);
    expectNear ("drop no tempo",    dropFraction (0.0, 1.0),    0.0);

    // --- riseShape ---
    expectNear ("rise start",  riseShape (0.0, k), 0.0);
    expectNear ("rise end",    riseShape (1.0, k), 1.0);
    expectNear ("rise half",   riseShape (0.5, k), (1.0 - std::exp (-k * 0.5)) / (1.0 - std::exp (-k)));
    expectNear ("rise linear", riseShape (0.3, 0.0), 0.3);
    expectTrue ("rise clamps", riseShape (2.0, k) == 1.0 && riseShape (-1.0, k) == 0.0);

    bool monotonic = true;
    for (int i = 1; i <= 1000; ++i)
        monotonic = monotonic && riseShape (i / 1000.0, k) >= riseShape ((i - 1) / 1000.0, k);
    expectTrue ("rise monotonic", monotonic);

    // --- pumpShape / pumpGain ---
    const double drop = dropFraction (120.0, 1.0);
    expectNear ("shape on beat",       pumpShape (0.0, drop, k),        1.0);
    expectNear ("shape mid drop",      pumpShape (drop * 0.5, drop, k), 0.5);
    expectNear ("shape end of drop",   pumpShape (drop, drop, k),       0.0);
    expectNear ("shape no drop start", pumpShape (0.0, 0.0, k),         0.0);
    expectTrue ("shape near end ~1",   pumpShape (0.9999, drop, k) > 0.999);

    expectNear ("gain depth 0",   pumpGain (drop, 0.0, drop, k),  1.0);
    expectNear ("gain depth 1",   pumpGain (drop, 1.0, drop, k),  0.0);
    expectNear ("gain depth 0.5", pumpGain (drop, 0.5, drop, k),  0.5);
    expectNear ("gain clamps",    pumpGain (drop, 2.0, drop, k),  0.0);

    // Continuity at the cycle boundary: end of one cycle ~ start of the next
    expectTrue ("wrap continuity", std::abs (pumpGain (0.99999, 1.0, drop, k) - pumpGain (0.0, 1.0, drop, k)) < 1.0e-3);

    if (failures == 0)
        std::printf ("PumpCurveTests: all passed\n");

    return failures == 0 ? 0 : 1;
}
