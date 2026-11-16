#pragma once
#include "ExerciseView.h"
#include "core/Fingerings.h"

// Tuner: note name, cents needle, frequency.
class TunerView : public juce::Component
{
public:
    void setPitch (const PitchFrame& f);
    void paint (juce::Graphics&) override;
private:
    PitchFrame frame;
    float smoothCents = 0.0f;
    juce::uint32 lastValid = 0;
};

// Metronome controls; the clicks themselves come from the audio engine.
class MetronomePanel : public juce::Component, private juce::Timer
{
public:
    explicit MetronomePanel (AudioEngine&);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    AudioEngine& engine;
    juce::Slider bpm, volume;
    juce::ComboBox beats;
    juce::TextButton startButton { "Start" }, tapButton { "Tap" }, slower { "-5" }, faster { "+5" };
    juce::Array<juce::uint32> taps;
    int shownPulse = -2;
    void timerCallback() override;
};

// The neck with every note named, by letter or by interval from a chosen root, plus the note-finder drill.
class FretboardExplorer : public juce::Component, public LiveInput, private juce::Timer
{
public:
    FretboardExplorer (AudioEngine&, Progress&);
    void notePlayed (const NoteEvent&) override;
    void pitchChanged (const PitchFrame&) override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void startDrill (juce::int64 seed = 0);   // public for the snapshot tool

private:
    AudioEngine& engine;
    Progress& progress;
    FretboardView board;
    juce::ComboBox root, labels, show;
    juce::TextButton drillButton { "Start note finder" };
    juce::String info;

    // drill: play one note on each string, 6th to 1st
    bool drilling = false;
    int drillPc = 0, drillString = 6;
    double drillStart = 0;
    std::vector<FretMark> found;
    juce::Random rng;
    juce::String drillResult;

    void updateMarks();
    void timerCallback() override { repaint(); }
};

// Scales (any fingering, run patterns) or technique drills, checked note by note with pitch detection.
// Shows the next note, scores accuracy and evenness of timing, and with the tempo ladder raises the
// metronome 5 BPM after each clean run (and lowers it after a sloppy one).
class PatternTrainer : public juce::Component, public LiveInput, private juce::Timer
{
public:
    enum class Mode { scales, technique };
    PatternTrainer (AudioEngine&, Progress&, Mode);
    void notePlayed (const NoteEvent&) override;
    void pitchChanged (const PitchFrame&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

    // for the snapshot tool and tests
    void startRun();
    void simulateRun (int hits, int mistakes, double bpm);
    void select (int rootPc, const juce::String& scaleId, int fingeringIndex, int patternIndex);
    void selectDrill (int drill);

private:
    AudioEngine& engine;
    Progress& progress;
    const Mode mode;
    FretboardView board;
    juce::ComboBox root, scale, fingering, pattern, labels, notesPerBeat, startFret;
    juce::ToggleButton ladder { "Tempo ladder: +5 BPM after a clean run" };
    juce::TextButton startButton { "Start" }, listenButton { "Listen" };

    std::vector<Fingering> fingerings;
    std::vector<FretNote> targets;
    std::array<juce::String, 12> spelling;
    bool running = false;
    int pos = 0, mistakes = 0;
    std::vector<double> times;
    juce::String result;
    juce::Colour resultColour;
    std::vector<FretMark> flashMarks;
    juce::uint32 flashUntil = 0;

    const theory::ScaleType& currentScale() const;
    theory::Note rootNote() const;
    int npb() const { return juce::jmax (1, notesPerBeat.getSelectedId()); }
    juce::String drillKey() const;
    juce::String label (const FretNote&) const;
    void rebuildFingerings();
    void rebuildTargets();
    void updateMarks();
    void stopRun();
    void finishRun (double bpmOverride = -1.0);
    void timerCallback() override;
};

class PracticePage : public juce::Component, public LiveInput
{
public:
    enum Tab { fretboardTab, scalesTab, techniqueTab };
    PracticePage (AudioEngine&, Progress&);
    void notePlayed (const NoteEvent& e) override;
    void pitchChanged (const PitchFrame& f) override;
    FretboardExplorer& getExplorer() { return explorer; }
    PatternTrainer& getScales() { return scales; }
    PatternTrainer& getTechnique() { return technique; }
    void showTab (int tab);
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TunerView tuner;
    MetronomePanel metronome;
    FretboardExplorer explorer;
    PatternTrainer scales, technique;
    juce::TextButton tabButtons[3];
    int currentTab = fretboardTab;
};
