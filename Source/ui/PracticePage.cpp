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

// ---------------------------------------------------------------- page

PracticePage::PracticePage (AudioEngine& e, Progress& p) : metronome (e), explorer (e, p)
{
    addAndMakeVisible (tuner);
    addAndMakeVisible (metronome);
    addAndMakeVisible (explorer);
}

void PracticePage::paint (juce::Graphics&) {}

void PracticePage::resized()
{
    auto r = getLocalBounds().reduced (12);
    auto top = r.removeFromTop (230);
    tuner.setBounds (top.removeFromLeft (top.getWidth() / 2 - 6));
    top.removeFromLeft (12);
    metronome.setBounds (top);
    r.removeFromTop (12);
    explorer.setBounds (r);
}
