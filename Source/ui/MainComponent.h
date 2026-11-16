#pragma once
#include "TheoryPage.h"
#include "PracticePage.h"
#include "ImprovPage.h"
#include "StatsPage.h"
#include "core/Planner.h"

// One item of today's session: minutes, what to do, done or Start.
class PlanRow : public juce::Component
{
public:
    PlanItem item;
    bool done = false;
    std::function<void()> onClick;
    void paint (juce::Graphics&) override;
    void mouseUp (const juce::MouseEvent& e) override { if (e.mouseWasClicked() && onClick) onClick(); }
    void mouseEnter (const juce::MouseEvent&) override { repaint(); }
    void mouseExit (const juce::MouseEvent&) override { repaint(); }
};
#include <juce_audio_utils/juce_audio_utils.h>

class HomePage : public juce::Component
{
public:
    HomePage (Progress&, const std::vector<Lesson>& theory, const std::vector<Lesson>& blues);
    std::function<void (int lesson)> onContinue;
    std::function<void (int page)> onGoTo;
    std::function<void (const PlanItem&)> onPlanItem;
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    Progress& progress;
    const std::vector<Lesson>& lessons;
    const std::vector<Lesson>& bluesLessons;
    std::vector<Lick> licks;
    juce::TextButton continueButton, practiceButton { "Open Practice" };
    juce::ComboBox length;
    std::vector<PlanItem> plan;
    juce::OwnedArray<PlanRow> rows;
    int nextLesson = 0;
};

class AudioPage : public juce::Component, public LiveInput, private juce::Timer
{
public:
    explicit AudioPage (AudioEngine&);
    void pitchChanged (const PitchFrame& f) override { frame = f; }
    void notePlayed (const NoteEvent& e) override { lastNote = e; lastNoteTime = juce::Time::getMillisecondCounter(); }
    void paint (juce::Graphics&) override;
    void resized() override;
private:
    AudioEngine& engine;
    juce::AudioDeviceSelectorComponent selector;
    juce::ComboBox channel;
    juce::TextButton testButton { "Play test notes" };
    juce::Slider outVolume;
    PitchFrame frame;
    NoteEvent lastNote;
    juce::uint32 lastNoteTime = 0;
    float meter = 0.0f;
    void timerCallback() override;
};

class MainComponent : public juce::Component, private juce::Timer
{
public:
    enum Page { home, theory, improv, practice, stats, audio, numPages };

    MainComponent (AudioEngine&, Progress&);
    ~MainComponent() override;
    void showPage (int page);
    TheoryPage& getTheoryPage() { return theoryPage; }
    PracticePage& getPracticePage() { return practicePage; }
    ImprovPage& getImprovPage() { return improvPage; }

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioEngine& engine;
    Progress& progress;
    CoachLook look;
    std::vector<Lesson> allLessons, lessons, bluesLessons;   // lessons: the Music Theory track

    HomePage homePage;
    TheoryPage theoryPage;
    ImprovPage improvPage;
    PracticePage practicePage;
    StatsPage statsPage;
    AudioPage audioPage;
    juce::OwnedArray<juce::TextButton> tabs;
    int current = home;
    PitchFrame lastFrame;
    float headerLevel = 0.0f;
    std::vector<Lick> licks;
    double lastActivity = -1.0e9, lastTick = 0.0, unsentSeconds = 0.0;
    void openPlanItem (const PlanItem&);

    juce::Component* pageComponent (int p);
    LiveInput* pageInput (int p);
    void timerCallback() override;
};
