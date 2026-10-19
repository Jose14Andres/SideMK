#pragma once

// Static gain computer of the compressor, kept free of JUCE so it can be unit-tested
// in isolation. Pure function: no state, no allocation — safe on the audio thread.
namespace sidemk
{
    // Hard-knee transfer curve. Returns the gain change in dB (<= 0 : a cut) for an
    // envelope level envDb. Above threshold the output rises at 1/ratio dB per dB, so
    // the reduction is (envDb - threshold) * (1/ratio - 1); at or below it, 0 dB.
    // Expects ratio >= 1 (ratio == 1 → no reduction).
    inline float computeGainReductionDb (float envDb, float thresholdDb, float ratio) noexcept
    {
        const float overDb = envDb - thresholdDb;
        return overDb > 0.0f ? overDb * (1.0f / ratio - 1.0f) : 0.0f;
    }
}
