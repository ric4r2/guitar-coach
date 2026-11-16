#include "TheoryPage.h"
#include "core/Fingerings.h"

using namespace theory;

TheoryPage::TheoryPage (AudioEngine& e, Progress& p, const std::vector<Lesson>& l)
    : engine (e), progress (p), lessons (l), exercise (e, p)
{
    for (int i = 0; i < (int) lessons.size(); ++i)
    {
        auto* b = lessonButtons.add (new juce::TextButton());
        b->setClickingTogglesState (false);
        b->onClick = [this, i] { openLesson (i); };
        listContent.addAndMakeVisible (b);
    }
    listViewport.setViewedComponent (&listContent, false);
    listViewport.setScrollBarsShown (true, false);
    listViewport.setScrollBarThickness (8);
    addAndMakeVisible (listViewport);
    addAndMakeVisible (diagramStaff);
    addAndMakeVisible (diagramFret);
    addChildComponent (circle);
    addChildComponent (chartDiagram);
    circle.onSelect = [this] (int fifths, bool minor)
    {
        // play the tonic triad of the clicked key
        auto tonic = keyTonic (fifths, minor);
        tonic.octave = 3;
        engine.stopNotes();
        for (auto& n : chordNotes (tonic, chordType (minor ? "minor" : "major")))
            engine.playNote (n.midi(), 0.0, 2.0, 0.6f);
    };
    textViewport.setViewedComponent (&text, false);
    textViewport.setScrollBarsShown (true, false);
    addAndMakeVisible (textViewport);
    addChildComponent (exercise);

    diagramFret.setClickable (true);
    diagramFret.onClick = [this] (int s, int f) { engine.playNote (fretMidi (s, f), 0.0, 1.5); };

    exercise.onBack = [this] { openLesson (current); };
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
    circle.setVisible (false);
    chartDiagram.setVisible (false);
    diagramStaff.setKeySignature (0);

    auto degreeMarks = [] (int rootPc, const ScaleType& st, const std::vector<FretNote>& notes)
    {
        std::vector<FretMark> marks;
        for (auto& n : notes)
            marks.push_back ({ n.string, n.fret, n.degree == 0 ? juce::String ("R") : degreeLabel (st.intervals[(size_t) n.degree]), n.degree == 0 ? 1 : 0 });
        juce::ignoreUnused (rootPc);
        return marks;
    };

    if (auto* rhythm = d["rhythm"].getArray())
    {
        std::vector<StaffView::RhythmNote> notes;
        for (auto& e : *rhythm)
            if (auto* pair = e.getArray(); pair != nullptr && pair->size() >= 2)
                notes.push_back ({ Note { 6, 0, 3 }, (double) (*pair)[0], (double) (*pair)[1], {} });
        diagramStaff.setRhythm (notes, (double) d.getProperty ("beats", 4));
        diagramStaff.setVisible (true);
    }
    else if (d.hasProperty ("bluesChart"))
    {
        int index = 0;
        for (int i = 0; i < (int) bluesForms().size(); ++i)
            if (bluesForms()[(size_t) i].id == d["bluesChart"].toString())
                index = i;
        chartDiagram.setForm (index);
        chartDiagram.setKey (parseNote (d.getProperty ("key", "A").toString()).value_or (Note { 5, 0, 3 }).pc());
        chartDiagram.setVisible (true);
    }
    else if (d.hasProperty ("box") || d.hasProperty ("boxes"))
    {
        const auto& spec = d.hasProperty ("box") ? d["box"] : d["boxes"];
        const int rootPc = parseNote (spec["root"].toString()).value_or (Note { 5, 0, 3 }).pc();
        const auto& st = scaleType (spec["scale"].toString());
        const auto boxes = positionFingerings (rootPc, st);
        std::vector<FretMark> marks;
        if (d.hasProperty ("box"))
            marks = degreeMarks (rootPc, st, boxes[(size_t) juce::jlimit (0, (int) boxes.size() - 1, (int) spec.getProperty ("index", 0))].notes);
        else
            for (int s = 1; s <= 6; ++s)
                for (int f = 0; f <= 15; ++f)
                    for (int k = 0; k < st.size(); ++k)
                        if (mod12 (fretMidi (s, f) - rootPc) == st.intervals[(size_t) k].semitones)
                            marks.push_back ({ s, f, k == 0 ? juce::String ("R") : degreeLabel (st.intervals[(size_t) k]), k == 0 ? 1 : 0 });
        diagramFret.setMaxFret (15);
        diagramFret.setMarks (marks);
        diagramFret.setVisible (true);
    }
    else if (d.hasProperty ("scale"))
    {
        const auto root = parseNote (d["scale"]["root"].toString()).value_or (Note { 0, 0, 3 });
        const auto notes = scaleNotes (root, scaleType (d["scale"]["type"].toString()));
        std::vector<juce::Colour> cols (notes.size(), Palette::ink);
        cols.front() = cols.back() = juce::Colour (0xff9a4a24);   // the root, in brown
        diagramStaff.setNotes (notes, cols, true);
        diagramStaff.setStepLabels (juce::StringArray::fromTokens (stepPattern (notes), " ", ""));
        diagramStaff.setVisible (true);
    }
    else if (d.hasProperty ("circle"))
    {
        circle.setSelected (0);
        circle.setVisible (true);
    }
    else if (auto* chords = d["chords"].getArray())
    {
        std::vector<std::vector<Note>> cs;
        for (auto& c : *chords)
        {
            std::vector<Note> notes;
            if (auto* a = c.getArray())
                for (auto& v : *a)
                    if (auto n = parseNote (v.toString()))
                        notes.push_back (*n);
            cs.push_back (notes);
        }
        juce::StringArray labels;
        if (auto* l = d["labels"].getArray())
            for (auto& v : *l)
                labels.add (v.toString());
        diagramStaff.setChords (cs, labels);
        diagramStaff.setVisible (true);
    }
    else if (d.hasProperty ("triadShapes"))
    {
        const auto& ts = d["triadShapes"];
        const auto root = parseNote (ts["root"].toString()).value_or (Note {});
        diagramFret.setMaxFret (15);
        diagramFret.setMarks (triadShapeMarks (root, chordType (ts["type"].toString()), (int) ts.getProperty ("top", 1), 15));
        diagramFret.setVisible (true);
    }
    else if (auto* arr = d["staff"].getArray())
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
    scrollToCurrent();
    setupDiagram();
    refreshButtons();
    resized();
    repaint();
}

void TheoryPage::scrollToCurrent()
{
    if (auto* b = lessonButtons[current])
    {
        const int top = b->getY() - 40, bottom = b->getBottom() + 10;
        const auto view = listViewport.getViewArea();
        if (top < view.getY()) listViewport.setViewPosition (0, juce::jmax (0, top));
        else if (bottom > view.getBottom()) listViewport.setViewPosition (0, bottom - view.getHeight());
    }
}

void TheoryPage::openExercise (int lessonIndex, int exerciseIndex, juce::int64 seed)
{
    if (lessonIndex != current)
        openLesson (lessonIndex);
    for (auto* b : exerciseButtons) b->setVisible (false);
    diagramStaff.setVisible (false);
    diagramFret.setVisible (false);
    circle.setVisible (false);
    chartDiagram.setVisible (false);
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
                      exHead.translated (0, 30 + exerciseRowHeight * (int) l.exercises.size() + 6).withHeight (44), juce::Justification::topLeft, 3);
}

void TheoryPage::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (330).reduced (12);
    listViewport.setBounds (left.reduced (2, 6));
    // a heading for each level, then its lessons; fills the panel when they fit, scrolls when they do not
    static const char* levelNames[] = { "", "Level 1 \xc2\xb7 Foundations", "Level 2 \xc2\xb7 Position player", "Level 3 \xc2\xb7 Harmony", "Level 4 \xc2\xb7 Pre-Gleason" };
    int levels = 0, lastLevel = -1;
    for (auto& l : lessons)
        if (l.level != lastLevel) { ++levels; lastLevel = l.level; }
    const int width = listViewport.getWidth() - 10;
    const int rowH = juce::jlimit (38, 52, (listViewport.getHeight() - levels * 36) / juce::jmax (1, (int) lessons.size()));
    listContent.heads.clear();
    lastLevel = -1;
    int y = 0;
    for (int i = 0; i < lessonButtons.size(); ++i)
    {
        const int level = lessons[(size_t) i].level;
        if (level != lastLevel)
        {
            listContent.heads.push_back ({ juce::Rectangle<int> (12, y, width - 12, 36), juce::String::fromUTF8 (levelNames[juce::jlimit (0, 4, level)]) });
            y += 36;
            lastLevel = level;
        }
        lessonButtons[i]->setBounds (juce::Rectangle<int> (6, y, width - 6, rowH).reduced (0, 3));
        y += rowH;
    }
    listContent.setSize (width, y + 6);

    auto right = r.reduced (12);
    if (exercise.isVisible())
    {
        exercise.setBounds (right);
        return;
    }

    right.removeFromTop (60);
    auto diagram = right.removeFromTop (200);
    diagramStaff.setBounds (diagram.withSizeKeepingCentre (juce::jmin (diagram.getWidth(), 720), diagram.getHeight()));
    diagramFret.setBounds (diagram);
    circle.setBounds (diagram.withSizeKeepingCentre (diagram.getHeight(), diagram.getHeight()));
    chartDiagram.setBounds (diagram.withSizeKeepingCentre (juce::jmin (diagram.getWidth(), 640), diagram.getHeight()));
    right.removeFromTop (12);

    auto exCol = right.removeFromRight (juce::jmax (330, right.getWidth() * 2 / 5));
    right.removeFromRight (16);
    textViewport.setVisible (true);
    textViewport.setBounds (right);
    text.setSize (right.getWidth() - 14, text.heightForWidth (right.getWidth() - 14));

    exCol.removeFromTop (30);
    exerciseRowHeight = juce::jlimit (36, 50, (exCol.getHeight() - 50) / juce::jmax (1, exerciseButtons.size()));
    for (auto* b : exerciseButtons)
        b->setBounds (exCol.removeFromTop (exerciseRowHeight).reduced (0, 3));
}
