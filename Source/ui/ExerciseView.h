#pragma once
#include "StaffView.h"
#include "FretboardView.h"
#include "audio/AudioEngine.h"
#include "core/Progress.h"

// Anything that wants to hear the guitar: notes as they are played, and the running pitch.
struct LiveInput
{
    virtual ~LiveInput() = default;
    virtual void notePlayed (const NoteEvent&) {}
    virtual void pitchChanged (const PitchFrame&) {}
    virtual void onsetDetected (double /*inputSeconds*/) {}
};

// Runs one exercise: ten questions, scored, then a summary. The first answer to each question counts;
// for played, clicked-on-the-neck and written answers you may keep trying until you find it.
// A big pad to tap rhythms on: reacts on mouse-down, not on release, so the timing is right.
class TapPad : public juce::Component
{
public:
    std::function<void()> onTap;
    void setText (const juce::String& t, juce::Colour c) { text = t; colour = c; repaint(); }
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (2.0f);
        g.setColour (colour.withAlpha (flash > 0 ? 0.45f : 0.2f));
        g.fillRoundedRectangle (r, 10.0f);
        g.setColour (colour);
        g.drawRoundedRectangle (r, 10.0f, 2.0f);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (19.0f, true));
        g.drawFittedText (text, getLocalBounds().reduced (12, 4), juce::Justification::centred, 2);
    }
    void mouseDown (const juce::MouseEvent&) override { pulse(); if (onTap) onTap(); }
    void pulse() { flash = 1; repaint(); juce::Timer::callAfterDelay (90, [safe = juce::Component::SafePointer<TapPad> (this)] { if (safe != nullptr) { safe->flash = 0; safe->repaint(); } }); }
private:
    juce::String text;
    juce::Colour colour { 0xff5aa7d8 };
    int flash = 0;
};

class ExerciseView : public juce::Component, public LiveInput, private juce::Timer
{
public:
    static constexpr int questionsPerRound = 10;

    ExerciseView (AudioEngine&, Progress&);

    void start (const Lesson& lesson, int exerciseIndex, juce::int64 seed = 0);
    std::function<void()> onBack;      // back to the lesson
    std::function<void()> onFinished;  // a round was completed (progress changed)

    void notePlayed (const NoteEvent&) override;
    void pitchChanged (const PitchFrame&) override;
    void onsetDetected (double inputSeconds) override;
    bool keyPressed (const juce::KeyPress&) override;
    void simulateTaps (double jitterBeats);   // for the snapshot tool

    // for the snapshot tool: answer the current question with the right or a wrong choice
    void simulateChoice (bool correct);
    void simulateFinish (int score);
    void simulateSequence (int notes);   // give the next notes of a sequence answer, correctly

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioEngine& engine;
    Progress& progress;
    const Lesson* lesson = nullptr;
    int exerciseIndex = 0;
    ExerciseSpec spec;
    juce::Random rng;

    Question q;
    int questionNumber = 0, score = 0;
    bool done = false, missed = false, finished = false;
    juce::uint32 questionShownAt = 0;
    juce::String feedback;
    juce::Colour feedbackColour;

    StaffView staff;
    FretboardView fretboard;
    juce::TextButton backButton { "< Lesson" }, listenButton { "Listen" }, nextButton { "Next" }, againButton { "Try again" };
    juce::TextButton flatButton, naturalButton, sharpButton;
    juce::OwnedArray<juce::TextButton> choiceButtons;
    std::vector<FretMark> wrongMarks, enteredMarks;
    int seqPos = 0;   // next note of a sequence answer

    // rhythm tapping
    TapPad tapPad;
    std::vector<double> taps;
    int tapSource = -1;               // 0 = keyboard or pad (UI clock), 1 = guitar (input clock)
    int attemptId = 0;
    double countInStartedAt = -1.0;
    void tap (double seconds, int source);
    void evaluateTaps();
    void startCountIn();
    void showRhythm (const std::vector<juce::Colour>& colours = {});
    void updateTapPad();

    void nextQuestion();
    void showQuestion();
    void attempt (bool correct, const juce::String& hint, bool isChoice, int choice = -1);
    void reveal (bool correct);
    void finish();
    void timerCallback() override;
    void updateButtons();
    void playListen();
    void showSequenceSoFar();
    void sequenceAnswer (bool ok, const juce::String& wrongText);
    bool sequenceMatches (int midi) const;
};
