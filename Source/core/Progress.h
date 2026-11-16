#pragma once
#include <juce_core/juce_core.h>

// What the player has done: best score and attempts per exercise, practice days, drill records, a daily history,
// and spaced-repetition review dates. Saved as JSON in %APPDATA%\GuitarCoach\progress.json after every change.
class Progress
{
public:
    static constexpr double passMark = 0.8;

    explicit Progress (juce::File file);
    static juce::File defaultFile();

    // dates are "YYYY-MM-DD"; tests can pretend it is another day
    juce::String today() const;
    void setToday (const juce::String& date) { todayOverride = date; }
    static juce::String addDays (const juce::String& date, int days);
    static int daysBetween (const juce::String& from, const juce::String& to);

    void recordExercise (const juce::String& key, int correct, int total);
    double bestScore (const juce::String& key) const;   // 0..1, -1 when never tried
    double lastScore (const juce::String& key) const;   // -1 when never tried
    int attempts (const juce::String& key) const;
    bool passed (const juce::String& key) const { return bestScore (key) >= passMark; }
    juce::String lastDate (const juce::String& key) const;

    // Spaced repetition (Leitner boxes): a passed round moves an exercise up a box and its next review further away
    // (1, 3, 7, 14, 30, 60 days); a failed round sends it back to box 0, due tomorrow.
    int box (const juce::String& key) const;
    juce::String dueDate (const juce::String& key) const;          // empty when never passed
    bool isDue (const juce::String& key) const;                     // passed before and due today or earlier
    juce::StringArray dueKeys() const;                              // most overdue first
    static int intervalForBox (int box);

    void recordDrill (const juce::String& key, double value);   // e.g. seconds per note: lower is better
    double bestDrill (const juce::String& key) const;          // -1 when never done

    void recordTempo (const juce::String& key, double bpm);      // fastest clean tempo: higher is better
    double bestTempo (const juce::String& key) const;           // 0 when never done
    juce::NamedValueSet allTempos() const;

    void recordRun (const juce::String& kind);                   // a scale or technique run finished ("scale", "tech")

    // practice time, added up while something is being practised
    void addPracticeSeconds (double seconds);

    struct Day
    {
        juce::String date;
        int exercises = 0, correct = 0, questions = 0, scaleRuns = 0, techRuns = 0;
        double seconds = 0.0;
    };
    Day day (const juce::String& date) const;

    // the plan made for one day, kept so it stays the same all day
    juce::var getPlan (const juce::String& date) const;
    void setPlan (const juce::String& date, const juce::var& plan);

    juce::var setting (const juce::String& name, const juce::var& fallback) const;
    void setSetting (const juce::String& name, const juce::var& value);

    void markPracticeToday();
    int streakDays() const;
    int totalExercisesDone() const;
    double totalPracticeSeconds() const;

private:
    juce::File file;
    juce::var data;
    juce::String todayOverride;
    double unsavedSeconds = 0.0;
    juce::DynamicObject* section (const char* name);
    const juce::DynamicObject* section (const char* name) const;
    juce::DynamicObject* dayEntry (const juce::String& date);
    void save() const;
};
