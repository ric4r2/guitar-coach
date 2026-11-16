#include "TheoryPage.h"

using namespace theory;

TheoryPage::TheoryPage (AudioEngine& e, Progress& p, const std::vector<Lesson>& l)
    : engine (e), progress (p), lessons (l), exercise (e, p)
{
    for (int i = 0; i < (int) lessons.size(); ++i)
    {
        auto* b = lessonButtons.add (new juce::TextButton());
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { openLesson (i); };
        addAndMakeVisible (b);
    }
    addAndMakeVisible (diagramStaff);
    addAndMakeVisible (diagramFret);
    textViewport.setViewedComponent (&text, false);
    textViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (textViewport);
    addChildComponent (exercise);

    diagramFret.setClickable (true);
    diagramFret.onClick = [this] (int s, int f) { engine.playNote (fretMidi (s, f), 0.0, 1.5); };

    exercise.onBack = [this] { exercise.setVisible (false); refreshButtons(); resized(); repaint(); };
    exercise.onFinished = [this] { refreshButtons(); if (onProgressChanged) onProgressChanged(); };

    openLesson (0);
}

int TheoryPage::firstUnfinishedLesson() const
{
    for (int i = 0; i < (int) lessons.size(); ++i)
        for (int x = 0; x < (int) lessons[(size_t) i].exercises.size(); ++x)
            if (! progress.passed (lessons[(size_t) i].exerciseKey (x)))
                return i;
    return 0;
}

juce::String TheoryPage::scoreText (const juce::String& key) const
{
    const double best = progress.bestScore (key);
    if (best < 0) return "new";
    return juce::String (juce::roundToInt (best * 100)) + "%" + (best >= Progress::passMark ? juce::String (juce::CharPointer_UTF8 (" \xe2\x9c\x93")) : juce::String());
}

void TheoryPage::refreshButtons()
{
    for (int i = 0; i < lessonButtons.size(); ++i)
    {
        const auto& l = lessons[(size_t) i];
        int passed = 0;
        for (int x = 0; x < (int) l.exercises.size(); ++x)
            passed += progress.passed (l.exerciseKey (x)) ? 1 : 0;
        lessonButtons[i]->setButtonText (juce::String (i + 1) + ". " + l.title + "   (" + juce::String (passed) + "/" + juce::String ((int) l.exercises.size()) + ")");
        lessonButtons[i]->setToggleState (i == current, juce::dontSendNotification);
    }

    exerciseButtons.clear();
    const auto& l = lessons[(size_t) current];
    for (int x = 0; x < (int) l.exercises.size(); ++x)
    {
        const auto& ex = l.exercises[(size_t) x];
        auto* b = exerciseButtons.add (new juce::TextButton (ex.title + juce::String::fromUTF8 ("  ·  ") + describeExerciseInput (ex) + juce::String::fromUTF8 ("  ·  ") + scoreText (l.exerciseKey (x))));
        b->onClick = [this, x] { openExercise (current, x); };
        addAndMakeVisible (b);
        b->setVisible (! exercise.isVisible());
    }
}

void TheoryPage::setupDiagram()
{
    const auto& d = lessons[(size_t) current].diagram;
    diagramStaff.setVisible (false);
    diagramFret.setVisible (false);

    if (auto* arr = d["staff"].getArray())
    {
        std::vector<Note> notes;
        for (auto& v : *arr)
            if (auto n = parseNote (v.toString()))
                notes.push_back (*n);
        diagramStaff.setNotes (notes, {}, (bool) d.getProperty ("labels", false));
        diagramStaff.setVisible (true);
    }
    else if (d.hasProperty ("fretboard"))
    {
        const auto kind = d["fretboard"].toString();
        const int maxFret = (int) d.getProperty ("maxFret", 12);
        std::vector<int> strings;
        if (auto* s = d["strings"].getArray())
            for (auto& v : *s) strings.push_back ((int) v);
        std::vector<FretMark> marks;
        for (int s : strings)
            for (int f = 0; f <= maxFret; ++f)
            {
                const auto n = noteFromMidi (fretMidi (s, f));
                if (kind == "naturals" && n.acc != 0)
                    continue;
                marks.push_back ({ s, f, kind == "chromatic" && n.acc != 0 ? pcNameBoth (n.pc()) : n.name(), n.acc == 0 ? (n.letter == 0 ? 1 : 0) : 4 });
            }
        diagramFret.setMaxFret (maxFret);
        diagramFret.setMarks (marks);
        diagramFret.setVisible (true);
    }
    else if (d.hasProperty ("intervals"))
    {
        // interval shapes around a root on the 5th string, 3rd fret (C)
        const int rootString = 5, rootFret = 3;
        const int root = fretMidi (rootString, rootFret);
        std::vector<FretMark> marks;
        for (int s = 1; s <= 5; ++s)
            for (int f = 0; f <= 8; ++f)
            {
                const int semis = fretMidi (s, f) - root;
                if (semis < 0 || semis > 12)
                    continue;
                if (s == rootString && f == rootFret)
                    marks.push_back ({ s, f, "R", 1 });
                else if (std::abs (f - rootFret) <= 3)
                    marks.push_back ({ s, f, octaveIntervals()[(size_t) semis].shortName(), semis == 12 ? 1 : 0 });
            }
        diagramFret.setMaxFret (8);
        diagramFret.setMarks (marks);
        diagramFret.setVisible (true);
    }
}

void TheoryPage::openLesson (int index)
{
    current = juce::jlimit (0, (int) lessons.size() - 1, index);
    exercise.setVisible (false);
    text.setLines (lessons[(size_t) current].body);
    setupDiagram();
    refreshButtons();
    resized();
    repaint();
}

void TheoryPage::openExercise (int lessonIndex, int exerciseIndex, juce::int64 seed)
{
    if (lessonIndex != current)
        openLesson (lessonIndex);
    for (auto* b : exerciseButtons) b->setVisible (false);
    diagramStaff.setVisible (false);
    diagramFret.setVisible (false);
    textViewport.setVisible (false);
    exercise.setVisible (true);
    resized();
    exercise.start (lessons[(size_t) current], exerciseIndex, seed);
}

void TheoryPage::notePlayed (const NoteEvent& e)
{
    if (exercise.isVisible())
        exercise.notePlayed (e);
}

void TheoryPage::pitchChanged (const PitchFrame& f)
{
    if (exercise.isVisible())
        exercise.pitchChanged (f);
    else if (diagramFret.isVisible())
        diagramFret.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1);
}

void TheoryPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (330).reduced (12);
    drawPanel (g, left.toFloat());
    drawHeading (g, juce::String::fromUTF8 ("Level 1 · Foundations"), left.removeFromTop (44).reduced (14, 0), 19.0f);

    if (exercise.isVisible())
        return;

    auto right = r.reduced (12);
    const auto& l = lessons[(size_t) current];
    auto head = right.removeFromTop (56);
    drawHeading (g, l.title, head.removeFromTop (34), 26.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawText (l.strand + juce::String::fromUTF8 ("  ·  Level ") + juce::String (l.level) + juce::String::fromUTF8 ("  ·  ") + l.summary, head, juce::Justification::centredLeft);

    auto exHead = juce::Rectangle<int> (textViewport.getRight() + 16, textViewport.getY(), right.getRight() - textViewport.getRight() - 16, 30);
    drawHeading (g, "Exercises", exHead, 19.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawFittedText ("8 out of 10 passes an exercise. Pass them all to finish the lesson. Click the neck to hear any note.",
                      exHead.translated (0, 30 + 50 * (int) l.exercises.size() + 6).withHeight (44), juce::Justification::topLeft, 3);
}

void TheoryPage::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (330).reduced (12);
    left.removeFromTop (48);
    for (auto* b : lessonButtons)
        b->setBounds (left.removeFromTop (52).reduced (8, 4));

    auto right = r.reduced (12);
    if (exercise.isVisible())
    {
        exercise.setBounds (right);
        return;
    }

    right.removeFromTop (60);
    auto diagram = right.removeFromTop (200);
    diagramStaff.setBounds (diagram.withSizeKeepingCentre (juce::jmin (diagram.getWidth(), 640), diagram.getHeight()));
    diagramFret.setBounds (diagram);
    right.removeFromTop (12);

    auto exCol = right.removeFromRight (juce::jmax (330, right.getWidth() * 2 / 5));
    right.removeFromRight (16);
    textViewport.setVisible (true);
    textViewport.setBounds (right);
    text.setSize (right.getWidth() - 14, text.heightForWidth (right.getWidth() - 14));

    exCol.removeFromTop (30);
    for (auto* b : exerciseButtons)
        b->setBounds (exCol.removeFromTop (50).reduced (0, 4));
}
