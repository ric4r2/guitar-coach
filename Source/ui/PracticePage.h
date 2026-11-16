#pragma once
#include "ExerciseView.h"

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

class PracticePage : public juce::Component, public LiveInput
{
public:
    PracticePage (AudioEngine&, Progress&);
    void notePlayed (const NoteEvent& e) override { explorer.notePlayed (e); }
    void pitchChanged (const PitchFrame& f) override { tuner.setPitch (f); explorer.pitchChanged (f); }
    FretboardExplorer& getExplorer() { return explorer; }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    TunerView tuner;
    MetronomePanel metronome;
    FretboardExplorer explorer;
};
