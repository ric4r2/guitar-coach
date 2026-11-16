#pragma once
#include "ExerciseView.h"
#include "CircleView.h"
#include "BluesChartView.h"

// Lesson list on the left; on the right the lesson (diagram, text, exercises) or a running exercise.
class TheoryPage : public juce::Component, public LiveInput
{
public:
    TheoryPage (AudioEngine&, Progress&, const std::vector<Lesson>&);

    void openLesson (int index);
    void openExercise (int lessonIndex, int exerciseIndex, juce::int64 seed = 0);
    ExerciseView& getExerciseView() { return exercise; }
    int firstUnfinishedLesson() const;
    std::function<void()> onProgressChanged;

    void notePlayed (const NoteEvent& e) override;
    void pitchChanged (const PitchFrame& f) override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    AudioEngine& engine;
    Progress& progress;
    const std::vector<Lesson>& lessons;
    int current = 0;

    juce::OwnedArray<juce::TextButton> lessonButtons;
    juce::OwnedArray<juce::TextButton> exerciseButtons;
    StaffView diagramStaff;
    FretboardView diagramFret;
    CircleView circle;
    BluesChartView chartDiagram;
    std::vector<std::pair<juce::Rectangle<int>, juce::String>> levelHeads;
    int exerciseRowHeight = 50;
    RichText text;
    juce::Viewport textViewport;
    ExerciseView exercise;

    void refreshButtons();
    void setupDiagram();
    juce::String scoreText (const juce::String& key) const;
};
