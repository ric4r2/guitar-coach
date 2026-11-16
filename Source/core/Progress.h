#pragma once
#include <juce_core/juce_core.h>

// What the player has done: best score and attempts per exercise, practice days, drill records.
// Saved as JSON in %APPDATA%\GuitarCoach\progress.json after every change.
class Progress
{
public:
    static constexpr double passMark = 0.8;

    explicit Progress (juce::File file);
    static juce::File defaultFile();

    void recordExercise (const juce::String& key, int correct, int total);
    double bestScore (const juce::String& key) const;   // 0..1, -1 when never tried
    int attempts (const juce::String& key) const;
    bool passed (const juce::String& key) const { return bestScore (key) >= passMark; }

    void recordDrill (const juce::String& key, double value);   // e.g. seconds per note: lower is better
    double bestDrill (const juce::String& key) const;          // -1 when never done

    void markPracticeToday();
    int streakDays() const;
    int totalExercisesDone() const;

private:
    juce::File file;
    juce::var data;
    juce::DynamicObject* section (const char* name);
    const juce::DynamicObject* section (const char* name) const;
    void save() const;
};
