#include "MainComponent.h"

using namespace theory;

// ---------------------------------------------------------------- home

HomePage::HomePage (Progress& p, const std::vector<Lesson>& l) : progress (p), lessons (l)
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
        { "5 min",  "Warm-up: chromatic 1-2-3-4 on every string with the metronome. Start where you make a few mistakes, then add 5 BPM when it is clean." },
        { "10 min", "Theory: read the next lesson and pass one exercise." },
        { "10 min", "Fretboard: note finder drill. Then name every natural note on one string, out loud." },
        { "15 min", "Ears and playing: interval ear training, then play the intervals from different roots." },
        { "5 min",  "Review: redo one exercise you passed earlier this week." } };
    for (auto& it : items)
    {
        auto row = inner.removeFromTop (64);
        g.setColour (Palette::gold);
        g.setFont (Fonts::ui (17.0f, true));
        g.drawText (it.mins, row.removeFromLeft (70), juce::Justification::topLeft);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (16.0f));
        g.drawFittedText (it.what, row, juce::Justification::topLeft, 3);
    }
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawFittedText ("Practise in short cycles: repeat until you tire, rest about half as long, repeat. Little and often beats long and rare.",
                      inner.removeFromTop (44), juce::Justification::topLeft, 2);

    // progress
    drawPanel (g, right.toFloat());
    inner = right.reduced (20, 14);
    drawHeading (g, "Level 1: Foundations", inner.removeFromTop (34), 21.0f);
    for (auto& l : lessons)
    {
        int passed = 0;
        for (int x = 0; x < (int) l.exercises.size(); ++x)
            passed += progress.passed (l.exerciseKey (x)) ? 1 : 0;
        auto row = inner.removeFromTop (58);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (17.0f));
        g.drawText (l.title, row.removeFromTop (26), juce::Justification::centredLeft);
        auto bar = row.removeFromTop (14).toFloat().withTrimmedRight (90);
        g.setColour (Palette::panelLight);
        g.fillRoundedRectangle (bar, 5.0f);
        g.setColour (passed == (int) l.exercises.size() ? Palette::green : Palette::gold);
        g.fillRoundedRectangle (bar.withWidth (bar.getWidth() * (float) passed / (float) juce::jmax (1, (int) l.exercises.size())), 5.0f);
        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (14.0f));
        g.drawText (juce::String (passed) + " of " + juce::String ((int) l.exercises.size()), juce::Rectangle<float> (bar.getRight() + 8, bar.getY() - 4, 80, 22), juce::Justification::centredLeft);
    }
    inner.removeFromTop (10);
    drawHeading (g, "Coming next", inner.removeFromTop (30), 18.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawFittedText ("Level 1 continues with the major scale, keys C, G and F, triads, rhythm reading and the 12-bar blues in E and A (milestones 2 and 3).",
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

// ---------------------------------------------------------------- improvisation

ImprovPage::ImprovPage()
{
    for (int pc : { 4, 9, 7, 2, 0, 5, 10 })
        key.addItem ("Key of " + pcName (pc, pc == 5 || pc == 10), pc + 1);
    key.setSelectedId (5, juce::dontSendNotification);
    key.onChange = [this] { repaint(); };
    quickChange.onClick = [this] { repaint(); };
    addAndMakeVisible (key);
    addAndMakeVisible (quickChange);

    text.setLines ({
        "## The blues track",
        "Improvisation starts with the 12-bar blues, as it does at Berklee and in Gleason's book. The chart on the left is the form every blues player knows: three chords, I, IV and V, all played as dominant 7th chords. Count the bars out loud while a song plays until you always know where bar 1 is.",
        "## Think in numbers",
        "The chart shows Roman numerals as well as chord names. Gleason's book thinks in numbers all the way through, so a lick learned in E can be moved to A by moving its numbers. Change the key above and watch the numbers stay the same.",
        "## Coming in milestone 3",
        "- Backing tracks made in the app: shuffle, straight, slow and funk feels, any key and tempo",
        "- Minor pentatonic and blues scales in five shapes, then major pentatonic and Mixolydian",
        "- A lick library in notation, tab and on the neck, scored by pitch detection as you play along",
        "- Call and response, bends, slides and vibrato, and targeting chord tones on the changes",
        "## Getting ready for Gleason's book",
        "Before the book, the app makes sure you can: name any fret, read single-note lines in position, play the five octave shapes, count and play rhythmic words, build triads and seventh chords, and know the chords of every key by number." });
    addAndMakeVisible (text);
}

void ImprovPage::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (r.getWidth() / 2);
    drawPanel (g, left.toFloat());
    auto inner = left.reduced (20, 14);
    drawHeading (g, "12-bar blues", inner.removeFromTop (34), 22.0f);
    inner.removeFromTop (46);

    const int rootPc = key.getSelectedId() - 1;
    const bool flats = rootPc == 5 || rootPc == 10 || rootPc == 0;
    static const int normal[12] = { 1, 1, 1, 1, 4, 4, 1, 1, 5, 4, 1, 5 };
    static const int quick[12]  = { 1, 4, 1, 1, 4, 4, 1, 1, 5, 4, 1, 5 };
    const int* form = quickChange.getToggleState() ? quick : normal;

    const int cellW = inner.getWidth() / 4, cellH = 92;
    for (int bar = 0; bar < 12; ++bar)
    {
        auto cell = juce::Rectangle<int> (inner.getX() + (bar % 4) * cellW, inner.getY() + (bar / 4) * (cellH + 10), cellW, cellH).reduced (4);
        const int degree = form[bar];
        const int pc = mod12 (rootPc + (degree == 1 ? 0 : degree == 4 ? 5 : 7));
        g.setColour (degree == 1 ? Palette::panelLight : degree == 4 ? Palette::line : Palette::rosewood.brighter (0.3f));
        g.fillRoundedRectangle (cell.toFloat(), 6.0f);
        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (13.0f));
        g.drawText ("bar " + juce::String (bar + 1), cell.reduced (8, 4), juce::Justification::topLeft);
        g.setColour (Palette::text);
        g.setFont (Fonts::ui (26.0f, true));
        g.drawText (pcName (pc, flats) + "7", cell.translated (0, 4), juce::Justification::centred);
        g.setColour (Palette::gold);
        g.setFont (Fonts::ui (16.0f, true));
        g.drawText (degree == 1 ? "I7" : degree == 4 ? "IV7" : "V7", cell.reduced (8, 6), juce::Justification::bottomRight);
    }
}

void ImprovPage::resized()
{
    auto r = getLocalBounds().reduced (24, 16);
    auto left = r.removeFromLeft (r.getWidth() / 2).reduced (20, 14);
    left.removeFromTop (38);
    auto row = left.removeFromTop (34);
    key.setBounds (row.removeFromLeft (150).reduced (2));
    row.removeFromLeft (12);
    quickChange.setBounds (row.removeFromLeft (260));
    r.removeFromLeft (24);
    text.setBounds (r.withHeight (text.heightForWidth (r.getWidth())));
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
    : engine (e), progress (p), lessons (loadLessons()),
      homePage (p, lessons), theoryPage (e, p, lessons), practicePage (e, p), audioPage (e)
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
