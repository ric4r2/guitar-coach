#include "MainComponent.h"

using namespace theory;

// ---------------------------------------------------------------- home

void PlanRow::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat().reduced (0, 3);
    g.setColour (isMouseOver() ? Palette::panelLight.brighter (0.08f) : Palette::panelLight);
    g.fillRoundedRectangle (r, 8.0f);
    auto inner = r.reduced (12, 6);
    g.setColour (Palette::gold);
    g.setFont (Fonts::ui (15.0f, true));
    g.drawText (juce::String (item.minutes) + " min", inner.removeFromLeft (58), juce::Justification::centredLeft);

    auto mark = inner.removeFromRight (70);
    if (done)
    {
        const auto c = mark.withSizeKeepingCentre (26, 26);
        g.setColour (Palette::green);
        g.fillEllipse (c);
        g.setColour (Palette::ink);
        g.setFont (Fonts::ui (17.0f, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x9c\x93")), c, juce::Justification::centred);
    }
    else
    {
        g.setColour (Palette::blue);
        g.setFont (Fonts::ui (15.0f, true));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("Start \xe2\x80\xba")), mark, juce::Justification::centredRight);
    }
    g.setColour (done ? Palette::dim : Palette::text);
    g.setFont (Fonts::ui (16.0f, true));
    g.drawText (item.title, inner.removeFromTop (inner.getHeight() * 0.48f), juce::Justification::bottomLeft);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (13.5f));
    g.drawFittedText (item.detail, inner.toNearestInt(), juce::Justification::topLeft, 2, 1.0f);
}

HomePage::HomePage (Progress& p, const std::vector<Lesson>& l, const std::vector<Lesson>& b) : progress (p), lessons (l), bluesLessons (b), licks (loadLicks())
{
    addAndMakeVisible (continueButton);
    addAndMakeVisible (practiceButton);
    continueButton.onClick = [this] { if (onContinue) onContinue (nextLesson); };
    practiceButton.onClick = [this] { if (onGoTo) onGoTo (3); };
    length.addItem ("30 min", 30);
    length.addItem ("45 min", 45);
    length.addItem ("60 min", 60);
    length.setSelectedId ((int) progress.setting ("sessionMinutes", 45), juce::dontSendNotification);
    if (length.getSelectedId() == 0) length.setSelectedId (45, juce::dontSendNotification);
    length.onChange = [this] { progress.setSetting ("sessionMinutes", length.getSelectedId()); refresh(); };
    addAndMakeVisible (length);
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

    const PlanContext ctx { lessons, bluesLessons, licks };
    plan = todaysPlan (ctx, progress, juce::jmax (30, length.getSelectedId()));
    rows.clear();
    for (auto& item : plan)
    {
        auto* row = rows.add (new PlanRow());
        row->item = item;
        row->done = isDone (item, progress);
        row->onClick = [this, item] { if (onPlanItem) onPlanItem (item); };
        addAndMakeVisible (row);
    }
    resized();
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
    const int due = progress.dueKeys().size();
    const int todayMinutes = juce::roundToInt (progress.day (progress.today()).seconds / 60.0);
    g.drawText ("Streak: " + juce::String (streak) + (streak == 1 ? " day" : " days") + juce::String::fromUTF8 ("   \xc2\xb7   practised today: ") + juce::String (todayMinutes)
                + " min" + juce::String::fromUTF8 ("   \xc2\xb7   reviews due: ") + juce::String (due)
                + juce::String::fromUTF8 ("   \xc2\xb7   goal: ready for Gleason's Guitar Improv in 12 to 18 months"),
                r.removeFromTop (28), juce::Justification::centredLeft);
    r.removeFromTop (70);   // continue button row

    auto cols = r;
    auto left = cols.removeFromLeft (cols.getWidth() / 2 - 12);
    cols.removeFromLeft (24);
    auto right = cols;

    // today's session
    drawPanel (g, left.toFloat());
    auto inner = left.reduced (20, 14);
    int doneCount = 0, total = 0;
    for (auto* row : rows) { doneCount += row->done ? 1 : 0; total += row->item.minutes; }
    auto head = inner.removeFromTop (34);
    drawHeading (g, "Today's session", head, 21.0f);
    g.setColour (doneCount == rows.size() && rows.size() > 0 ? Palette::green : Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawText (juce::String (doneCount) + " of " + juce::String (rows.size()) + " done" + juce::String::fromUTF8 ("  \xc2\xb7  about ") + juce::String (total) + " min",
                head.withTrimmedLeft (170).withTrimmedRight (110), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawFittedText (doneCount == rows.size() && rows.size() > 0
                          ? "All done for today. Rest is part of practice: come back tomorrow, and reviews will be waiting when they are due."
                          : "Work down the list. Reviews come back on a schedule: 1, 3, 7, 14, 30 and 60 days after you pass, sooner if you slip.",
                      left.reduced (20, 14).removeFromBottom (40), juce::Justification::topLeft, 2, 1.0f);

    // progress, level by level
    drawPanel (g, right.toFloat());
    inner = right.reduced (20, 14);
    drawHeading (g, "Your progress", inner.removeFromTop (34), 21.0f);
    static const char* levelNames[] = { "", "Level 1: Foundations", "Level 2: Position player", "Level 3: Harmony", "Level 4: Pre-Gleason" };
    int lastLevel = -1;
    const int rowH = juce::jlimit (24, 38, (inner.getHeight() - 110) / 12);
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
    heading ("Music Theory: lessons finished");
    for (int level = 1; level <= 4; ++level)
    {
        int inLevel = 0, finished = 0;
        for (auto& l : lessons)
            if (l.level == level)
            {
                ++inLevel;
                finished += passedIn (l) == (int) l.exercises.size() ? 1 : 0;
            }
        if (inLevel > 0)
            barRow (levelNames[level], finished, inLevel);
        lastLevel = level;
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
    for (auto& l : lessons)
        if (l.id == "L24-readiness")
        {
            heading ("Ready for Gleason's book?");
            barRow ("Readiness quizzes passed", passedIn (l), (int) l.exercises.size());
        }
    inner.removeFromTop (10);
    drawHeading (g, "The road ahead", inner.removeFromTop (28), 18.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawFittedText ("Work through the four levels and the blues track, a session a day. When all four readiness quizzes pass, start Part 1 of Gleason's book and keep using the backing tracks, metronome and scale shapes next to it.",
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
    r.removeFromTop (22);

    auto left = r.removeFromLeft (r.getWidth() / 2 - 12).reduced (20, 14);
    length.setBounds (left.removeFromTop (34).removeFromRight (100).reduced (0, 4));
    left.removeFromTop (6);
    left.removeFromBottom (46);
    const int rowH = juce::jlimit (48, 70, left.getHeight() / juce::jmax (1, rows.size()));
    for (auto* pr : rows)
        pr->setBounds (left.removeFromTop (rowH));
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

    monitorToggle.onClick = [this] { engine.monitorOn.store (monitorToggle.getToggleState()); if (onMonitorChanged) onMonitorChanged(); };
    monitorVolume.setSliderStyle (juce::Slider::LinearHorizontal);
    monitorVolume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    monitorVolume.setRange (0.0, 1.0, 0.01);
    monitorVolume.onValueChange = [this] { engine.monitorVolume.store ((float) monitorVolume.getValue()); if (onMonitorChanged) onMonitorChanged(); };
    monitorTone.addItem ("Tone: clean amp", 1);
    monitorTone.addItem ("Tone: dry (straight from the input)", 2);
    monitorTone.onChange = [this] { engine.monitorWarm.store (monitorTone.getSelectedId() == 1); if (onMonitorChanged) onMonitorChanged(); };
    for (juce::Component* c : { (juce::Component*) &monitorToggle, (juce::Component*) &monitorVolume, (juce::Component*) &monitorTone })
        addAndMakeVisible (c);
    syncMonitor();
    startTimerHz (30);
}

void AudioPage::syncMonitor()
{
    monitorToggle.setToggleState (engine.monitorOn.load(), juce::dontSendNotification);
    monitorVolume.setValue (engine.monitorVolume.load(), juce::dontSendNotification);
    monitorTone.setSelectedId (engine.monitorWarm.load() ? 1 : 2, juce::dontSendNotification);
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

    auto bottom = r.reduced (20, 14).removeFromBottom (170);
    drawHeading (g, "Your guitar in the headphones", bottom.removeFromTop (26), 17.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    bottom.removeFromTop (64);
    g.drawText ("Output volume (examples, backing tracks, metronome)", bottom.removeFromTop (22), juce::Justification::centredLeft);
    g.drawFittedText ("Tip: the Audient ASIO driver gives the lowest delay (48 kHz, 128 samples), so what you hear stays in time with your hands. The guitar goes into the front instrument input (input 1).",
                      bottom.withTrimmedTop (36), juce::Justification::topLeft, 2, 1.0f);
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
    auto bottom = r.reduced (20, 14).removeFromBottom (170);
    bottom.removeFromTop (30);
    auto mon = bottom.removeFromTop (30);
    monitorToggle.setBounds (mon.removeFromLeft (170));
    monitorTone.setBounds (mon.removeFromLeft (290).reduced (2));
    auto mon2 = bottom.removeFromTop (30);
    monitorVolume.setBounds (mon2.removeFromLeft (mon2.getWidth() - 180));
    bottom.removeFromTop (4 + 22);
    auto row = bottom.removeFromTop (28);
    outVolume.setBounds (row.removeFromLeft (row.getWidth() - 180));
    testButton.setBounds (row.withTrimmedLeft (12));
}

// ---------------------------------------------------------------- main

MainComponent::MainComponent (AudioEngine& e, Progress& p)
    : engine (e), progress (p), allLessons (loadLessons()),
      lessons ([this] { std::vector<Lesson> v; for (auto& l : allLessons) if (l.track != "blues") v.push_back (l); return v; }()),
      bluesLessons ([this] { std::vector<Lesson> v; for (auto& l : allLessons) if (l.track == "blues") v.push_back (l); return v; }()),
      homePage (p, lessons, bluesLessons), theoryPage (e, p, lessons), improvPage (e, p, bluesLessons), practicePage (e, p),
      statsPage (p, lessons, bluesLessons), audioPage (e), licks (loadLicks())
{
    setLookAndFeel (&look);
    const char* names[] = { "Home", "Music Theory", "Improvisation", "Practice", "Progress", "Audio" };
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
    homePage.onPlanItem = [this] (const PlanItem& item) { openPlanItem (item); };
    engine.monitorOn.store ((bool) progress.setting ("monitorOn", true));
    engine.monitorVolume.store ((float) (double) progress.setting ("monitorVolume", 0.7));
    engine.monitorWarm.store ((bool) progress.setting ("monitorWarm", true));
    audioPage.syncMonitor();
    audioPage.onMonitorChanged = [this] { saveMonitor(); repaint (livePill()); };
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
        case stats:    return &statsPage;
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
    if (p == stats)
        statsPage.refresh();
    resized();
    repaint();
}

void MainComponent::saveMonitor()
{
    progress.setSetting ("monitorOn", engine.monitorOn.load());
    progress.setSetting ("monitorVolume", (double) engine.monitorVolume.load());
    progress.setSetting ("monitorWarm", engine.monitorWarm.load());
}

juce::Rectangle<int> MainComponent::livePill() const
{
    return getLocalBounds().removeFromTop (56).removeFromRight (220).reduced (12, 8);
}

void MainComponent::mouseDown (const juce::MouseEvent& e)
{
    // the guitar pill in the header switches hearing your guitar on and off
    if (livePill().contains (e.getPosition()))
    {
        engine.monitorOn.store (! engine.monitorOn.load());
        audioPage.syncMonitor();
        saveMonitor();
        repaint (livePill());
    }
}

void MainComponent::openPlanItem (const PlanItem& item)
{
    const PlanContext ctx { lessons, bluesLessons, licks };
    switch (item.kind)
    {
        case PlanItem::Kind::warmup:
            showPage (practice);
            practicePage.showTab (PracticePage::techniqueTab);
            practicePage.getTechnique().selectDrill (item.drill);
            break;
        case PlanItem::Kind::scales:
            showPage (practice);
            practicePage.showTab (PracticePage::scalesTab);
            practicePage.getScales().select (item.rootPc, item.scaleId, 0, 1);
            break;
        case PlanItem::Kind::improv:
            showPage (improv);
            improvPage.showTab (ImprovPage::licksTab);
            improvPage.getLicks().selectLickById (item.lickId);
            break;
        default:
        {
            bool blues = false;
            int index = 0;
            if (findLesson (ctx, item.lessonId, &blues, &index) == nullptr)
                break;
            if (blues)
            {
                showPage (improv);
                improvPage.showTab (ImprovPage::lessonsTab);
                improvPage.getLessons().openExercise (index, item.exercise);
            }
            else
            {
                showPage (theory);
                theoryPage.openExercise (index, item.exercise);
            }
            break;
        }
    }
}

void MainComponent::timerCallback()
{
    auto* input = pageInput (current);
    const double now = juce::Time::getMillisecondCounterHiRes() / 1000.0;
    NoteEvent e;
    while (engine.popNote (e))
    {
        lastActivity = now;
        if (input != nullptr)
            input->notePlayed (e);
    }

    double onset = 0.0;
    while (engine.popOnset (onset))
    {
        lastActivity = now;
        if (input != nullptr)
            input->onsetDetected (onset);
    }

    // practice time: the window is showing and something was played or clicked in the last minute
    const auto lastClick = juce::Desktop::getInstance().getMainMouseSource().getLastMouseDownTime();
    if (lastClick.toMilliseconds() > 0 && (juce::Time::getCurrentTime() - lastClick).inSeconds() < 60.0 && hasKeyboardFocus (true))
        lastActivity = juce::jmax (lastActivity, now - (juce::Time::getCurrentTime() - lastClick).inSeconds());
    const double dt = juce::jlimit (0.0, 1.0, now - lastTick);
    lastTick = now;
    if (isShowing() && now - lastActivity < 60.0)
    {
        unsentSeconds += dt;
        if (unsentSeconds >= 5.0)
        {
            progress.addPracticeSeconds (unsentSeconds);
            unsentSeconds = 0.0;
        }
    }

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
    auto live = livePill();
    const bool mon = engine.monitorOn.load();
    g.setColour (Palette::panelLight);
    g.fillRoundedRectangle (live.toFloat(), 8.0f);
    g.setColour (mon ? Palette::green.withAlpha (0.7f) : Palette::line);
    g.drawRoundedRectangle (live.toFloat().reduced (0.5f), 8.0f, 1.0f);
    auto textArea = live.reduced (10, 2);
    g.setColour (lastFrame.valid ? Palette::blue : Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    g.drawText (lastFrame.valid ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + noteFromMidi (juce::roundToInt (lastFrame.midi)).name (true)
                                : juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa  ")) + "Guitar",
                textArea.removeFromLeft (textArea.getWidth() / 2), juce::Justification::centredLeft);
    g.setColour (mon ? Palette::green : Palette::dim);
    g.setFont (Fonts::ui (13.0f));
    g.drawText (mon ? "monitor on" : "monitor off", textArea, juce::Justification::centredRight);
}

void MainComponent::resized()
{
    auto r = getLocalBounds();
    auto header = r.removeFromTop (56);
    header.removeFromLeft (210);
    header.removeFromRight (230);
    const int tabW = juce::jmin (150, header.getWidth() / juce::jmax (1, tabs.size()));
    for (auto* t : tabs)
        t->setBounds (header.removeFromLeft (tabW).reduced (4, 10));
    for (int i = 0; i < numPages; ++i)
        pageComponent (i)->setBounds (r);
}
