#include "PracticePage.h"

using namespace theory;

// ---------------------------------------------------------------- tuner

void TunerView::setPitch (const PitchFrame& f)
{
    if (f.valid)
    {
        const float cents = (f.midi - std::round (f.midi)) * 100.0f;
        smoothCents = frame.valid ? smoothCents + 0.35f * (cents - smoothCents) : cents;
        lastValid = juce::Time::getMillisecondCounter();
        frame = f;
    }
    else if (juce::Time::getMillisecondCounter() - lastValid > 600)
        frame.valid = false;
    repaint();
}

void TunerView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r);
    auto inner = r.reduced (16, 10);
    drawHeading (g, "Tuner", inner.removeFromTop (30).toNearestInt(), 19.0f);

    const bool ok = frame.valid;
    const int midi = juce::roundToInt (frame.midi);
    const bool inTune = ok && std::abs (smoothCents) < 5.0f;

    auto nameArea = inner.removeFromLeft (150);
    g.setColour (ok ? (inTune ? Palette::green : Palette::text) : Palette::dim);
    g.setFont (Fonts::ui (64.0f, true));
    g.drawText (ok ? noteFromMidi (midi).name() : "-", nameArea.removeFromTop (90), juce::Justification::centred);
    g.setFont (Fonts::ui (16.0f));
    g.setColour (Palette::dim);
    g.drawText (ok ? noteFromMidi (midi).name (true) + "  " + juce::String (frame.hz, 1) + " Hz" : "play a string", nameArea.removeFromTop (24), juce::Justification::centred);

    // needle meter, -50..+50 cents
    auto meter = inner.reduced (10, 6);
    const float cx = meter.getCentreX(), base = meter.getBottom() - 10.0f, radius = juce::jmin (meter.getWidth() * 0.5f, meter.getHeight() - 20.0f);
    g.setColour (Palette::line);
    for (int c = -50; c <= 50; c += 10)
    {
        const float a = juce::degreesToRadians ((float) c * 1.2f);
        const float len = c == 0 ? 18.0f : 10.0f;
        g.setColour (c == 0 ? Palette::green : Palette::dim);
        g.drawLine (cx + std::sin (a) * (radius - len), base - std::cos (a) * (radius - len), cx + std::sin (a) * radius, base - std::cos (a) * radius, c == 0 ? 3.0f : 1.5f);
    }
    g.setFont (Fonts::ui (13.0f));
    g.setColour (Palette::dim);
    g.drawText ("flat", juce::Rectangle<float> (cx - radius, base - 18, 60, 18), juce::Justification::left);
    g.drawText ("sharp", juce::Rectangle<float> (cx + radius - 60, base - 18, 60, 18), juce::Justification::right);
    if (ok)
    {
        const float a = juce::degreesToRadians (juce::jlimit (-50.0f, 50.0f, smoothCents) * 1.2f);
        g.setColour (inTune ? Palette::green : Palette::gold);
        g.drawLine (cx, base, cx + std::sin (a) * (radius - 4), base - std::cos (a) * (radius - 4), 3.5f);
        g.setFont (Fonts::ui (15.0f, true));
        g.drawText ((smoothCents >= 0 ? "+" : "") + juce::String (juce::roundToInt (smoothCents)) + " cents",
                    juce::Rectangle<float> (cx - 60, base - radius * 0.45f, 120, 20), juce::Justification::centred);
    }
    g.setColour (Palette::gold);
    g.fillEllipse (cx - 6, base - 6, 12, 12);
}

// ---------------------------------------------------------------- metronome

MetronomePanel::MetronomePanel (AudioEngine& e) : engine (e)
{
    bpm.setSliderStyle (juce::Slider::LinearHorizontal);
    bpm.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 28);
    bpm.setRange (30.0, 260.0, 1.0);
    bpm.setValue (engine.metronomeBpm.load(), juce::dontSendNotification);
    bpm.onValueChange = [this] { engine.metronomeBpm.store ((float) bpm.getValue()); };
    addAndMakeVisible (bpm);

    volume.setSliderStyle (juce::Slider::LinearHorizontal);
    volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volume.setRange (0.0, 10.0, 0.1);
    volume.setValue (engine.metronomeVolume.load(), juce::dontSendNotification);
    volume.onValueChange = [this] { engine.metronomeVolume.store ((float) volume.getValue()); };
    addAndMakeVisible (volume);

    for (int b : { 2, 3, 4, 5, 6, 7 })
        beats.addItem (juce::String (b) + "/4", b);
    beats.setSelectedId (engine.metronomeBeats.load(), juce::dontSendNotification);
    beats.onChange = [this] { engine.metronomeBeats.store (beats.getSelectedId()); };
    addAndMakeVisible (beats);

    startButton.onClick = [this]
    {
        const bool on = ! engine.metronomeOn.load();
        engine.metronomeOn.store (on);
        startButton.setButtonText (on ? "Stop" : "Start");
        startButton.setToggleState (on, juce::dontSendNotification);
    };
    tapButton.onClick = [this]
    {
        const auto now = juce::Time::getMillisecondCounter();
        if (! taps.isEmpty() && now - taps.getLast() > 2000)
            taps.clear();
        taps.add (now);
        if (taps.size() > 5) taps.remove (0);
        if (taps.size() >= 2)
            bpm.setValue (60000.0 * (taps.size() - 1) / (double) (taps.getLast() - taps.getFirst()));
    };
    slower.onClick = [this] { bpm.setValue (bpm.getValue() - 5); };
    faster.onClick = [this] { bpm.setValue (bpm.getValue() + 5); };
    for (auto* b : { &startButton, &tapButton, &slower, &faster })
        addAndMakeVisible (b);
    startTimerHz (30);
}

void MetronomePanel::timerCallback()
{
    const int p = engine.getMetronomePulse();
    if (p != shownPulse) { shownPulse = p; repaint(); }
    // the drills can start the metronome and move the tempo: keep the controls in step
    const bool on = engine.metronomeOn.load();
    if (startButton.getToggleState() != on)
    {
        startButton.setButtonText (on ? "Stop" : "Start");
        startButton.setToggleState (on, juce::dontSendNotification);
    }
    if (! bpm.isMouseButtonDown() && std::abs (bpm.getValue() - engine.metronomeBpm.load()) > 0.5)
        bpm.setValue (engine.metronomeBpm.load(), juce::dontSendNotification);
}

void MetronomePanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r);
    auto inner = r.reduced (16, 10);
    drawHeading (g, "Metronome", inner.removeFromTop (30).toNearestInt(), 19.0f);

    // beat lamps
    const int n = engine.metronomeBeats.load();
    auto lamps = inner.removeFromBottom (40);
    const float d = 24.0f;
    const float x0 = lamps.getCentreX() - n * (d + 12) * 0.5f;
    for (int i = 0; i < n; ++i)
    {
        const bool lit = engine.metronomeOn.load() && i == shownPulse;
        g.setColour (lit ? (i == 0 ? Palette::gold : Palette::green) : Palette::panelLight);
        g.fillEllipse (x0 + i * (d + 12), lamps.getCentreY() - d * 0.5f, d, d);
    }
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawText ("Volume", volume.getBounds().translated (0, -18).withHeight (18), juce::Justification::centredLeft);
}

void MetronomePanel::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    r.removeFromTop (36);
    auto row1 = r.removeFromTop (36);
    slower.setBounds (row1.removeFromLeft (50).reduced (2));
    bpm.setBounds (row1.removeFromLeft (row1.getWidth() - 56).reduced (4, 2));
    faster.setBounds (row1.removeFromLeft (50).reduced (2));
    r.removeFromTop (10);
    auto row2 = r.removeFromTop (38);
    startButton.setBounds (row2.removeFromLeft (110).reduced (2));
    tapButton.setBounds (row2.removeFromLeft (80).reduced (2));
    beats.setBounds (row2.removeFromLeft (90).reduced (4, 4));
    row2.removeFromLeft (10);
    volume.setBounds (row2.withTrimmedTop (14).reduced (4, 0));
}

// ---------------------------------------------------------------- fretboard explorer

FretboardExplorer::FretboardExplorer (AudioEngine& e, Progress& p) : engine (e), progress (p)
{
    for (int pc = 0; pc < 12; ++pc)
        root.addItem ("Root " + pcNameBoth (pc), pc + 1);
    root.setSelectedId (1, juce::dontSendNotification);
    labels.addItemList ({ "Note names", "Intervals from root", "Semitones from root", "Tones from root" }, 1);
    labels.setSelectedId (1, juce::dontSendNotification);
    show.addItemList ({ "All notes", "Natural notes", "Root only (octaves)" }, 1);
    show.setSelectedId (2, juce::dontSendNotification);
    for (auto* c : { &root, &labels, &show })
    {
        c->onChange = [this] { updateMarks(); };
        addAndMakeVisible (c);
    }
    addAndMakeVisible (drillButton);
    drillButton.onClick = [this] { if (drilling) { drilling = false; stopTimer(); drillButton.setButtonText ("Start note finder"); updateMarks(); } else startDrill(); };

    board.setMaxFret (15);
    board.setClickable (true);
    board.onClick = [this] (int s, int f)
    {
        const int midi = fretMidi (s, f);
        engine.playNote (midi, 0.0, 1.5);
        const int rootPc = root.getSelectedId() - 1;
        const int semis = mod12 (midi - rootPc);
        info = noteFromMidi (midi).name (true) + " on the " + stringName (s) + (f == 0 ? juce::String (", open") : ", fret " + juce::String (f))
             + juce::String::fromUTF8 ("   ·   ") + octaveIntervals()[(size_t) semis].longName() + " above " + pcName (rootPc) + ": " + semitonesText (semis)
             + " = " + tonesText (semis);
        repaint();
    };
    addAndMakeVisible (board);
    updateMarks();
}

void FretboardExplorer::updateMarks()
{
    if (drilling)
    {
        board.setMarks (found);
        return;
    }
    const int rootPc = root.getSelectedId() - 1;
    const int mode = labels.getSelectedId(), filter = show.getSelectedId();
    std::vector<FretMark> marks;
    for (int s = 1; s <= 6; ++s)
        for (int f = 0; f <= board.getMaxFret(); ++f)
        {
            const int midi = fretMidi (s, f);
            const auto n = noteFromMidi (midi);
            const int semis = mod12 (midi - rootPc);
            if (filter == 2 && n.acc != 0 && semis != 0) continue;
            if (filter == 3 && semis != 0) continue;
            juce::String label;
            switch (mode)
            {
                case 2:  label = semis == 0 ? "R" : octaveIntervals()[(size_t) semis].shortName(); break;
                case 3:  label = juce::String (semis); break;
                case 4:  label = tonesText (semis).upToFirstOccurrenceOf (" ", false, false); break;
                default: label = n.acc != 0 ? n.name() : n.name(); break;
            }
            marks.push_back ({ s, f, label, semis == 0 ? 1 : (n.acc != 0 ? 4 : 0) });
        }
    board.setMarks (marks);
}

void FretboardExplorer::startDrill (juce::int64 seed)
{
    rng.setSeed (seed != 0 ? seed : juce::Time::currentTimeMillis());
    static const int naturals[] = { 0, 2, 4, 5, 7, 9, 11 };
    drillPc = naturals[rng.nextInt (7)];
    drillString = 6;
    drillStart = juce::Time::getMillisecondCounterHiRes();
    found.clear();
    drillResult.clear();
    drilling = true;
    drillButton.setButtonText ("Stop");
    updateMarks();
    startTimerHz (10);
}

void FretboardExplorer::notePlayed (const NoteEvent& e)
{
    if (! drilling)
        return;
    // the note on the current string, frets 0 to 12
    for (int f = 0; f <= 12; ++f)
    {
        if (fretMidi (drillString, f) != e.midi || mod12 (e.midi) != drillPc)
            continue;
        found.push_back ({ drillString, f, pcName (drillPc), 2 });
        if (--drillString < 1)
        {
            const double secs = (juce::Time::getMillisecondCounterHiRes() - drillStart) / 1000.0;
            const double perNote = secs / 6.0;
            const double best = progress.bestDrill ("noteFinder");
            progress.recordDrill ("noteFinder", perNote);
            drillResult = "All six " + pcName (drillPc) + "s in " + juce::String (secs, 1) + " s (" + juce::String (perNote, 1) + " s per note"
                        + (best < 0 || perNote < best ? ", a new best!)" : ", best " + juce::String (best, 1) + " s)");
            drilling = false;
            stopTimer();
            drillButton.setButtonText ("Start note finder");
            board.setMarks (found);
        }
        else
            updateMarks();
        repaint();
        return;
    }
}

void FretboardExplorer::pitchChanged (const PitchFrame& f)
{
    board.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1);
}

void FretboardExplorer::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    drawPanel (g, r);
    auto inner = getLocalBounds().reduced (16, 10);
    drawHeading (g, "Fretboard", inner.removeFromTop (30), 19.0f);

    auto status = juce::Rectangle<int> (inner.getX(), board.getBottom() + 4, inner.getWidth(), 28);
    g.setFont (Fonts::ui (17.0f));
    if (drilling)
    {
        const double secs = (juce::Time::getMillisecondCounterHiRes() - drillStart) / 1000.0;
        g.setColour (Palette::gold);
        g.drawText ("Play " + pcName (drillPc) + " on the " + stringName (drillString) + juce::String::fromUTF8 ("   ·   ") + juce::String (6 - drillString) + juce::String::fromUTF8 (" of 6   ·   ")
                    + juce::String (secs, 1) + " s", status, juce::Justification::centredLeft);
    }
    else if (drillResult.isNotEmpty())
    {
        g.setColour (Palette::green);
        g.drawText (drillResult, status, juce::Justification::centredLeft);
    }
    else
    {
        g.setColour (Palette::dim);
        g.drawText (info.isNotEmpty() ? info : "Click any fret to hear it and see its interval from the root. What you play lights up in blue.",
                    status, juce::Justification::centredLeft);
    }
}

void FretboardExplorer::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (130);
    root.setBounds (top.removeFromLeft (130).reduced (2));
    labels.setBounds (top.removeFromLeft (190).reduced (2));
    show.setBounds (top.removeFromLeft (190).reduced (2));
    drillButton.setBounds (top.removeFromRight (180).reduced (2, 0));
    r.removeFromTop (8);
    r.removeFromBottom (34);
    board.setBounds (r);
}

// ---------------------------------------------------------------- scales and technique

PatternTrainer::PatternTrainer (AudioEngine& e, Progress& p, Mode m) : engine (e), progress (p), mode (m)
{
    if (mode == Mode::scales)
    {
        for (int pc = 0; pc < 12; ++pc)
            root.addItem (pcNameBoth (pc), pc + 1);
        root.setSelectedId (8, juce::dontSendNotification);   // G
        int id = 1;
        for (auto& t : scaleTypes())
            scale.addItem (t.name, id++);
        scale.setSelectedId (1, juce::dontSendNotification);
        pattern.addItemList (runPatternNames(), 1);
        labels.addItemList ({ "Note names", "Scale degrees" }, 1);
        for (auto* c : { &root, &scale })
            c->onChange = [this] { rebuildFingerings(); };
        fingering.onChange = [this] { rebuildTargets(); };
    }
    else
    {
        pattern.addItemList (techDrillNames(), 1);
        for (int f = 1; f <= 12; ++f)
            startFret.addItem ("From fret " + juce::String (f), f + 1);
        startFret.setSelectedId (6, juce::dontSendNotification);   // fret 5
        startFret.onChange = [this] { rebuildTargets(); };
        labels.addItemList ({ "Finger numbers", "Note names" }, 1);
    }
    pattern.setSelectedId (mode == Mode::scales ? 2 : 1, juce::dontSendNotification);
    pattern.onChange = [this] { rebuildTargets(); };
    labels.setSelectedId (1, juce::dontSendNotification);
    labels.onChange = [this] { updateMarks(); };
    notesPerBeat.addItem ("Quarter notes", 1);
    notesPerBeat.addItem ("Eighth notes", 2);
    notesPerBeat.addItem ("Triplets", 3);
    notesPerBeat.addItem ("Sixteenths", 4);
    notesPerBeat.setSelectedId (mode == Mode::scales ? 1 : 2, juce::dontSendNotification);
    notesPerBeat.onChange = [this] { repaint(); };
    ladder.setToggleState (true, juce::dontSendNotification);

    for (juce::Component* c : { (juce::Component*) &root, (juce::Component*) &scale, (juce::Component*) &fingering, (juce::Component*) &pattern,
                                (juce::Component*) &labels, (juce::Component*) &notesPerBeat, (juce::Component*) &startFret,
                                (juce::Component*) &ladder, (juce::Component*) &startButton, (juce::Component*) &listenButton })
        addChildComponent (c);
    for (juce::Component* c : { (juce::Component*) &pattern, (juce::Component*) &labels, (juce::Component*) &notesPerBeat,
                                (juce::Component*) &ladder, (juce::Component*) &startButton, (juce::Component*) &listenButton })
        c->setVisible (true);
    const std::vector<juce::Component*> modeControls = mode == Mode::scales ? std::vector<juce::Component*> { &root, &scale, &fingering }
                                                                            : std::vector<juce::Component*> { &startFret };
    for (auto* c : modeControls)
        c->setVisible (true);

    startButton.onClick = [this] { if (running) stopRun(); else startRun(); };
    listenButton.onClick = [this]
    {
        engine.stopNotes();
        const double gap = juce::jmax (0.12, 60.0 / (double) engine.metronomeBpm.load() / npb());
        std::vector<int> m;
        for (size_t i = 0; i < targets.size() && i < 60; ++i)
            m.push_back (targets[i].midi);
        engine.playSequence (m, gap, gap * 1.5);
    };

    board.setClickable (true);
    board.onClick = [this] (int s, int f) { engine.playNote (fretMidi (s, f), 0.0, 1.2); };
    addAndMakeVisible (board);
    if (mode == Mode::scales)
        rebuildFingerings();
    else
        rebuildTargets();
}

const ScaleType& PatternTrainer::currentScale() const
{
    return scaleTypes()[(size_t) juce::jlimit (0, (int) scaleTypes().size() - 1, scale.getSelectedId() - 1)];
}

Note PatternTrainer::rootNote() const
{
    // the usual key spelling: flats for D-flat, E-flat, A-flat, B-flat major; sharps for C-sharp, F-sharp, G-sharp minor
    const int pc = root.getSelectedId() - 1;
    const bool flats = currentScale().isMinor() ? (pc == 3 || pc == 10) : (pc == 1 || pc == 3 || pc == 8 || pc == 10);
    auto n = noteFromMidi (48 + mod12 (pc), flats);
    return n;
}

void PatternTrainer::select (int rootPc, const juce::String& scaleId, int fingeringIndex, int patternIndex)
{
    root.setSelectedId (mod12 (rootPc) + 1, juce::dontSendNotification);
    for (int i = 0; i < (int) scaleTypes().size(); ++i)
        if (scaleTypes()[(size_t) i].id == scaleId)
            scale.setSelectedId (i + 1, juce::dontSendNotification);
    pattern.setSelectedId (patternIndex + 1, juce::dontSendNotification);
    rebuildFingerings();
    fingering.setSelectedId (fingeringIndex + 1, juce::dontSendNotification);
    rebuildTargets();
}

void PatternTrainer::rebuildFingerings()
{
    const auto& st = currentScale();
    const int pc = root.getSelectedId() - 1;
    fingerings = positionFingerings (pc, st);
    for (auto& f : threeNpsFingerings (pc, st))
        fingerings.push_back (f);
    spelling = scaleSpelling (rootNote(), st);
    fingering.clear (juce::dontSendNotification);
    for (int i = 0; i < (int) fingerings.size(); ++i)
        fingering.addItem (fingerings[(size_t) i].name, i + 1);
    fingering.setSelectedId (1, juce::dontSendNotification);
    rebuildTargets();
}

void PatternTrainer::rebuildTargets()
{
    stopRun();
    result.clear();
    if (mode == Mode::scales)
    {
        const int i = juce::jlimit (0, (int) fingerings.size() - 1, fingering.getSelectedId() - 1);
        targets = fingerings.empty() ? std::vector<FretNote>() : scaleRun (fingerings[(size_t) i], (RunPattern) (pattern.getSelectedId() - 1));
    }
    else
        targets = techniqueDrill ((TechDrill) (pattern.getSelectedId() - 1), startFret.getSelectedId() - 1);

    int high = 12;
    for (auto& t : targets)
        high = juce::jmax (high, t.fret + 1);
    board.setMaxFret (juce::jmax (15, high));
    updateMarks();
}

juce::String PatternTrainer::label (const FretNote& n) const
{
    if (mode == Mode::technique)
        return labels.getSelectedId() == 1 ? juce::String (n.degree) : noteFromMidi (n.midi).name();
    if (labels.getSelectedId() == 2)
        return n.degree == 0 ? juce::String ("R") : degreeLabel (currentScale().intervals[(size_t) n.degree]);
    return spelling[(size_t) mod12 (n.midi)];
}

void PatternTrainer::updateMarks()
{
    std::vector<FretMark> marks;
    // every position used, once
    for (auto& t : targets)
    {
        bool seen = false;
        for (auto& m : marks)
            seen = seen || (m.string == t.string && m.fret == t.fret);
        if (! seen)
            marks.push_back ({ t.string, t.fret, label (t), mode == Mode::scales && t.degree == 0 ? 1 : 0 });
    }
    if (running && pos < (int) targets.size())
    {
        const auto& t = targets[(size_t) pos];
        marks.push_back ({ t.string, t.fret, label (t), 4 });
    }
    if (juce::Time::getMillisecondCounter() < flashUntil)
        marks.insert (marks.end(), flashMarks.begin(), flashMarks.end());
    board.setMarks (marks);
    repaint();
}

juce::String PatternTrainer::drillKey() const
{
    if (mode == Mode::scales)
        return "scale/" + currentScale().id + "/" + juce::String (pattern.getSelectedId()) + "/" + juce::String (npb());
    return "tech/" + juce::String (pattern.getSelectedId()) + "/" + juce::String (npb());
}

void PatternTrainer::startRun()
{
    if (targets.empty())
        return;
    if (ladder.getToggleState())
        engine.metronomeOn.store (true);
    running = true;
    pos = 0;
    mistakes = 0;
    times.clear();
    result.clear();
    startButton.setButtonText ("Stop");
    startTimerHz (15);
    updateMarks();
}

void PatternTrainer::stopRun()
{
    running = false;
    startButton.setButtonText ("Start");
    stopTimer();
}

void PatternTrainer::finishRun (double bpmOverride)
{
    stopRun();
    const bool metro = engine.metronomeOn.load() || bpmOverride > 0;
    const double bpm = bpmOverride > 0 ? bpmOverride : (double) engine.metronomeBpm.load();
    const auto s = scoreRun (times, mistakes, metro ? bpm : 0.0, npb());
    const juce::String dot = juce::String::fromUTF8 ("  \xc2\xb7  ");
    result = juce::String (juce::roundToInt (s.accuracy * 100)) + "% right notes (" + juce::String (mistakes) + " wrong"
           + ")" + dot + "timing " + juce::String (juce::roundToInt (s.evenness * 100)) + "% even" + dot + "you played at about "
           + juce::String (juce::roundToInt (s.averageBpm)) + " BPM";
    if (s.clean)
    {
        resultColour = Palette::green;
        result = "Clean run! " + result + ".";
        if (metro)
        {
            progress.recordTempo (drillKey(), bpm);
            if (ladder.getToggleState() && bpmOverride <= 0)
            {
                engine.metronomeBpm.store ((float) juce::jmin (260.0, bpm + 5.0));
                result << " Tempo up to " << juce::roundToInt (bpm + 5.0) << " BPM.";
            }
        }
    }
    else
    {
        resultColour = Palette::gold;
        const bool sloppy = s.accuracy < 0.85;
        result << (sloppy ? ". Too many wrong notes: slow down until only 1 in 10 or 20 is wrong." : ". Nearly: play it again at this tempo.");
        if (sloppy && metro && ladder.getToggleState() && bpmOverride <= 0)
        {
            engine.metronomeBpm.store ((float) juce::jmax (30.0, bpm - 5.0));
            result << " Tempo down to " << juce::roundToInt (bpm - 5.0) << " BPM.";
        }
    }
    updateMarks();
}

void PatternTrainer::notePlayed (const NoteEvent& e)
{
    if (! running || pos >= (int) targets.size())
        return;
    const auto& want = targets[(size_t) pos];
    if (e.midi == want.midi)
    {
        times.push_back (e.time);
        flashMarks = { { want.string, want.fret, label (want), 2 } };
        flashUntil = juce::Time::getMillisecondCounter() + 250;
        if (++pos >= (int) targets.size())
            finishRun();
        else
            updateMarks();
        return;
    }
    // the note just played, heard again straight away, is the same pluck: ignore it
    if (pos > 0 && e.midi == targets[(size_t) pos - 1].midi && ! times.empty() && e.time - times.back() < 0.15)
        return;
    ++mistakes;
    // show the wrong note where it is closest to the target
    FretPos best { want.string, want.fret };
    int dist = 99;
    for (auto& p : positionsOf (e.midi, board.getMaxFret()))
        if (std::abs (p.fret - want.fret) + std::abs (p.string - want.string) < dist)
        {
            dist = std::abs (p.fret - want.fret) + std::abs (p.string - want.string);
            best = p;
        }
    flashMarks = { { best.string, best.fret, noteFromMidi (e.midi).name(), 3 } };
    flashUntil = juce::Time::getMillisecondCounter() + 600;
    updateMarks();
}

void PatternTrainer::simulateRun (int hits, int wrong, double bpm)
{
    startRun();
    const double gap = 60.0 / bpm / npb();
    for (int i = 0; i < hits && running; ++i)
    {
        if (i == 3)
            mistakes += wrong;
        times.push_back (i * gap + 0.05 * gap * ((i % 3) - 1));   // on the beat, give or take 5%
        if (++pos >= (int) targets.size())
            finishRun (bpm);
    }
    updateMarks();
}

void PatternTrainer::pitchChanged (const PitchFrame& f)
{
    board.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1);
}

void PatternTrainer::timerCallback()
{
    if (flashUntil != 0 && juce::Time::getMillisecondCounter() >= flashUntil)
    {
        flashUntil = 0;
        updateMarks();
    }
}

void PatternTrainer::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto inner = getLocalBounds().reduced (16, 10);
    drawHeading (g, mode == Mode::scales ? "Scales" : "Technique", inner.removeFromTop (30), 19.0f);

    auto status = juce::Rectangle<int> (inner.getX(), board.getBottom() + 4, inner.getWidth(), getHeight() - board.getBottom() - 10);
    const juce::String dot = juce::String::fromUTF8 ("   \xc2\xb7   ");
    const double best = progress.bestTempo (drillKey());
    const auto bestText = best > 0 ? "best clean tempo " + juce::String (juce::roundToInt (best)) + " BPM" : juce::String ("no clean run yet");
    g.setFont (Fonts::ui (17.0f));
    if (running && pos < (int) targets.size())
    {
        const auto& t = targets[(size_t) pos];
        g.setColour (Palette::blue);
        g.drawText ("Play " + noteFromMidi (t.midi).name (true) + " (" + stringName (t.string) + ", fret " + juce::String (t.fret) + ")" + dot
                        + "note " + juce::String (pos + 1) + " of " + juce::String ((int) targets.size()) + dot + juce::String (mistakes) + " wrong",
                    status.removeFromTop (26), juce::Justification::centredLeft);
    }
    else if (result.isNotEmpty())
    {
        g.setColour (resultColour);
        g.drawFittedText (result, status.removeFromTop (44), juce::Justification::topLeft, 2);
    }
    else
    {
        g.setColour (Palette::dim);
        g.drawFittedText (mode == Mode::scales
                              ? "Pick a scale and a fingering, turn the metronome on, press Start and play each note as it lights up in blue."
                              : "One finger per fret. Keep fingers close to the strings, and pick strictly down-up. Press Start and play each note as it lights up.",
                          status.removeFromTop (44), juce::Justification::topLeft, 2);
    }
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (14.0f));
    g.drawText (juce::String ((int) targets.size()) + " notes" + dot + juce::String (juce::roundToInt (engine.metronomeBpm.load())) + " BPM, "
                    + notesPerBeat.getText().toLowerCase() + (engine.metronomeOn.load() ? juce::String() : " (metronome off)") + dot + bestText,
                status.removeFromBottom (20), juce::Justification::centredLeft);
}

void PatternTrainer::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (120);
    auto row2 = r.removeFromTop (38).withTrimmedTop (6);
    if (mode == Mode::scales)
    {
        root.setBounds (top.removeFromLeft (100).reduced (2));
        scale.setBounds (top.removeFromLeft (170).reduced (2));
        fingering.setBounds (top.removeFromLeft (250).reduced (2));
        pattern.setBounds (top.removeFromLeft (190).reduced (2));
    }
    else
    {
        pattern.setBounds (top.removeFromLeft (230).reduced (2));
        startFret.setBounds (top.removeFromLeft (140).reduced (2));
    }
    labels.setBounds (row2.removeFromLeft (150).reduced (2));
    notesPerBeat.setBounds (row2.removeFromLeft (150).reduced (2));
    row2.removeFromLeft (8);
    ladder.setBounds (row2.removeFromLeft (310));
    startButton.setBounds (row2.removeFromRight (120).reduced (2, 0));
    row2.removeFromRight (8);
    listenButton.setBounds (row2.removeFromRight (100).reduced (2, 0));
    r.removeFromTop (6);
    r.removeFromBottom (68);
    board.setBounds (r);
}

// ---------------------------------------------------------------- page

PracticePage::PracticePage (AudioEngine& e, Progress& p)
    : metronome (e), explorer (e, p), scales (e, p, PatternTrainer::Mode::scales), technique (e, p, PatternTrainer::Mode::technique)
{
    addAndMakeVisible (tuner);
    addAndMakeVisible (metronome);
    addAndMakeVisible (explorer);
    addChildComponent (scales);
    addChildComponent (technique);
    const char* names[] = { "Fretboard", "Scales", "Technique" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText (names[i]);
        tabButtons[i].setRadioGroupId (2);
        tabButtons[i].setClickingTogglesState (true);
        tabButtons[i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabButtons[i]);
    }
    showTab (fretboardTab);
}

void PracticePage::showTab (int tab)
{
    currentTab = tab;
    explorer.setVisible (tab == fretboardTab);
    scales.setVisible (tab == scalesTab);
    technique.setVisible (tab == techniqueTab);
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setToggleState (i == tab, juce::dontSendNotification);
}

void PracticePage::notePlayed (const NoteEvent& e)
{
    if (currentTab == fretboardTab) explorer.notePlayed (e);
    else if (currentTab == scalesTab) scales.notePlayed (e);
    else technique.notePlayed (e);
}

void PracticePage::pitchChanged (const PitchFrame& f)
{
    tuner.setPitch (f);
    if (currentTab == fretboardTab) explorer.pitchChanged (f);
    else if (currentTab == scalesTab) scales.pitchChanged (f);
    else technique.pitchChanged (f);
}

void PracticePage::paint (juce::Graphics&) {}

void PracticePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (196);
    tuner.setBounds (top.removeFromLeft (top.getWidth() / 2 - 6));
    top.removeFromLeft (12);
    metronome.setBounds (top);
    r.removeFromTop (8);
    auto tabs = r.removeFromTop (38);
    for (auto& b : tabButtons)
        b.setBounds (tabs.removeFromLeft (140).reduced (2, 2));
    r.removeFromTop (6);
    explorer.setBounds (r);
    scales.setBounds (r);
    technique.setBounds (r);
}
