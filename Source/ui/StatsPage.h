#pragma once
#include "Look.h"
#include "core/Lessons.h"
#include "core/Licks.h"
#include "core/Progress.h"

// Practice minutes per day for the last two weeks, as bars. Hover a bar for its numbers.
class PracticeChart : public juce::Component
{
public:
    struct DayBar { juce::String label, tip; double minutes = 0.0; bool today = false; };
    void setDays (std::vector<DayBar> d) { days = std::move (d); repaint(); }
    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override { hover = -1; repaint(); }
private:
    std::vector<DayBar> days;
    int hover = -1;
    juce::Rectangle<float> plotArea() const;
};

// The Progress tab: streak and totals, the chart, fastest clean tempos, what needs work, reviews coming up.
class StatsPage : public juce::Component
{
public:
    StatsPage (Progress&, const std::vector<Lesson>& theory, const std::vector<Lesson>& blues);
    void refresh();
    void paint (juce::Graphics&) override;
    void resized() override;

private:
    Progress& progress;
    const std::vector<Lesson>& theory;
    const std::vector<Lesson>& blues;
    std::vector<Lick> licks;
    PracticeChart chart;

    struct Tile { juce::String value, label; };
    std::vector<Tile> tiles;
    juce::StringArray tempoLines, weakLines, reviewLines;
};
