#pragma once

// Tempo-synced volume "pump" curve.
//
// Pure functions, no JUCE dependency, so the curve can be unit-tested with any
// C++17 compiler (see tests/PumpCurveTests.cpp).
//
// One cycle of the curve spans one musical division (e.g. one quarter note).
// The phase runs from 0 to 1 across that division:
//
//   gain
//    1 |\                ____________
//      | \          ____/
//      |  \      __/
//      |   \   _/
//      |    \ /
//  1-d |     '
//      +-----+----------------------------> phase
//      0   drop                         1
//
//   - [0, drop):  short linear fall from 1 to (1 - depth). It only exists to avoid
//                 the click a hard step would produce; its length is fixed in
//                 milliseconds (kDropMs) and converted to a phase fraction.
//   - [drop, 1):  recovery  1 - exp(-k * t), normalised so it ends exactly at 1.
//
// k is a working constant (starting point agreed in docs/contexto-plugin.md). It is
// expected to be re-fitted once a reference bounce has been measured.

#include <algorithm>
#include <cmath>

namespace sidemk::pump
{
    // Curve steepness. Higher k = faster recovery after the dip.
    constexpr double kDefaultCurveK = 6.5;

    // Length of the anti-click fall at the start of every cycle.
    constexpr double kDropMs = 1.0;

    // Tempo used when the host does not report one (e.g. the Standalone app).
    constexpr double kFallbackBpm = 120.0;

    // Wraps any real number into [0, 1). Works for negative input (host pre-roll).
    inline double wrapPhase (double x) noexcept
    {
        x -= std::floor (x);
        return x >= 1.0 ? 0.0 : x; // guards the rounding case floor(x) == x - 1 + eps
    }

    // Phase of a PPQ position inside a cycle of `divisionPpq` quarter notes that
    // starts at `originPpq`.
    inline double phaseFromPpq (double ppq, double divisionPpq, double originPpq = 0.0) noexcept
    {
        if (divisionPpq <= 0.0)
            return 0.0;

        return wrapPhase ((ppq - originPpq) / divisionPpq);
    }

    // Fraction of one cycle taken by the anti-click fall. Capped at half a cycle
    // so very fast rates still have a recovery segment.
    inline double dropFraction (double bpm, double divisionPpq, double dropMs = kDropMs) noexcept
    {
        if (bpm <= 0.0 || divisionPpq <= 0.0 || dropMs <= 0.0)
            return 0.0;

        const double cycleMs = divisionPpq * 60000.0 / bpm;
        return std::min (dropMs / cycleMs, 0.5);
    }

    // Recovery segment: maps t in [0, 1] to [0, 1], 0 at t = 0 and exactly 1 at t = 1.
    inline double riseShape (double t, double k) noexcept
    {
        t = std::clamp (t, 0.0, 1.0);

        if (k <= 1.0e-9)
            return t; // k -> 0 degenerates to a straight line

        return (1.0 - std::exp (-k * t)) / (1.0 - std::exp (-k));
    }

    // Full-depth shape in [0, 1]: 1 = untouched, 0 = fully ducked.
    inline double pumpShape (double phase, double drop, double k) noexcept
    {
        phase = std::clamp (phase, 0.0, 1.0);

        if (drop > 0.0 && phase < drop)
            return 1.0 - phase / drop;

        const double span = 1.0 - drop;
        return riseShape (span > 0.0 ? (phase - drop) / span : 1.0, k);
    }

    // Linear gain to apply to the signal. depth in [0, 1]: 0 = no effect,
    // 1 = the dip goes all the way to silence.
    inline double pumpGain (double phase, double depth, double drop, double k) noexcept
    {
        depth = std::clamp (depth, 0.0, 1.0);
        return 1.0 - depth * (1.0 - pumpShape (phase, drop, k));
    }
} // namespace sidemk::pump
