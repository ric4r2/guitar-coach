#pragma once
#include "TheoryPage.h"
#include "BluesChartView.h"
#include "core/Licks.h"
#include "core/Fingerings.h"

// Guitar tab under a rhythm staff: fret numbers lined up with the notes above.
class TabView : public juce::Component
{
public:
    struct Entry { int string = 1; double beat = 0; juce::String text, above; juce::Colour colour { 0xff1d1f23 }; };
    void setEntries (std::vector<Entry> e) { entries = std::move (e); repaint(); }
    std::function<float (double beat)> xForBeat;
    void paint (juce::Graphics&) override;
private:
    std::vector<Entry> entries;
};

// Backing tracks: key, form, feel, tempo and parts; the chart follows the band; the neck shows what to play.
class BackingPanel : public juce::Component, public LiveInput, private juce::Timer
{
    friend struct PathTester;   // Tests/Paths.cpp drives every screen
public:
    explicit BackingPanel (AudioEngine&);
    ~BackingPanel() override;
    void pitchChanged (const PitchFrame&) override;
    void paint (juce::Graphics&) override;
    void resized() override;
    void showBar (int bar);   // for the snapshot tool: pretend the band is in this bar

private:
    AudioEngine& engine;
    juce::ComboBox key, form, feel, scaleMap;
    juce::Slider tempo, volume;
    juce::ToggleButton drums { "Drums" }, bass { "Bass" }, guitar { "Guitar" }, keys { "Organ" };
    juce::TextButton playButton { "Play" };
    BluesChartView chart;
    FretboardView board;
    int shownBar = -2;

    BackingSettings current() const;
    void push();
    void updateMarks (int bar);
    void timerCallback() override;
};

// The lick library: notation and tab, the neck, listen at any tempo, then play it back and get scored.
class LickPanel : public juce::Component, public LiveInput, private juce::Timer
{
    friend struct PathTester;   // Tests/Paths.cpp drives every screen
public:
    LickPanel (AudioEngine&, Progress&);
    void notePlayed (const NoteEvent&) override;
    void pitchChanged (const PitchFrame&) override;
    void paint (juce::Graphics&) override;
    void resized() override;

    void selectLick (int index);
    void selectLickById (const juce::String& id);
    void startTurn();
    void simulateTurn (int wrongNotes);   // for the snapshot tool

private:
    AudioEngine& engine;
    Progress& progress;
    std::vector<Lick> licks;
    int current = 0;
    Lick lick;                            // the current lick in the chosen key
    juce::OwnedArray<juce::TextButton> lickButtons;
    juce::ComboBox key;
    juce::Slider tempo;
    juce::ToggleButton withBacking { "With backing track" };
    juce::TextButton listenButton { "Listen" }, turnButton { "Your turn" };
    StaffView staff;
    TabView tab;
    FretboardView board;
    LickFollower follower;
    bool running = false;
    juce::String result;
    juce::Colour resultColour;

    void rebuild();
    void updateMarks();
    void refreshList();
    void finishTurn();
    int keyPc() const { return key.getSelectedId() - 1; }
    juce::String degree (int midi) const;
    theory::Note spell (int midi) const;
    bool swung() const { return lick.feel == "shuffle"; }
    void timerCallback() override { repaint(); }
};

class ImprovPage : public juce::Component, public LiveInput
{
    friend struct PathTester;   // Tests/Paths.cpp drives every screen
public:
    enum Tab { lessonsTab, backingTab, licksTab };
    ImprovPage (AudioEngine&, Progress&, const std::vector<Lesson>& bluesLessons);
    void notePlayed (const NoteEvent&) override;
    void pitchChanged (const PitchFrame&) override;
    void onsetDetected (double t) override { if (currentTab == lessonsTab) lessons.onsetDetected (t); }
    void showTab (int tab);
    TheoryPage& getLessons() { return lessons; }
    BackingPanel& getBacking() { return backing; }
    LickPanel& getLicks() { return licksPanel; }
    std::function<void()> onProgressChanged;
    void resized() override;

private:
    TheoryPage lessons;
    BackingPanel backing;
    LickPanel licksPanel;
    juce::TextButton tabButtons[3];
    int currentTab = lessonsTab;
};
