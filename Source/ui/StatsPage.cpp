#include "StatsPage.h"
#include "core/Fingerings.h"

namespace
{
    const juce::String dot = juce::String::fromUTF8 ("  \xc2\xb7  ");

    juce::String minutesText (double seconds)
    {
        const int m = juce::roundToInt (seconds / 60.0);
        return m >= 60 ? juce::String (m / 60) + " h " + juce::String (m % 60) + " min" : juce::String (m) + " min";
    }

    // "tech/2/2" -> "Spider 1-3-2-4, eighth notes"; "scale/major/2/1" -> "Major, up and down, quarter notes"
    juce::String tempoName (const juce::String& key)
    {
        static const char* npb[5] = { "", "quarter notes", "eighth notes", "triplets", "sixteenths" };
        const auto parts = juce::StringArray::fromTokens (key, "/", "");
        if (parts[0] == "tech" && parts.size() >= 3)
            return techDrillNames()[juce::jlimit (0, 2, parts[1].getIntValue() - 1)] + ", " + npb[juce::jlimit (1, 4, parts[2].getIntValue())];
        if (parts[0] == "scale" && parts.size() >= 4)
            return theory::scaleType (parts[1]).name + ", " + runPatternNames()[juce::jlimit (0, 4, parts[2].getIntValue() - 1)].toLowerCase() + ", "
                 + npb[juce::jlimit (1, 4, parts[3].getIntValue())];
        return key;
    }
}

// ---------------------------------------------------------------- chart

juce::Rectangle<float> PracticeChart::plotArea() const
{
    return getLocalBounds().toFloat().withTrimmedLeft (44).withTrimmedRight (8).withTrimmedTop (24).withTrimmedBottom (26);
}

void PracticeChart::paint (juce::Graphics& g)
{
    const auto plot = plotArea();
    double top = 30.0;   // at least half an hour on the scale
    for (auto& d : days) top = juce::jmax (top, d.minutes);
    const double step = top <= 30 ? 10 : top <= 60 ? 15 : top <= 120 ? 30 : 60;
    top = std::ceil (top / step) * step;
    auto yFor = [&] (double m) { return plot.getBottom() - (float) (m / top) * plot.getHeight(); };

    // recessive grid and axis labels
    g.setFont (Fonts::ui (12.0f));
    for (double m = 0; m <= top + 0.1; m += step)
    {
        const float y = yFor (m);
        g.setColour (Palette::line.withAlpha (m == 0 ? 0.9f : 0.45f));
        g.fillRect (plot.getX(), y - 0.5f, plot.getWidth(), 1.0f);
        g.setColour (Palette::dim);
        g.drawText (juce::String ((int) m), juce::Rectangle<float> (0, y - 8, 38, 16), juce::Justification::centredRight);
    }
    g.setColour (Palette::dim);
    g.drawText ("min", juce::Rectangle<float> (0, plot.getY() - 22, 38, 16), juce::Justification::centredRight);

    const int n = (int) days.size();
    if (n == 0)
        return;
    const float slot = plot.getWidth() / (float) n;
    const float barW = juce::jmin (28.0f, slot - 6.0f);
    for (int i = 0; i < n; ++i)
    {
        const auto& d = days[(size_t) i];
        const float cx = plot.getX() + slot * ((float) i + 0.5f);
        if (d.minutes > 0)
        {
            // thin bar, rounded only at the data end, anchored to the baseline
            const float y = yFor (d.minutes);
            juce::Path bar;
            bar.addRoundedRectangle (cx - barW * 0.5f, y, barW, plot.getBottom() - y, 4.0f, 4.0f, true, true, false, false);
            g.setColour (i == hover ? Palette::gold.brighter (0.25f) : Palette::gold);
            g.fillPath (bar);
        }
        g.setColour (d.today ? Palette::text : Palette::dim);
        g.setFont (Fonts::ui (12.0f, d.today));
        g.drawText (d.label, juce::Rectangle<float> (cx - slot * 0.5f, plot.getBottom() + 4, slot, 18), juce::Justification::centred);
    }

    if (hover >= 0 && hover < n)
    {
        const auto& d = days[(size_t) hover];
        const float cx = plot.getX() + slot * ((float) hover + 0.5f);
        g.setFont (Fonts::ui (14.0f));
        const float w = juce::jmax (150.0f, (float) d.tip.length() * 7.0f);
        auto box = juce::Rectangle<float> (juce::jlimit (plot.getX(), plot.getRight() - w, cx - w * 0.5f), plot.getY() - 22, w, 22);
        g.setColour (Palette::bg.withAlpha (0.95f));
        g.fillRoundedRectangle (box, 5.0f);
        g.setColour (Palette::line);
        g.drawRoundedRectangle (box, 5.0f, 1.0f);
        g.setColour (Palette::text);
        g.drawText (d.tip, box.reduced (8, 0), juce::Justification::centred);
    }
}

void PracticeChart::mouseMove (const juce::MouseEvent& e)
{
    const auto plot = plotArea();
    int h = -1;
    if (! days.empty() && e.position.x >= plot.getX() && e.position.x < plot.getRight())
        h = juce::jlimit (0, (int) days.size() - 1, (int) ((e.position.x - plot.getX()) / (plot.getWidth() / (float) days.size())));
    if (h != hover) { hover = h; repaint(); }
}

// ---------------------------------------------------------------- page

StatsPage::StatsPage (Progress& p, const std::vector<Lesson>& t, const std::vector<Lesson>& b)
    : progress (p), theory (t), blues (b), licks (loadLicks())
{
    addAndMakeVisible (chart);
    refresh();
}

void StatsPage::refresh()
{
    const auto today = progress.today();

    // last 14 days
    std::vector<PracticeChart::DayBar> bars;
    static const char* weekdays[7] = { "Su", "Mo", "Tu", "We", "Th", "Fr", "Sa" };
    double week = 0.0;
    int weekCorrect = 0, weekQuestions = 0;
    for (int k = 13; k >= 0; --k)
    {
        const auto date = Progress::addDays (today, -k);
        const auto d = progress.day (date);
        const auto t = juce::Time (date.substring (0, 4).getIntValue(), date.substring (5, 7).getIntValue() - 1, date.substring (8, 10).getIntValue(), 12, 0);
        PracticeChart::DayBar bar;
        bar.label = juce::String (weekdays[t.getDayOfWeek()]) + " " + juce::String (t.getDayOfMonth());
        bar.minutes = d.seconds / 60.0;
        bar.today = k == 0;
        bar.tip = juce::String (juce::roundToInt (bar.minutes)) + " min" + dot + juce::String (d.exercises) + (d.exercises == 1 ? " exercise" : " exercises")
                + dot + juce::String (d.scaleRuns + d.techRuns) + " runs";
        bars.push_back (bar);
        if (k < 7)
        {
            week += d.seconds;
            weekCorrect += d.correct;
            weekQuestions += d.questions;
        }
    }
    chart.setDays (bars);

    // totals
    auto lessonDone = [&] (const Lesson& l)
    {
        for (int x = 0; x < (int) l.exercises.size(); ++x)
            if (! progress.passed (l.exerciseKey (x))) return false;
        return true;
    };
    int lessonsDone = 0, bluesDone = 0, learned = 0;
    for (auto& l : theory) lessonsDone += lessonDone (l) ? 1 : 0;
    for (auto& l : blues) bluesDone += lessonDone (l) ? 1 : 0;
    for (auto& lk : licks) learned += progress.bestScore ("lick/" + lk.id) >= Progress::passMark ? 1 : 0;
    const int streak = progress.streakDays();
    tiles = {
        { juce::String (streak), streak == 1 ? "day streak" : "days streak" },
        { minutesText (week), "practised this week" },
        { minutesText (progress.totalPracticeSeconds()), "practised in all" },
        { weekQuestions > 0 ? juce::String (juce::roundToInt (100.0 * weekCorrect / weekQuestions)) + "%" : juce::String ("-"), "right answers this week" },
        { juce::String (lessonsDone) + "/" + juce::String ((int) theory.size()), "theory lessons finished" },
        { juce::String (bluesDone) + "/" + juce::String ((int) blues.size()) + dot + juce::String (learned) + "/" + juce::String ((int) licks.size()),
          "blues lessons" + dot + "licks" } };

    // fastest clean tempos
    tempoLines.clear();
    for (auto& t : progress.allTempos())
        tempoLines.add (tempoName (t.name.toString()) + ": " + juce::String (juce::roundToInt ((double) t.value)) + " BPM");
    tempoLines.sort (true);

    // needs work: tried, not passed, lowest first
    std::vector<std::pair<double, juce::String>> weak;
    for (auto* list : { &theory, &blues })
        for (auto& l : *list)
            for (int x = 0; x < (int) l.exercises.size(); ++x)
            {
                const auto key = l.exerciseKey (x);
                const double best = progress.bestScore (key);
                if (best >= 0 && best < Progress::passMark)
                    weak.push_back ({ best, l.exercises[(size_t) x].title + dot + l.title + dot + "best " + juce::String (juce::roundToInt (best * 100)) + "%" });
            }
    std::sort (weak.begin(), weak.end(), [] (auto& a, auto& b) { return a.first < b.first; });
    weakLines.clear();
    for (size_t i = 0; i < weak.size() && i < 6; ++i)
        weakLines.add (weak[i].second);

    // reviews in the next week
    reviewLines.clear();
    for (int k = 0; k < 7; ++k)
    {
        const auto date = Progress::addDays (today, k);
        int count = 0;
        for (auto* list : { &theory, &blues })
            for (auto& l : *list)
                for (int x = 0; x < (int) l.exercises.size(); ++x)
                {
                    const auto due = progress.dueDate (l.exerciseKey (x));
                    if (due.isNotEmpty() && (k == 0 ? due <= date : due == date))
                        ++count;
                }
        if (count > 0)
        {
            const auto when = juce::Time (date.substring (0, 4).getIntValue(), date.substring (5, 7).getIntValue() - 1, date.substring (8, 10).getIntValue(), 12, 0);
            reviewLines.add ((k == 0 ? juce::String ("Today") : k == 1 ? juce::String ("Tomorrow") : when.formatted ("%A %d"))
                             + ": " + juce::String (count) + (count == 1 ? " review" : " reviews"));
        }
    }
    repaint();
}

void StatsPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (24, 16);
    drawHeading (g, "Your practice", r.removeFromTop (40), 28.0f);

    // stat tiles
    auto tileRow = r.removeFromTop (92);
    const int n = (int) tiles.size();
    const int w = (tileRow.getWidth() - (n - 1) * 12) / juce::jmax (1, n);
    for (int i = 0; i < n; ++i)
    {
        auto t = tileRow.removeFromLeft (w);
        tileRow.removeFromLeft (12);
        drawPanel (g, t.toFloat());
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (28.0f, true));
        g.drawFittedText (tiles[(size_t) i].value, t.reduced (12, 8).removeFromTop (44), juce::Justification::centredLeft, 1);
        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (14.0f));
        g.drawFittedText (tiles[(size_t) i].label, t.reduced (12, 8).withTrimmedTop (44), juce::Justification::topLeft, 2);
    }
    r.removeFromTop (14);

    auto bottom = r;
    auto left = bottom.removeFromLeft (bottom.getWidth() * 3 / 5);
    bottom.removeFromLeft (16);
    drawPanel (g, left.toFloat());
    drawHeading (g, "Practice minutes, last 14 days", left.reduced (16, 10).removeFromTop (28), 19.0f);

    auto right = bottom;
    auto list = [&] (juce::Rectangle<int> area, const juce::String& title, const juce::StringArray& lines, const juce::String& empty)
    {
        drawPanel (g, area.toFloat());
        auto in = area.reduced (16, 10);
        drawHeading (g, title, in.removeFromTop (28), 18.0f);
        g.setFont (Fonts::ui (14.5f));
        if (lines.isEmpty())
        {
            g.setColour (Palette::dim);
            g.drawFittedText (empty, in.removeFromTop (40), juce::Justification::topLeft, 2, 1.0f);
            return;
        }
        g.setColour (Palette::text);
        for (auto& l : lines)
        {
            if (in.getHeight() < 20) break;
            g.drawFittedText (l, in.removeFromTop (22), juce::Justification::centredLeft, 1, 0.8f);
        }
    };
    const int third = right.getHeight() / 3;
    list (right.removeFromTop (third - 8), "Fastest clean tempos", tempoLines, "Clean runs in Practice > Scales or Technique show up here.");
    right.removeFromTop (8);
    list (right.removeFromTop (third - 8), "Needs work", weakLines, "Nothing below 8 out of 10. Keep going.");
    right.removeFromTop (8);
    list (right, "Reviews coming up", reviewLines, "No reviews in the next week. Passed exercises come back after 1, 3, 7, 14, 30 and 60 days.");
}

void StatsPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    r.removeFromTop (40 + 92 + 14);
    auto left = r.removeFromLeft (r.getWidth() * 3 / 5);
    chart.setBounds (left.reduced (16, 10).withTrimmedTop (34));
}
