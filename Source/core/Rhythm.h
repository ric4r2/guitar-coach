#pragma once
#include <juce_core/juce_core.h>
#include <vector>

// Rhythm reading: rhythm cells (one beat split in three or four pulses), random rhythms to read, and scoring
// taps against the written rhythm.

struct RhythmEvent
{
    double beat = 0.0, length = 1.0;   // in beats (quarter notes)
};

// One beat split into 3 or 4 pulses, with notes starting on some of them.
struct RhythmCell
{
    juce::String id, name, syllables;   // "swing", "Swing", "1 - let"
    int pulses = 4;
    std::vector<int> hits;              // pulses where a note starts (0 = on the beat)
};

const std::vector<RhythmCell>& threePulseCells();   // triplet, swing, Afro-Cuban, waltz
const std::vector<RhythmCell>& fourPulseCells();    // four sixteenths, eighths, 1 & a, 1 e &, 1 a
std::vector<RhythmEvent> cellEvents (const RhythmCell&, double startBeat);

// A rhythm to read, starting with a note on beat 1.
//  level 1: quarters, eighths, halves and quarter rests
//  level 2: + dotted quarters, syncopation (eighth-quarter-eighth), eighth rests
//  level 3: + sixteenth cells and triplets
std::vector<RhythmEvent> randomRhythm (int level, int beats, juce::Random&);

// How a note value is named and how long it is: "dotted quarter" = 1.5
struct NoteValue { juce::String name; double beats; };
const std::vector<NoteValue>& noteValues();

// Taps against the written rhythm at a known tempo. The taps may be shifted by any amount (latency, starting
// late): the best offset is removed first. Each error is measured against an eighth note.
struct TapResult
{
    double score = 0.0;                  // 0..1, missing or extra taps count as fully wrong
    std::vector<double> errorsInBeats;   // per written note that was tapped: + late, - early
    int missing = 0, extra = 0;
};
TapResult scoreTaps (const std::vector<RhythmEvent>& written, const std::vector<double>& tapSeconds, double bpm);
