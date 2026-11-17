#include "ImprovPage.h"

using namespace theory;

namespace
{
    const juce::String dot = juce::String::fromUTF8 ("   \xc2\xb7   ");

    juce::String degreeOf (int semis)
    {
        static const char* labels[12] = { "R", "b2", "2", "b3", "3", "4", "b5", "5", "b6", "6", "b7", "7" };
        return labels[mod12 (semis)];
    }

    // the usual spelling of a blues key: flats for D-flat, E-flat, A-flat, B-flat
    Note keyNote (int pc) { return noteFromMidi (48 + mod12 (pc), pc == 1 || pc == 3 || pc == 8 || pc == 10); }

    void addKeys (juce::ComboBox& box, int selectedPc)
    {
        for (int pc : { 9, 4, 7, 0, 2, 5, 10, 3, 8, 1, 6, 11 })
            box.addItem ("Key of " + keyNote (pc).name(), pc + 1);
        box.setSelectedId (selectedPc + 1, juce::dontSendNotification);
    }
}

// ---------------------------------------------------------------- tab

void TabView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::cream);
    g.fillRoundedRectangle (r, 8.0f);
    const float top = r.getY() + 18.0f, gap = (r.getHeight() - 30.0f) / 5.0f;
    g.setColour (Palette::ink.withAlpha (0.55f));
    for (int s = 0; s < 6; ++s)
        g.fillRect (8.0f, top + gap * (float) s - 0.5f, r.getWidth() - 16.0f, 1.0f);
    g.setColour (Palette::ink);
    g.setFont (Fonts::ui (gap * 1.1f, true));
    const char* tabLetters[3] = { "T", "A", "B" };
    for (int i = 0; i < 3; ++i)
        g.drawText (tabLetters[i], juce::Rectangle<float> (14.0f, top + gap * (1.0f + (float) i * 1.1f) - gap * 0.6f, gap * 1.4f, gap * 1.2f), juce::Justification::centred);

    if (! xForBeat)
        return;
    for (auto& e : entries)
    {
        const float x = xForBeat (e.beat), y = top + gap * (float) (e.string - 1);
        g.setFont (Fonts::ui (juce::jmin (16.0f, gap * 1.15f), true));
        const float w = (float) e.text.length() * gap * 0.62f + 6.0f;
        g.setColour (Palette::cream);
        g.fillRect (x - w * 0.5f, y - gap * 0.55f, w, gap * 1.1f);
        g.setColour (e.colour);
        g.drawText (e.text, juce::Rectangle<float> (x - w, y - gap * 0.7f, w * 2, gap * 1.4f), juce::Justification::centred, false);
        if (e.above.isNotEmpty())
        {
            g.setColour (Palette::ink.withAlpha (0.7f));
            g.setFont (Fonts::ui (12.0f, true));
            g.drawText (e.above, juce::Rectangle<float> (x - 20, 1.0f, 40, 14), juce::Justification::centred);
        }
    }
}

// ---------------------------------------------------------------- backing tracks

BackingPanel::BackingPanel (AudioEngine& e) : engine (e)
{
    addKeys (key, 9);
    int id = 1;
    for (auto& f : bluesForms())
        form.addItem (f.name, id++);
    form.setSelectedId (1, juce::dontSendNotification);
    feel.addItemList (feelNames(), 1);
    feel.setSelectedId (1, juce::dontSendNotification);
    scaleMap.addItemList ({ "Show: minor pentatonic", "Show: blues scale", "Show: major pentatonic", "Show: Mixolydian of each chord", "Show: chord tones of each chord" }, 1);
    scaleMap.setSelectedId (1, juce::dontSendNotification);

    tempo.setSliderStyle (juce::Slider::LinearHorizontal);
    tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
    tempo.setRange (40.0, 200.0, 1.0);
    tempo.setValue (90.0, juce::dontSendNotification);
    tempo.setTextValueSuffix (" BPM");
    volume.setSliderStyle (juce::Slider::LinearHorizontal);
    volume.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    volume.setRange (0.0, 1.0, 0.01);
    volume.setValue (0.8, juce::dontSendNotification);

    for (auto* c : { &key, &form, &feel })
        c->onChange = [this] { push(); updateMarks (shownBar); };
    scaleMap.onChange = [this] { updateMarks (shownBar); };
    tempo.onValueChange = [this] { push(); };
    volume.onValueChange = [this] { push(); };
    for (auto* t : { &drums, &bass, &guitar, &keys })
    {
        t->setToggleState (true, juce::dontSendNotification);
        t->onClick = [this] { push(); };
    }
    playButton.onClick = [this]
    {
        if (engine.getBacking().isPlaying())
            engine.getBacking().stop();
        else
        {
            push();
            engine.getBacking().start();
        }
    };

    for (juce::Component* c : { (juce::Component*) &key, (juce::Component*) &form, (juce::Component*) &feel, (juce::Component*) &scaleMap,
                                (juce::Component*) &tempo, (juce::Component*) &volume, (juce::Component*) &drums, (juce::Component*) &bass,
                                (juce::Component*) &guitar, (juce::Component*) &keys, (juce::Component*) &playButton, (juce::Component*) &chart,
                                (juce::Component*) &board })
        addAndMakeVisible (c);
    board.setMaxFret (15);
    board.setClickable (true);
    board.onClick = [this] (int s, int f) { engine.playNote (fretMidi (s, f), 0.0, 1.2); };
    updateMarks (-1);
    startTimerHz (20);
}

BackingPanel::~BackingPanel() { stopTimer(); }

BackingSettings BackingPanel::current() const
{
    BackingSettings s;
    s.keyPc = key.getSelectedId() - 1;
    s.form = form.getSelectedId() - 1;
    s.feel = feel.getSelectedId() - 1;
    s.bpm = tempo.getValue();
    s.volume = (float) volume.getValue();
    s.drums = drums.getToggleState();
    s.bass = bass.getToggleState();
    s.guitar = guitar.getToggleState();
    s.keys = keys.getToggleState();
    return s;
}

void BackingPanel::push()
{
    engine.getBacking().setSettings (current());
    chart.setKey (key.getSelectedId() - 1);
    chart.setForm (form.getSelectedId() - 1);
}

void BackingPanel::showBar (int bar)
{
    chart.setCurrentBar (bar);
    updateMarks (bar);
    shownBar = bar;
}

void BackingPanel::updateMarks (int bar)
{
    const int keyPc = key.getSelectedId() - 1;
    const auto& f = bluesForms()[(size_t) juce::jlimit (0, (int) bluesForms().size() - 1, form.getSelectedId() - 1)];
    const auto& chord = f.bars[(size_t) juce::jlimit (0, 11, bar < 0 ? 0 : bar)][0];
    const int chordRoot = mod12 (keyPc + chord.offset);
    const int mode = scaleMap.getSelectedId();

    std::vector<int> semis;      // from `base`
    int base = keyPc;
    switch (mode)
    {
        case 1: semis = { 0, 3, 5, 7, 10 }; break;
        case 2: semis = { 0, 3, 5, 6, 7, 10 }; break;
        case 3: semis = { 0, 2, 4, 7, 9 }; break;
        case 4: semis = { 0, 2, 4, 5, 7, 9, 10 }; base = chordRoot; break;
        default: semis = bluesChordTones (chord); base = chordRoot; break;
    }
    const auto tones = bluesChordTones (chord);
    std::vector<FretMark> marks;
    for (int s = 1; s <= 6; ++s)
        for (int fr = 0; fr <= 15; ++fr)
        {
            const int midi = fretMidi (s, fr);
            const int fromBase = mod12 (midi - base);
            if (std::find (semis.begin(), semis.end(), fromBase) == semis.end())
                continue;
            // the 3rd and 7th of the chord that is playing light up green
            const int fromChord = mod12 (midi - chordRoot);
            const bool guide = fromChord == tones[1] || fromChord == tones[3];
            marks.push_back ({ s, fr, degreeOf (fromBase), fromBase == 0 ? 1 : (bar >= 0 && guide ? 2 : 0) });
        }
    board.setMarks (marks);
}

void BackingPanel::timerCallback()
{
    auto& b = engine.getBacking();
    playButton.setButtonText (b.isPlaying() ? "Stop" : "Play");
    playButton.setToggleState (b.isPlaying(), juce::dontSendNotification);
    const int bar = b.isPlaying() ? b.getBar() : -1;
    if (bar != shownBar)
        showBar (bar);
}

void BackingPanel::pitchChanged (const PitchFrame& f) { board.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1); }

void BackingPanel::paint (juce::Graphics& g)
{
    drawPanel (g, getLocalBounds().toFloat());
    auto inner = getLocalBounds().reduced (16, 10);
    drawHeading (g, "Backing tracks", inner.removeFromTop (30), 19.0f);
    const auto& f = bluesForms()[(size_t) juce::jlimit (0, (int) bluesForms().size() - 1, form.getSelectedId() - 1)];
    const int keyPc = key.getSelectedId() - 1;
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    auto tips = juce::Rectangle<int> (chart.getRight() + 16, chart.getY(), getWidth() - chart.getRight() - 32, chart.getHeight());
    g.drawFittedText ("Start simple: play only the root of each chord, on beat 1, as the chart lights up. Then improvise with the scale on the neck. "
                      "Green dots are the 3rd and 7th of the chord that is playing: land on them when the chord changes.\n\n"
                      "Minor pentatonic in " + keyNote (keyPc).name() + ": box 1 starts at fret " + juce::String (mod12 (keyPc - 4)) + " on the 6th string."
                      + (f.minor ? "\n\nMinor blues: natural minor and minor pentatonic both fit." : juce::String()),
                      tips, juce::Justification::topLeft, 12, 1.0f);
}

void BackingPanel::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (30);
    top.removeFromLeft (150);
    key.setBounds (top.removeFromLeft (130).reduced (2));
    form.setBounds (top.removeFromLeft (230).reduced (2));
    feel.setBounds (top.removeFromLeft (180).reduced (2));
    playButton.setBounds (top.removeFromRight (120).reduced (2, 0));
    r.removeFromTop (6);
    auto row = r.removeFromTop (32);
    tempo.setBounds (row.removeFromLeft (300).reduced (2));
    row.removeFromLeft (10);
    for (auto* t : { &drums, &bass, &guitar, &keys })
        t->setBounds (row.removeFromLeft (90));
    row.removeFromLeft (10);
    volume.setBounds (row.removeFromLeft (160).reduced (2, 6));
    r.removeFromTop (8);
    auto boardArea = r.removeFromBottom (juce::jmin (230, r.getHeight() / 2));
    scaleMap.setBounds (boardArea.removeFromTop (30).removeFromLeft (300).reduced (2));
    boardArea.removeFromTop (4);
    board.setBounds (boardArea);
    r.removeFromBottom (8);
    chart.setBounds (r.removeFromLeft (juce::jmin (r.getWidth() * 3 / 5, 720)));
}

// ---------------------------------------------------------------- licks

LickPanel::LickPanel (AudioEngine& e, Progress& p) : engine (e), progress (p), licks (loadLicks())
{
    for (int i = 0; i < (int) licks.size(); ++i)
    {
        auto* b = lickButtons.add (new juce::TextButton());
        b->onClick = [this, i] { selectLick (i); };
        addAndMakeVisible (b);
    }
    addKeys (key, 9);
    key.onChange = [this] { rebuild(); };
    tempo.setSliderStyle (juce::Slider::LinearHorizontal);
    tempo.setTextBoxStyle (juce::Slider::TextBoxRight, false, 70, 26);
    tempo.setRange (40.0, 180.0, 1.0);
    tempo.setValue (70.0, juce::dontSendNotification);
    tempo.setTextValueSuffix (" BPM");
    listenButton.onClick = [this]
    {
        engine.stopNotes();
        const double beat = 60.0 / tempo.getValue();
        for (auto& n : lick.notes)
        {
            const double at = swingBeat (n.beat, swung()) * beat;
            const double len = juce::jmax (0.15, n.length * beat * 0.95);
            if (n.tech == "b")
                engine.playBentNote (n.fretMidi(), at, len, (float) n.bend, juce::jmin (0.08, len * 0.3));
            else if (n.tech == "r")
                engine.playBentNote (n.fretMidi() + n.bend, at, len, (float) -n.bend, 0.03);
            else
                engine.playNote (n.fretMidi(), at, len, n.tech == "h" || n.tech == "p" ? 0.55f : 0.8f);
        }
    };
    turnButton.onClick = [this]
    {
        if (running)
        {
            running = false;
            stopTimer();
            engine.getBacking().stop();
            turnButton.setButtonText ("Your turn");
            updateMarks();
            return;
        }
        startTurn();
    };
    for (juce::Component* c : { (juce::Component*) &key, (juce::Component*) &tempo, (juce::Component*) &withBacking, (juce::Component*) &listenButton,
                                (juce::Component*) &turnButton, (juce::Component*) &staff, (juce::Component*) &tab, (juce::Component*) &board })
        addAndMakeVisible (c);
    tab.xForBeat = [this] (double b) { return staff.headXForBeat (b); };
    board.setMaxFret (17);
    board.setClickable (true);
    board.onClick = [this] (int s, int f) { engine.playNote (fretMidi (s, f), 0.0, 1.2); };
    selectLick (0);
}

juce::String LickPanel::degree (int midi) const { return degreeOf (midi - keyPc()); }

Note LickPanel::spell (int midi) const
{
    // spelled from the key: the blues scale and the major pentatonic, other notes with sharps
    const auto root = keyNote (keyPc());
    for (auto* id : { "blues", "majorPentatonic" })
        for (auto& n : scaleNotes (root, scaleType (id), false))
            if (n.pc() == mod12 (midi))
            {
                Note s = n;
                s.octave = (midi - naturalPc[s.letter] - s.acc) / 12 - 1;
                return s;
            }
    return noteFromMidi (midi);
}

void LickPanel::selectLick (int index)
{
    current = juce::jlimit (0, (int) licks.size() - 1, index);
    running = false;
    stopTimer();
    turnButton.setButtonText ("Your turn");
    result.clear();
    rebuild();
    refreshList();
}

void LickPanel::selectLickById (const juce::String& id)
{
    for (int i = 0; i < (int) licks.size(); ++i)
        if (licks[(size_t) i].id == id)
            selectLick (i);
}

void LickPanel::refreshList()
{
    for (int i = 0; i < lickButtons.size(); ++i)
    {
        const auto& l = licks[(size_t) i];
        const double best = progress.bestScore ("lick/" + l.id);
        lickButtons[i]->setButtonText (l.name + (best >= Progress::passMark ? juce::String (juce::CharPointer_UTF8 ("  \xe2\x9c\x93")) : juce::String()));
        lickButtons[i]->setToggleState (i == current, juce::dontSendNotification);
    }
}

void LickPanel::rebuild()
{
    lick = transposeLick (licks[(size_t) current], keyPc());
    std::vector<StaffView::RhythmNote> rn;
    std::vector<TabView::Entry> te;
    for (auto& n : lick.notes)
    {
        juce::String above = n.tech == "b" ? (n.bend == 1 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xbd")) : juce::String ("full"))
                           : n.tech == "h" ? "H" : n.tech == "p" ? "P" : n.tech == "s" ? "sl." : n.tech == "v" ? "~" : n.tech == "r" ? "rel." : "";
        rn.push_back ({ spell (n.fretMidi()), n.beat, n.length, above });
        juce::String text (n.fret);
        if (n.tech == "b") text << "b" << (n.fret + n.bend);
        if (n.tech == "r") text = "r" + juce::String (n.fret);
        if (n.tech == "v") text << "~";
        te.push_back ({ n.string, n.beat, text, {} });
    }
    staff.setRhythm (rn, lick.beats);
    tab.setEntries (te);
    updateMarks();
    repaint();
}

void LickPanel::updateMarks()
{
    std::vector<FretMark> marks;
    for (int i = 0; i < (int) lick.notes.size(); ++i)
    {
        const auto& n = lick.notes[(size_t) i];
        bool seen = false;
        for (auto& m : marks) seen = seen || (m.string == n.string && m.fret == n.fret);
        if (! seen)
            marks.push_back ({ n.string, n.fret, degree (n.fretMidi()), mod12 (n.fretMidi() - keyPc()) == 0 ? 1 : 0 });
        if (n.tech == "b")
            marks.push_back ({ n.string, n.fret + n.bend, degree (n.targetMidi()), 4 });   // where the bend goes
    }
    if (running && ! follower.finished())
    {
        const auto& n = lick.notes[(size_t) follower.position()];
        marks.push_back ({ n.string, n.fret, degree (n.fretMidi()), 4 });
    }
    board.setMarks (marks);
}

void LickPanel::startTurn()
{
    follower.reset (lick);
    running = true;
    result.clear();
    turnButton.setButtonText ("Stop");
    if (withBacking.getToggleState())
    {
        BackingSettings b;
        b.keyPc = keyPc();
        b.feel = lick.feel == "slow" ? 2 : lick.feel == "straight" ? 1 : 0;
        b.bpm = tempo.getValue();
        engine.getBacking().setSettings (b);
        engine.getBacking().start();
    }
    updateMarks();
    startTimerHz (10);
}

void LickPanel::finishTurn()
{
    running = false;
    stopTimer();
    turnButton.setButtonText ("Your turn");
    std::vector<double> written;
    for (auto& n : lick.notes)
        written.push_back (swingBeat (n.beat, swung()));
    const int notes = (int) lick.notes.size();
    const double accuracy = (double) notes / (double) (notes + follower.mistakes());
    const double rhythm = rhythmScore (written, follower.times());
    const double score = 0.6 * accuracy + 0.4 * rhythm;
    const double before = progress.bestScore ("lick/" + licks[(size_t) current].id);
    progress.recordExercise ("lick/" + licks[(size_t) current].id, juce::roundToInt (score * 20), 20);
    result = juce::String (juce::roundToInt (accuracy * 100)) + "% right notes (" + juce::String (follower.mistakes()) + " wrong)" + dot
           + "rhythm " + juce::String (juce::roundToInt (rhythm * 100)) + "%" + dot + "score " + juce::String (juce::roundToInt (score * 100)) + "%";
    resultColour = score >= Progress::passMark ? Palette::green : Palette::gold;
    result << (score >= Progress::passMark ? (before < Progress::passMark ? ". Learned! Now try it in another key." : ". Nice.")
                                           : ". Listen again, slow down, and copy the rhythm as well as the notes.");
    refreshList();
    updateMarks();
    repaint();
}

void LickPanel::notePlayed (const NoteEvent& e)
{
    if (! running)
        return;
    const auto r = follower.feed (e.midi, e.time);
    if (r == LickFollower::Result::finished)
        finishTurn();
    else if (r != LickFollower::Result::ignored)
        updateMarks();
    repaint();
}

void LickPanel::simulateTurn (int wrongNotes)
{
    startTurn();
    const double beat = 60.0 / tempo.getValue();
    int wrongLeft = wrongNotes;
    for (auto& n : lick.notes)
    {
        if (wrongLeft-- > 0)
            follower.feed (n.targetMidi() + 5, 0.0);
        if (n.tech == "b")
            follower.feed (n.fretMidi(), swingBeat (n.beat, swung()) * beat);
        follower.feed (n.targetMidi(), swingBeat (n.beat, swung()) * beat + 0.02);
    }
    finishTurn();
}

void LickPanel::pitchChanged (const PitchFrame& f) { board.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1); }

void LickPanel::paint (juce::Graphics& g)
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (290);
    drawPanel (g, left.reduced (0, 0).toFloat());
    drawHeading (g, "Licks", left.reduced (16, 10).removeFromTop (30), 19.0f);

    auto right = r.withTrimmedLeft (14);
    drawPanel (g, right.toFloat());
    auto inner = right.reduced (16, 10);
    drawHeading (g, licks[(size_t) current].name, inner.removeFromTop (30), 22.0f);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (15.0f));
    const auto& l = licks[(size_t) current];
    g.drawFittedText (l.description + "   (" + scaleType (l.scale).name + ", " + (l.feel == "slow" ? juce::String ("slow 12/8") : l.feel) + ")",
                      inner.removeFromTop (44), juce::Justification::topLeft, 2, 1.0f);

    auto status = juce::Rectangle<int> (inner.getX(), board.getBottom() + 4, inner.getWidth(), getHeight() - board.getBottom() - 8);
    g.setFont (Fonts::ui (17.0f));
    if (running)
    {
        g.setColour (Palette::blue);
        const int pos = follower.position();
        const auto& n = lick.notes[(size_t) juce::jmin (pos, (int) lick.notes.size() - 1)];
        g.drawText ("Play it: next " + spell (n.fretMidi()).name (true) + " (" + stringName (n.string) + ", fret " + juce::String (n.fret) + ")"
                        + (n.tech == "b" ? juce::String (" bent up to ") + spell (n.targetMidi()).name() : juce::String()) + dot
                        + "note " + juce::String (pos + 1) + " of " + juce::String ((int) lick.notes.size()) + dot + juce::String (follower.mistakes()) + " wrong",
                    status, juce::Justification::centredLeft);
    }
    else if (result.isNotEmpty())
    {
        g.setColour (resultColour);
        g.drawFittedText (result, status, juce::Justification::centredLeft, 2);
    }
    else
    {
        g.setColour (Palette::dim);
        const double best = progress.bestScore ("lick/" + l.id);
        g.drawText ("Listen first, then press Your turn and play it. Bends: push until you reach the blue dot's pitch."
                        + (best >= 0 ? dot + "best " + juce::String (juce::roundToInt (best * 100)) + "%" : juce::String()),
                    status, juce::Justification::centredLeft);
    }
}

void LickPanel::resized()
{
    auto r = getLocalBounds();
    auto left = r.removeFromLeft (290).reduced (12, 10);
    left.removeFromTop (36);
    const int rowH = juce::jlimit (30, 44, left.getHeight() / juce::jmax (1, lickButtons.size()));
    for (auto* b : lickButtons)
        b->setBounds (left.removeFromTop (rowH).reduced (2, 2));

    auto inner = r.withTrimmedLeft (14).reduced (16, 10);
    inner.removeFromTop (30 + 44 + 4);
    auto controls = inner.removeFromTop (32);
    key.setBounds (controls.removeFromLeft (130).reduced (2));
    turnButton.setBounds (controls.removeFromRight (120).reduced (2, 0));
    controls.removeFromRight (8);
    listenButton.setBounds (controls.removeFromRight (100).reduced (2, 0));
    withBacking.setBounds (controls.removeFromRight (190));
    tempo.setBounds (controls.removeFromLeft (juce::jmin (260, controls.getWidth())).reduced (2));
    inner.removeFromTop (8);
    inner.removeFromBottom (40);
    const int h = inner.getHeight();
    staff.setBounds (inner.removeFromTop (juce::jmin (170, h * 32 / 100)));
    tab.setBounds (inner.removeFromTop (juce::jmin (110, h * 21 / 100)).withTrimmedTop (2));
    inner.removeFromTop (8);
    board.setBounds (inner);
}

// ---------------------------------------------------------------- page

ImprovPage::ImprovPage (AudioEngine& e, Progress& p, const std::vector<Lesson>& bluesLessons)
    : lessons (e, p, bluesLessons), backing (e), licksPanel (e, p)
{
    addChildComponent (lessons);
    addChildComponent (backing);
    addChildComponent (licksPanel);
    lessons.onProgressChanged = [this] { if (onProgressChanged) onProgressChanged(); };
    const char* names[] = { "Blues lessons", "Backing tracks", "Licks" };
    for (int i = 0; i < 3; ++i)
    {
        tabButtons[i].setButtonText (names[i]);
        tabButtons[i].setRadioGroupId (3);
        tabButtons[i].setClickingTogglesState (true);
        tabButtons[i].onClick = [this, i] { showTab (i); };
        addAndMakeVisible (tabButtons[i]);
    }
    showTab (lessonsTab);
}

void ImprovPage::showTab (int t)
{
    currentTab = t;
    lessons.setVisible (t == lessonsTab);
    backing.setVisible (t == backingTab);
    licksPanel.setVisible (t == licksTab);
    for (int i = 0; i < 3; ++i)
        tabButtons[i].setToggleState (i == t, juce::dontSendNotification);
}

void ImprovPage::notePlayed (const NoteEvent& e)
{
    if (currentTab == lessonsTab) lessons.notePlayed (e);
    else if (currentTab == licksTab) licksPanel.notePlayed (e);
}

void ImprovPage::pitchChanged (const PitchFrame& f)
{
    if (currentTab == lessonsTab) lessons.pitchChanged (f);
    else if (currentTab == backingTab) backing.pitchChanged (f);
    else licksPanel.pitchChanged (f);
}

void ImprovPage::resized()
{
    auto r = getLocalBounds();
    auto tabs = r.removeFromTop (50).reduced (12, 6);
    for (auto& b : tabButtons)
        b.setBounds (tabs.removeFromLeft (160).reduced (2, 2));
    lessons.setBounds (r);
    backing.setBounds (r.reduced (12, 0).withTrimmedBottom (12));
    licksPanel.setBounds (r.reduced (12, 0).withTrimmedBottom (12));
}
