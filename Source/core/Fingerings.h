#pragma once
#include "Theory.h"
#include <array>

// Where scales sit on the neck, and the note sequences for scale and technique drills.

struct FretNote
{
    int string = 6, fret = 0, midi = 40;
    int degree = 0;    // scale: index of the degree (0 = root). Technique drills: the finger, 1 to 4.
};

struct Fingering
{
    juce::String name;               // "E shape", "Box 1", "3 per string from 3"
    int lowFret = 0, highFret = 0;
    std::vector<FretNote> notes;     // ascending pitch
};

// Positions that cover the scale in a 4 to 5 fret window.
//  7-note scales: the five CAGED shapes of the relative major (C, A, G, E and D shapes).
//  Pentatonic and blues: five boxes, box 1 starting on the root on the 6th string.
// Sorted from the lowest fret up.
std::vector<Fingering> positionFingerings (int rootPc, const theory::ScaleType&);

// Three notes per string: seven patterns, one starting on each degree on the 6th string (7-note scales only).
std::vector<Fingering> threeNpsFingerings (int rootPc, const theory::ScaleType&);

// Spelled name of each pitch class in the scale (for labels), indexed by pitch class; empty when not in the scale.
std::array<juce::String, 12> scaleSpelling (const theory::Note& root, const theory::ScaleType&);

enum class RunPattern { up, upDown, threes, fours, thirds };
const juce::StringArray& runPatternNames();
std::vector<FretNote> scaleRun (const Fingering&, RunPattern);

enum class TechDrill { chromatic, spider, stringSkip };
const juce::StringArray& techDrillNames();
// four frets from startFret (index finger), up across the strings and back down
std::vector<FretNote> techniqueDrill (TechDrill, int startFret);

// How a run went: pitch accuracy and evenness of timing (1 = perfect).
struct RunScore
{
    int notes = 0, mistakes = 0;
    double accuracy = 0.0;      // notes / (notes + mistakes)
    double evenness = 0.0;      // 1 - average timing error relative to the expected note length
    double averageBpm = 0.0;    // from the median gap between notes, at the given notes per beat
    bool clean = false;         // accuracy >= 95% and evenness >= 75% (and close to the tempo, when one was set)
};

// times: seconds when each target note was played. bpm <= 0: no metronome, judge evenness against the median gap.
RunScore scoreRun (const std::vector<double>& times, int mistakes, double bpm, int notesPerBeat);
