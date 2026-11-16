#pragma once
#include "Lessons.h"
#include "Licks.h"
#include "Progress.h"

// Today's practice session: a warm-up, the next new exercise, scales, reviews that are due (spaced repetition),
// ear training, and the blues track. Made once a day (per session length) and kept, so the list does not change
// as items get done.

struct PlanItem
{
    enum class Kind { warmup, lesson, blues, review, scales, ear, improv };
    Kind kind = Kind::lesson;
    juce::String title, detail;
    int minutes = 5;
    juce::String lessonId;       // lesson, blues, review, ear
    int exercise = -1;
    int drill = 0;               // warm-up: technique drill (0 chromatic, 1 spider, 2 string skipping)
    juce::String scaleId;        // scales
    int rootPc = 7;
    juce::String lickId;         // improv

    juce::var toVar() const;
    static PlanItem fromVar (const juce::var&);
};

struct PlanContext
{
    const std::vector<Lesson>& theory;
    const std::vector<Lesson>& blues;
    const std::vector<Lick>& licks;
};

std::vector<PlanItem> makePlan (const PlanContext&, const Progress&, int minutes);

// the stored plan for today and this length, or a new one (stored)
std::vector<PlanItem> todaysPlan (const PlanContext&, Progress&, int minutes);

bool isDone (const PlanItem&, const Progress&);
const Lesson* findLesson (const PlanContext&, const juce::String& id, bool* isBlues = nullptr, int* index = nullptr);
