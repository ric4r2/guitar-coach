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
};

// Runs one exercise: ten questions, scored, then a summary. The first answer to each question counts;
// for played, clicked-on-the-neck and written answers you may keep trying until you find it.
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

    // for the snapshot tool: answer the current question with the right or a wrong choice
    void simulateChoice (bool correct);
    void simulateFinish (int score);

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
    std::vector<FretMark> wrongMarks;

    void nextQuestion();
    void showQuestion();
    void attempt (bool correct, const juce::String& hint, bool isChoice, int choice = -1);
    void reveal (bool correct);
    void finish();
    void timerCallback() override;
    void updateButtons();
};
