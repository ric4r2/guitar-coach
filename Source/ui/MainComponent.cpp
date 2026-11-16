#include "MainComponent.h"

using namespace theory;

// ---------------------------------------------------------------- home

HomePage::HomePage (Progress& p, const std::vector<Lesson>& l, const std::vector<Lesson>& b) : progress (p), lessons (l), bluesLessons (b), licks (loadLicks())
{
    addAndMakeVisible (continueButton);
    addAndMakeVisible (practiceButton);
    continueButton.onClick = [this] { if (onContinue) onContinue (nextLesson); };
    practiceButton.onClick = [this] { if (onGoTo) onGoTo (3); };
    refresh();
}

void HomePage::refresh()
{
    nextLesson = 0;
    bool foundOpen = false;
    for (int i = 0; i < (int) lessons.size() && ! foundOpen; ++i)
        for (int x = 0; x < (int) lessons[(size_t) i].exercises.size(); ++x)
            if (! progress.passed (lessons[(size_t) i].exerciseKey (x))) { nextLesson = i; foundOpen = true; break; }
    continueButton.setButtonText ("Continue: " + lessons[(size_t) nextLesson].title);
    repaint();
}

void HomePage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (24, 16);
    g.setColour (Palette::text);
    g.setFont (Fonts::ui (34.0f, true));
    g.drawText ("Welcome back, Joe", r.removeFromTop (48), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (17.0f));
    const int streak = progress.streakDays();
    g.drawText ("Streak: " + juce::String (streak) + (streak == 1 ? " day" : " days") + juce::String::fromUTF8 ("   ·   exercises done: ") + juce::String (progress.totalExercisesDone())
                + juce::String::fromUTF8 ("   ·   goal: ready for Gleason's Guitar Improv in 12 to 18 months"),
                r.removeFromTop (28), juce::Justification::centredLeft);
    r.removeFromTop (70);   // continue button row

    auto cols = r;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 12);
    cols.removeFromLeft (24);
    auto right = cols;

    // today's session
    drawPanel (g, left.toFloat());
    auto inner = left.reduced (20, 14);
    drawHeading (g, "Today's session (45 min)", inner.removeFromTop (34), 21.0f);
    struct Item { const char* mins; const char* what; };
    const Item items[] = {
        { "5 min",  "Warm-up: Practice > Technique, chromatic or spider drill with the tempo ladder. It adds 5 BPM after each clean run." },
        { "10 min", "Theory: read the next lesson and pass one exercise." },
        { "10 min", "Scales: Practice > Scales, the scale of the current lesson in one shape, up and down, with the metronome." },
        { "15 min", "Ears and fretboard: one ear-training exercise, then the note finder or interval shapes." },
        { "5 min",  "Review: redo one exercise you passed earlier this week." } };
    for (auto& it : items)
    {
        auto row = inner.removeFromTop (64);
        g.setColour (Palette::gold);
        g.setFont (Fonts::ui (17.0f, true));
        g.drawText (it.mins, row.removeFromLeft (70), juce::Justification::topLeft);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (16.0f));
        g.drawFittedText (it.what, row, juce::Justification::topLeft, 3, 1.0f);
    }
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawFittedText ("Practise in short cycles: repeat until you tire, rest about half as long, repeat. Little and often beats long and rare.",
                      inner.removeFromTop (44), juce::Justification::topLeft, 2);

    // progress, level by level
    drawPanel (g, right.toFloat());
    inner = right.reduced (20, 14);
    drawHeading (g, "Your progress", inner.removeFromTop (34), 21.0f);
    static const char* levelNames[] = { "", "Level 1: Foundations", "Level 2: Position player", "Level 3: Harmony", "Level 4: Pre-Gleason" };
    int lastLevel = -1;
    const int rowH = juce::jlimit (22, 32, (inner.getHeight() - 110) / juce::jmax (1, (int) lessons.size() + 6));
    auto barRow = [&] (const juce::String& title, int done, int total)
    {
        auto row = inner.removeFromTop (rowH);
        total = juce::jmax (1, total);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (15.0f));
        g.drawText (title, row.removeFromLeft (row.getWidth() * 3 / 5), juce::Justification::centredLeft);
        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (13.0f));
        g.drawText (juce::String (done) + "/" + juce::String (total), row.removeFromRight (40), juce::Justification::centredRight);
        auto bar = row.reduced (6, 0).withSizeKeepingCentre (row.getWidth() - 12, 10).toFloat();
        g.setColour (Palette::panelLight);
        g.fillRoundedRectangle (bar, 4.0f);
        g.setColour (done >= total ? Palette::green : Palette::gold);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) done / (float) total), 4.0f);
    };
    auto heading = [&] (const juce::String& text)
    {
        g.setColour (Palette::gold.withAlpha (0.85f));
        g.setFont (Fonts::ui (15.0f, true));
        g.drawText (text, inner.removeFromTop (rowH), juce::Justification::bottomLeft);
    };
    auto passedIn = [&] (const Lesson& l)
    {
        int passed = 0;
        for (int x = 0; x < (int) l.exercises.size(); ++x)
            passed += progress.passed (l.exerciseKey (x)) ? 1 : 0;
        return passed;
    };
    for (auto& l : lessons)
    {
        if (l.level != lastLevel)
        {
            lastLevel = l.level;
            heading (levelNames[juce::jlimit (0, 4, l.level)]);
        }
        barRow (l.title, passedIn (l), (int) l.exercises.size());
    }
    heading ("Blues track");
    int lessonsDone = 0;
    for (auto& l : bluesLessons)
        lessonsDone += passedIn (l) == (int) l.exercises.size() ? 1 : 0;
    barRow ("Blues lessons finished", lessonsDone, (int) bluesLessons.size());
    int learned = 0;
    for (auto& lk : licks)
        learned += progress.bestScore ("lick/" + lk.id) >= Progress::passMark ? 1 : 0;
    barRow ("Licks learned", learned, (int) licks.size());
    inner.removeFromTop (10);
    drawHeading (g, "Coming next", inner.removeFromTop (28), 18.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawFittedText ("Milestone 4: seventh chords and the chords of every key, Roman numerals and progressions, chord-quality ear training, and rhythm reading with tapping.",
                      inner.removeFromTop (60), juce::Justification::topLeft, 3);
}

void HomePage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    r.removeFromTop (48 + 28 + 12);
    auto row = r.removeFromTop (48);
    continueButton.setBounds (row.removeFromLeft (520));
    row.removeFromLeft (12);
    practiceButton.setBounds (row.removeFromLeft (180));
}

// ---------------------------------------------------------------- audio

AudioPage::AudioPage (AudioEngine& e)
    : engine (e), selector (e.getDeviceManager(), 1, 2, 2, 2, false, false, false, false)
{
    addAndMakeVisible (selector);
    channel.addItem ("Listen to: all active inputs", 1);
    channel.addItem ("Listen to: input 1 only", 2);
    channel.addItem ("Listen to: input 2 only", 3);
    channel.setSelectedId (engine.getInputChannel() + 2, juce::dontSendNotification);
    channel.onChange = [this] { engine.setInputChannel (channel.getSelectedId() - 2); };
    addAndMakeVisible (channel);
    testButton.onClick = [this] { engine.playSequence ({ 40, 45, 50, 55, 59, 64 }, 0.35, 1.5); };
    addAndMakeVisible (testButton);
    outVolume.setSliderStyle (juce::Slider::LinearHorizontal);
    outVolume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    outVolume.setRange (0.0, 1.0, 0.01);
    outVolume.setValue (engine.outputVolume.load(), juce::dontSendNotification);
    outVolume.onValueChange = [this] { engine.outputVolume.store ((float) outVolume.getValue()); };
    addAndMakeVisible (outVolume);
    startTimerHz (30);
}

void AudioPage::timerCallback()
{
    meter = juce::jmax (engine.takeInputPeak(), meter * 0.85f);
    repaint (getLocalBounds().removeFromRight (getWidth() / 2));
}

void AudioPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (r.getWidth() / 2 - 12);
    drawPanel (g, left.toFloat());
    drawHeading (g, "Audio device", left.reduced (20, 10).removeFromTop (30), 21.0f);
    r.removeFromLeft (24);
    drawPanel (g, r.toFloat());
    auto inner = r.reduced (20, 14);
    drawHeading (g, "Input check", inner.removeFromTop (30), 21.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawFittedText (engine.describeDevice(), inner.removeFromTop (40), juce::Justification::topLeft, 2);
    inner.removeFromTop (50);   // channel box

    // input meter in dBFS
    auto meterArea = inner.removeFromTop (30).toFloat();
    g.setColour (Palette::panelLight);
    g.fillRoundedRectangle (meterArea, 5.0f);
    const float db = juce::Decibels::gainToDecibels (meter, -60.0f);
    const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
    g.setColour (db > -3.0f ? Palette::red : db > -18.0f ? Palette::green : Palette::gold);
    g.fillRoundedRectangle (meterArea.withWidth (meterArea.getWidth() * frac), 5.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawText ("Input level " + (meter > 0.0f ? juce::String (db, 1) + " dBFS" : juce::String ("(silent)")) + juce::String::fromUTF8 ("   ·   aim for peaks around -12 dB: turn the iD14 gain knob"),
                inner.removeFromTop (26), juce::Justification::centredLeft);

    inner.removeFromTop (16);
    g.setColour (Palette::text);
    g.setFont (Fonts::ui (64.0f, true));
    g.drawText (frame.valid ? noteFromMidi (juce::roundToInt (frame.midi)).name (true) : "-", inner.removeFromTop (90), juce::Justification::centred);
    g.setFont (Fonts::ui (16.0f));
    g.setColour (Palette::dim);
    g.drawText (frame.valid ? juce::String (frame.hz, 1) + " Hz, confidence " + juce::String (juce::roundToInt (frame.confidence * 100)) + "%" : "Play a note to test pitch detection",
                inner.removeFromTop (26), juce::Justification::centred);
    if (lastNoteTime > 0)
        g.drawText ("Last note detected: " + noteFromMidi (lastNote.midi).name (true) + " (" + (lastNote.cents >= 0 ? "+" : "") + juce::String (juce::roundToInt (lastNote.cents)) + " cents)",
                    inner.removeFromTop (26), juce::Justification::centred);

    auto bottom = r.reduced (20, 14).removeFromBottom (90);
    g.setColour (Palette::dim);
    g.drawText ("Output volume", bottom.removeFromTop (22), juce::Justification::centredLeft);
    g.drawFittedText ("Tip: the Audient ASIO driver gives the lowest delay. 48 kHz and 128 samples work well. The guitar goes into the front instrument input (input 1).",
                      bottom.withTrimmedTop (50), juce::Justification::topLeft, 2);
}

void AudioPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (r.getWidth() / 2 - 12).reduced (12, 10);
    left.removeFromTop (40);
    selector.setBounds (left);
    r.removeFromLeft (24);
    auto inner = r.reduced (20, 14);
    inner.removeFromTop (30 + 40);
    channel.setBounds (inner.removeFromTop (34).removeFromLeft (300));
    auto bottom = r.reduced (20, 14).removeFromBottom (90);
    bottom.removeFromTop (22);
    auto row = bottom.removeFromTop (28);
    outVolume.setBounds (row.removeFromLeft (row.getWidth() - 180));
    testButton.setBounds (row.withTrimmedLeft (12));
}

// ---------------------------------------------------------------- main

MainComponent::MainComponent (AudioEngine& e, Progress& p)
    : engine (e), progress (p), allLessons (loadLessons()),
      lessons ([this] { std::vector<Lesson> v; for (auto& l : allLessons) if (l.track != "blues") v.push_back (l); return v; }()),
      bluesLessons ([this] { std::vector<Lesson> v; for (auto& l : allLessons) if (l.track == "blues") v.push_back (l); return v; }()),
      homePage (p, lessons, bluesLessons), theoryPage (e, p, lessons), improvPage (e, p, bluesLessons), practicePage (e, p), audioPage (e)
{
    setLookAndFeel (&look);
    const char* names[] = { "Home", "Music Theory", "Improvisation", "Practice", "Audio" };
    for (int i = 0; i < numPages; ++i)
    {
        auto* b = tabs.add (new juce::TextButton (names[i]));
        b->setRadioGroupId (1);
        b->setClickingTogglesState (true);
        b->onClick = [this, i] { showPage (i); };
        addAndMakeVisible (b);
        addChildComponent (pageComponent (i));
    }
    homePage.onContinue = [this] (int lesson) { showPage (theory); theoryPage.openLesson (lesson); };
    homePage.onGoTo = [this] (int page) { showPage (page); };
    theoryPage.onProgressChanged = [this] { homePage.refresh(); };
    improvPage.onProgressChanged = [this] { homePage.refresh(); };

    setSize (1280, 820);
    showPage (home);
    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

juce::Component* MainComponent::pageComponent (int p)
{
    switch (p)
    {
        case home:     return &homePage;
        case theory:   return &theoryPage;
        case improv:   return &improvPage;
        case practice: return &practicePage;
        default:       return &audioPage;
    }
}

LiveInput* MainComponent::pageInput (int p)
{
    switch (p)
    {
        case theory:   return &theoryPage;
        case improv:   return &improvPage;
        case practice: return &practicePage;
        case audio:    return &audioPage;
        default:       return nullptr;
    }
}

void MainComponent::showPage (int p)
{
    current = p;
    for (int i = 0; i < numPages; ++i)
    {
        pageComponent (i)->setVisible (i == p);
        tabs[i]->setToggleState (i == p, juce::dontSendNotification);
    }
    if (p == home)
        homePage.refresh();
    resized();
    repaint();
}

void MainComponent::timerCallback()
{
    auto* input = pageInput (current);
    NoteEvent e;
    while (engine.popNote (e))
        if (input != nullptr)
            input->notePlayed (e);

    const auto f = engine.getPitch();
    if (input != nullptr)
        input->pitchChanged (f);
    if (f.valid != lastFrame.valid || (f.valid && juce::roundToInt (f.midi) != juce::roundToInt (lastFrame.midi)))
        repaint (getLocalBounds().removeFromTop (56).removeFromRight (220));
    lastFrame = f;
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (Palette::bg);
    auto header = getLocalBounds().removeFromTop (56);
    g.setColour (Palette::panel);
    g.fillRect (header);
    g.setColour (Palette::line);
    g.fillRect (header.removeFromBottom (1));

    g.setColour (Palette::gold);
    g.setFont (Fonts::ui (24.0f, true));
    g.drawText ("GuitarCoach", header.reduced (18, 0).removeFromLeft (190), juce::Justification::centredLeft);

    // what the guitar is playing right now
    auto live = header.removeFromRight (220).reduced (12, 8);
    g.setColour (Palette::panelLight);
    g.fillRoundedRectangle (live.toFloat(), 8.0f);
    g.setColour (lastFrame.valid ? Palette::blue : Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawText (lastFrame.valid ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + "Hearing " + noteFromMidi (juce::roundToInt (lastFrame.midi)).name (true)
                                : juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + "Guitar input",
                live, juce::Justification::centred);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56);
    header.removeFromLeft (210);
    header.removeFromRight (230);
    for (auto* t : tabs)
        t->setBounds (header.removeFromLeft (150).reduced (4, 10));
    for (int i = 0; i < numPages; ++i)
        pageComponent (i)->setBounds (r);
}
