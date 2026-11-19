#include <JuceHeader.h>

// Single entry point for the "tests" console app. Every juce::UnitTest declared
// as a static object in the target's sources (Tests.cpp, PluginProcessorTest.cpp,
// PumpModeTest.cpp) registers itself and runs here.
int main()
{
    juce::ScopedJuceInitialiser_GUI juceInitialiser;

    juce::UnitTestRunner runner;
    runner.runAllTests();

    for (int i = 0; i < runner.getNumResults(); ++i)
    {
        if (runner.getResult (i)->failures > 0)
            return 1;
    }
    return 0;
}
