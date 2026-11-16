#pragma once
#include "PitchDetector.h"
#include <array>
#include <atomic>
#include <vector>

// A note the player has just played: reported once when the pitch has been steady for a few frames.
struct NoteEvent
{
    int midi = 0;
    float cents = 0.0f;   // how far off the equal-tempered pitch
    float hz = 0.0f;
    double time = 0.0;    // seconds of input since the analyzer started, for timing between notes
};

// Latest reading of the input, for meters and the tuner.
struct PitchFrame
{
    bool valid = false;
    float hz = 0.0f;
    float midi = 0.0f;     // fractional MIDI note
    float confidence = 0.0f;
    float level = 0.0f;    // RMS of the input
};

// Turns guitar input into pitch readings and note events.
//  push()    : audio thread. Low-pass, decimate to about 24 kHz, write into a ring buffer.
//  analyze() : analysis thread (or a test). Runs YIN on the newest frame when a hop of new samples is in.
// One writer and one reader, no locks.
class InputAnalyzer
{
public:
    static constexpr int ringSize = 16384;
    static constexpr int hop = 256;            // decimated samples between analyses (about 10 ms)
    static constexpr int stableFrames = 3;     // frames the same note must last to count as played
    static constexpr int silenceFrames = 4;
    static constexpr float minLevel = 0.004f;    // RMS, about -48 dBFS

    void prepare (double inputSampleRate);
    void push (const float* x, int n);

    // Returns true when a new frame was analysed. A new note, if any, is put in `event`.
    bool analyze (NoteEvent& event, bool& gotEvent);

    PitchFrame getLatest() const;
    double getAnalysisRate() const { return analysisRate; }
    float takeInputPeak() { return inputLevel.exchange (0.0f); }   // peak since last asked, for meters

private:
    struct Biquad
    {
        float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void lowpass (double sr, double fc, double q);
        float process (float x)
        {
            const float y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    // audio thread side
    Biquad lp1, lp2;
    int decimation = 2, decimCounter = 0;
    std::vector<float> ring = std::vector<float> ((size_t) ringSize, 0.0f);
    std::atomic<long long> written { 0 };
    std::atomic<float> inputLevel { 0.0f };

    // analysis side
    double analysisRate = 24000.0;
    long long lastAnalysed = 0;
    PitchDetector detector;
    std::vector<float> frame = std::vector<float> ((size_t) PitchDetector::frameSize, 0.0f);
    int candidate = -1, candidateCount = 0, emitted = -1, quietCount = 0;
    float slowLevel = 0.0f;
    bool retrigger = false;
    int framesSinceOnset = 1000;

    mutable std::atomic<bool> latestValid { false };
    std::atomic<float> latestHz { 0 }, latestMidi { 0 }, latestConf { 0 }, latestLevel { 0 };
};
