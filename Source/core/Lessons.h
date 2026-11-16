#pragma once
#include "Theory.h"
#include <functional>

// Lessons come from Content/lessons.json (embedded). Each exercise turns into questions here.

struct ExerciseSpec
{
    juce::String type, title;
    juce::var params;
};

struct Lesson
{
    juce::String id, strand, title, summary;
    int level = 1;
    juce::StringArray body;      // "## heading", "- bullet" or a paragraph
    juce::var diagram;
    std::vector<ExerciseSpec> exercises;

    juce::String exerciseKey (int index) const { return id + "/" + juce::String (index); }
};

std::vector<Lesson> loadLessons();                        // from the embedded file
std::vector<Lesson> parseLessons (const juce::String& json);

// A marked spot on the fretboard.
struct FretMark
{
    int string = 6, fret = 0;
    juce::String label;
    int colour = 0;    // 0 = normal, 1 = root, 2 = target / correct, 3 = wrong, 4 = hint
};

// One question, whatever its type. The view shows whatever parts are filled in, and sends answers
// back through the judge functions. The first answer counts for the score.
struct Question
{
    juce::String prompt;

    // staff: sounding pitches (the staff shows them an octave higher, as guitar music is written)
    std::vector<theory::Note> staffNotes;
    bool staffClick = false;            // answer by placing a note on the staff
    bool staffAccidentalButtons = false;

    bool showFretboard = false;
    std::vector<FretMark> fretMarks;
    int maxFret = 12;
    bool fretClick = false;             // answer by clicking a fret

    juce::StringArray choices;
    int correctChoice = -1;
    int choiceColumns = 0;              // 0 = automatic

    bool acceptsPlaying = false;        // answer by playing the guitar
    std::vector<int> listenMidi;        // what the Listen button plays (in order)
    bool autoListen = false;

    std::function<bool (int choice)> judgeChoice;   // when empty, only correctChoice is right
    std::function<bool (int midi)> judgePlayed;
    std::function<bool (int string, int fret)> judgeFret;
    std::function<bool (const theory::Note& sounding)> judgeStaff;

    juce::String explanation;                     // shown after answering
    std::vector<FretMark> revealMarks;            // shown after answering
    std::vector<theory::Note> revealStaff;        // replaces staffNotes after answering, when not empty
    juce::String wrongPlayHint (int playedMidi) const;
    int expectedMidi = -1;                         // for "you played X" feedback
};

Question makeQuestion (const ExerciseSpec& spec, juce::Random& rng);
juce::String describeExerciseInput (const ExerciseSpec& spec);   // "click or play", for the exercise list

// Choice labels used by note-name questions
juce::StringArray naturalChoices();
juce::StringArray accidentalChoices();   // C C♯ D♭ D ... (17 spellings)
