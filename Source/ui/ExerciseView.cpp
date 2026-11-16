#include "ExerciseView.h"

ExerciseView::ExerciseView (AudioEngine& e, Progress& p) : engine (e), progress (p)
{
    addAndMakeVisible (backButton);
    addChildComponent (staff);
    addChildComponent (fretboard);
    addChildComponent (listenButton);
    addChildComponent (nextButton);
    addChildComponent (againButton);

    flatButton.setButtonText (theory::accidentalText (-1));
    naturalButton.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xae")));
    sharpButton.setButtonText (theory::accidentalText (1));
    int acc = -1;
    for (auto* b : { &flatButton, &naturalButton, &sharpButton })
    {
        addChildComponent (b);
        b->setClickingTogglesState (true);
        b->setRadioGroupId (77);
        b->onClick = [this, acc] { staff.setAccidental (acc); };
        ++acc;
    }

    backButton.onClick = [this] { stopTimer(); engine.stopNotes(); engine.getBacking().stop(); if (onBack) onBack(); };
    listenButton.onClick = [this] { playListen(); };
    nextButton.onClick = [this]
    {
        if (! done)
        {
            // "Show answer"
            done = true;
            missed = true;
            feedback = q.explanation;
            feedbackColour = Palette::gold;
            reveal (false);
            updateButtons();
            resized();
            repaint();
            return;
        }
        nextQuestion();
    };
    againButton.onClick = [this] { start (*lesson, exerciseIndex); };

    staff.onPlace = [this] (const theory::Note& n)
    {
        if (done || ! q.judgeStaff) return;
        if (! q.sequence.empty())
        {
            const auto& want = q.sequence[(size_t) seqPos];
            const bool right = q.sequenceAnyOctave ? (n.letter == want.letter && n.acc == want.acc) : n == want;
            sequenceAnswer (right, "You wrote " + n.name (false) + (n.letter == want.letter && n.acc == want.acc ? " in the wrong octave." : "."));
            return;
        }
        const bool ok = q.judgeStaff (n);
        q.staffNotes = { n };
        staff.setNotes (q.staffNotes, { ok ? Palette::green : Palette::red });
        attempt (ok, ok ? juce::String() : "You wrote " + n.name (false) + " (" + theory::noteFromMidi (n.midi()).name (true) + " sounding).", false);
    };

    fretboard.onClick = [this] (int s, int f)
    {
        engine.playNote (theory::fretMidi (s, f), 0.0, 1.2);
        if (done || ! q.judgeFret) return;
        if (! q.sequence.empty())
        {
            const int midi = theory::fretMidi (s, f);
            const bool right = sequenceMatches (midi);
            if (right)
                enteredMarks.push_back ({ s, f, q.sequence[(size_t) seqPos].name(), seqPos == 0 ? 1 : 2 });
            else
                wrongMarks.push_back ({ s, f, theory::noteFromMidi (midi).name(), 3 });
            auto marks = q.fretMarks;
            marks.insert (marks.end(), enteredMarks.begin(), enteredMarks.end());
            if (! right)
                marks.push_back (wrongMarks.back());
            fretboard.setMarks (marks);
            sequenceAnswer (right, "That is " + theory::noteFromMidi (midi).name (true) + ".");
            return;
        }
        const bool ok = q.judgeFret (s, f);
        if (! ok)
        {
            wrongMarks.push_back ({ s, f, theory::noteFromMidi (theory::fretMidi (s, f)).name(), 3 });
            auto marks = q.fretMarks;
            marks.insert (marks.end(), wrongMarks.begin(), wrongMarks.end());
            fretboard.setMarks (marks);
        }
        attempt (ok, ok ? juce::String() : "That is " + theory::noteFromMidi (theory::fretMidi (s, f)).name (true) + ".", false);
    };
}

void ExerciseView::start (const Lesson& l, int index, juce::int64 seed)
{
    lesson = &l;
    exerciseIndex = index;
    spec = l.exercises[(size_t) index];
    rng.setSeed (seed != 0 ? seed : juce::Time::currentTimeMillis());
    questionNumber = 0;
    score = 0;
    finished = false;
    nextQuestion();
}

void ExerciseView::nextQuestion()
{
    stopTimer();
    if (questionNumber >= questionsPerRound)
    {
        finish();
        return;
    }
    ++questionNumber;
    q = makeQuestion (spec, rng);
    showQuestion();
}

void ExerciseView::showQuestion()
{
    done = missed = false;
    feedback = {};
    wrongMarks.clear();
    enteredMarks.clear();
    seqPos = q.sequenceGiven;
    questionShownAt = juce::Time::getMillisecondCounter();

    const bool useStaff = ! q.staffNotes.empty() || q.staffClick || ! q.revealStaff.empty() || q.showKeySignature || ! q.sequence.empty();
    staff.setVisible (useStaff);
    staff.setKeySignature (q.showKeySignature ? q.keySignature : 0);
    if (q.staffChord)
        staff.setChords ({ q.staffNotes });
    else
        staff.setNotes (q.staffNotes);
    staff.setClickable (q.staffClick);
    staff.setAccidental (0);
    naturalButton.setToggleState (true, juce::dontSendNotification);
    for (auto* b : { &flatButton, &naturalButton, &sharpButton })
        b->setVisible (q.staffAccidentalButtons);

    fretboard.setVisible (q.showFretboard);
    fretboard.setMaxFret (q.maxFret);
    fretboard.setMarks (q.fretMarks);
    fretboard.setClickable (q.fretClick);
    fretboard.setLiveMidi (-1);

    choiceButtons.clear();
    for (int i = 0; i < q.choices.size(); ++i)
    {
        auto* b = choiceButtons.add (new juce::TextButton (q.choices[i]));
        addAndMakeVisible (b);
        b->onClick = [this, i]
        {
            if (! q.sequence.empty())
            {
                if (done) return;
                const bool right = q.choices[i] == q.sequence[(size_t) seqPos].name();
                if (! right)
                    choiceButtons[i]->setColour (juce::TextButton::buttonColourId, Palette::red.darker (0.2f));
                sequenceAnswer (right, "Not " + q.choices[i] + ".");
                return;
            }
            const bool ok = q.judgeChoice ? q.judgeChoice (i) : i == q.correctChoice;
            attempt (ok, {}, true, i);
        };
    }

    if (q.autoListen)
        playListen();

    updateButtons();
    resized();
    repaint();
}

void ExerciseView::updateButtons()
{
    nextButton.setVisible (! finished && (done || q.acceptsPlaying || q.fretClick || q.staffClick));
    nextButton.setButtonText (done ? (questionNumber >= questionsPerRound ? "Finish" : "Next") : "Show answer");
    againButton.setVisible (finished);
    const bool canListen = ! q.listenMidi.empty() || ! q.listenChords.empty() || q.listenBacking >= 0;
    listenButton.setVisible (! finished && canListen && (q.autoListen || done || q.listenBeforeAnswer));
    for (auto* b : choiceButtons)
        b->setVisible (! finished);
}

void ExerciseView::attempt (bool correct, const juce::String& hint, bool isChoice, int choice)
{
    if (done)
        return;

    if (correct)
    {
        done = true;
        if (! missed)
            ++score;
        feedback = (missed ? "Found it. " : "Right! ") + q.explanation;
        feedbackColour = Palette::green;
        reveal (true);
        if (! missed)
            startTimer (1600);   // straight on to the next question
    }
    else
    {
        missed = true;
        if (isChoice)
        {
            done = true;
            feedback = "Not quite. " + q.explanation;
            feedbackColour = Palette::red;
            if (choice >= 0 && choice < choiceButtons.size())
                choiceButtons[choice]->setColour (juce::TextButton::buttonColourId, Palette::red.darker (0.2f));
            reveal (false);
        }
        else
        {
            feedback = hint + " Try again, or press Show answer.";
            feedbackColour = Palette::red;
        }
    }
    updateButtons();
    resized();
    repaint();
}

void ExerciseView::reveal (bool correct)
{
    if (q.sequence.empty() && q.correctChoice >= 0 && q.correctChoice < choiceButtons.size())
        choiceButtons[q.correctChoice]->setColour (juce::TextButton::buttonColourId, Palette::green.darker (0.3f));
    if (! q.revealMarks.empty())
    {
        auto marks = q.revealMarks;
        if (! correct)
            marks.insert (marks.begin(), wrongMarks.begin(), wrongMarks.end());
        fretboard.setMarks (marks);
    }
    const auto shown = ! q.revealStaff.empty() ? q.revealStaff : q.sequence;
    if (! shown.empty())
    {
        staff.setVisible (true);
        if (q.staffChord)
        {
            juce::StringArray names;
            for (auto& n : shown) names.add (n.name());
            staff.setChords ({ shown }, { names.joinIntoString (" ") }, { Palette::green.darker (0.4f) });
        }
        else
        {
            staff.setNotes (shown, std::vector<juce::Colour> (shown.size(), Palette::green.darker (0.4f)), true);
            staff.setStepLabels (q.revealStepLabels);
        }
        staff.setClickable (false);
    }
    if (! q.revealMarks.empty())
        fretboard.setVisible (true);
}

void ExerciseView::timerCallback()
{
    stopTimer();
    nextQuestion();
}

void ExerciseView::finish()
{
    finished = true;
    progress.recordExercise (lesson->exerciseKey (exerciseIndex), score, questionsPerRound);
    staff.setVisible (false);
    fretboard.setVisible (false);
    for (auto* b : { &flatButton, &naturalButton, &sharpButton })
        b->setVisible (false);
    const bool pass = (double) score / questionsPerRound >= Progress::passMark;
    feedback = pass ? "Passed! Come back tomorrow to keep it fresh, or try again for a perfect round."
                    : "Keep at it: 8 out of 10 passes. Read the lesson again or slow down and try again.";
    feedbackColour = pass ? Palette::green : Palette::gold;
    updateButtons();
    resized();
    repaint();
    if (onFinished)
        onFinished();
}

void ExerciseView::notePlayed (const NoteEvent& e)
{
    if (finished || done || ! q.acceptsPlaying || ! q.judgePlayed)
        return;
    if (juce::Time::getMillisecondCounter() - questionShownAt < 500)
        return;   // ignore the last note still ringing from the previous question
    if (q.ignorePlayed && q.ignorePlayed (e.midi))
        return;
    if (! q.sequence.empty())
    {
        sequenceAnswer (sequenceMatches (e.midi), "You played " + theory::noteFromMidi (e.midi).name (true) + ".");
        return;
    }
    const bool ok = q.judgePlayed (e.midi);
    attempt (ok, ok ? juce::String() : q.wrongPlayHint (e.midi), false);
}

void ExerciseView::pitchChanged (const PitchFrame& f)
{
    if (fretboard.isVisible())
        fretboard.setLiveMidi (f.valid ? juce::roundToInt (f.midi) : -1);
}

void ExerciseView::playListen()
{
    engine.stopNotes();
    if (q.listenBacking >= 0)
    {
        // two bars of the backing band, no count-in
        BackingSettings b;
        b.keyPc = q.listenBackingKey;
        b.feel = q.listenBacking;
        b.bpm = q.listenBackingBpm;
        b.countIn = false;
        b.stopAfterBars = 2;
        engine.getBacking().setSettings (b);
        engine.getBacking().start();
        return;
    }
    if (! q.listenChords.empty())
    {
        for (size_t c = 0; c < q.listenChords.size(); ++c)
            for (int m : q.listenChords[c])
                engine.playNote (m, q.listenGap * (double) c, q.listenGap * 0.95, 0.55f);
        return;
    }
    engine.playSequence (q.listenMidi, q.listenGap, juce::jmax (0.6, q.listenGap * 1.6));
    if (q.listenAsChord)
    {
        const double at = q.listenGap * (double) q.listenMidi.size() + 0.3;
        for (int m : q.listenMidi)
            engine.playNote (m, at, 2.2, 0.6f);
    }
}

bool ExerciseView::sequenceMatches (int midi) const
{
    const auto& want = q.sequence[(size_t) seqPos];
    return q.sequenceAnyOctave ? theory::mod12 (midi) == want.pc() : midi == want.midi();
}

void ExerciseView::showSequenceSoFar()
{
    if (! staff.isVisible())
        return;
    std::vector<theory::Note> notes (q.sequence.begin(), q.sequence.begin() + seqPos);
    std::vector<juce::Colour> cols;
    for (int i = 0; i < seqPos; ++i)
        cols.push_back (i < q.sequenceGiven ? Palette::ink : Palette::green.darker (0.4f));
    staff.setNotes (notes, cols);
}

void ExerciseView::sequenceAnswer (bool ok, const juce::String& wrongText)
{
    if (done || finished)
        return;
    if (! ok)
    {
        missed = true;
        feedback = wrongText + " Try again, or press Show answer.";
        feedbackColour = Palette::red;
        repaint();
        return;
    }
    ++seqPos;
    showSequenceSoFar();
    if (seqPos >= (int) q.sequence.size())
    {
        attempt (true, {}, false);
        return;
    }
    feedback = q.sequence[(size_t) seqPos - 1].name() + juce::String (juce::CharPointer_UTF8 (" \xe2\x9c\x93   Next note?"));
    feedbackColour = Palette::green;
    repaint();
}

void ExerciseView::simulateChoice (bool correct)
{
    if (q.choices.isEmpty())
        return;
    int i = q.correctChoice;
    if (! correct)
        i = (q.correctChoice + 1) % q.choices.size();
    attempt (correct, {}, true, i);
    stopTimer();
}

void ExerciseView::simulateSequence (int notes)
{
    for (int i = 0; i < notes && ! done && seqPos < (int) q.sequence.size(); ++i)
        sequenceAnswer (true, {});
    stopTimer();
}

void ExerciseView::simulateFinish (int s)
{
    questionNumber = questionsPerRound;
    score = s;
    finish();
}

void ExerciseView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().reduced (16, 10);
    auto top = r.removeFromTop (40);
    top.removeFromLeft (110);
    g.setColour (Palette::gold);
    g.setFont (Fonts::ui (22.0f, true));
    g.drawText (spec.title, top.removeFromLeft (top.getWidth() - 360), juce::Justification::centredLeft);
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (16.0f));
    g.drawText (finished ? "Done" : "Question " + juce::String (questionNumber) + " of " + juce::String (questionsPerRound)
                    + juce::String::fromUTF8 ("   ·   score ") + juce::String (score)
                    + (! q.sequence.empty() && ! done ? juce::String::fromUTF8 ("   ·   note ") + juce::String (seqPos + 1) + " of " + juce::String ((int) q.sequence.size()) : juce::String()),
                top, juce::Justification::centredRight);

    // score dots
    auto dots = r.removeFromTop (14);
    for (int i = 0; i < questionsPerRound; ++i)
    {
        auto c = i < questionNumber - (done || finished ? 0 : 1) ? Palette::gold : Palette::panelLight;
        g.setColour (c);
        g.fillEllipse ((float) dots.getX() + i * 18.0f, (float) dots.getY() + 2, 10.0f, 10.0f);
    }

    r.removeFromTop (6);
    g.setColour (Palette::text);
    if (finished)
    {
        g.setFont (Fonts::ui (44.0f, true));
        g.drawText (juce::String (score) + " / " + juce::String (questionsPerRound), r.removeFromTop (110), juce::Justification::centred);
        g.setFont (Fonts::ui (19.0f));
        g.setColour (feedbackColour);
        g.drawFittedText (feedback, r.removeFromTop (60), juce::Justification::centred, 2);
        const double best = progress.bestScore (lesson->exerciseKey (exerciseIndex));
        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (16.0f));
        g.drawText ("Best: " + juce::String (juce::roundToInt (best * 100)) + "%", r.removeFromTop (30), juce::Justification::centred);
        return;
    }

    g.setFont (Fonts::ui (20.0f));
    g.drawFittedText (q.prompt, r.removeFromTop (34), juce::Justification::centredLeft, 1);

    if (q.acceptsPlaying)
    {
        g.setColour (Palette::blue);
        g.setFont (Fonts::ui (14.0f));
        g.drawText (juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaa")) + " Listening to your guitar", getLocalBounds().reduced (16, 10).removeFromTop (40).withTrimmedLeft (110).translated (0, 64).removeFromRight (240),
                    juce::Justification::centredRight);
    }

    if (feedback.isNotEmpty())
    {
        auto fb = getLocalBounds().reduced (16, 10).removeFromBottom (96).removeFromTop (44);
        g.setColour (feedbackColour.withAlpha (0.15f));
        g.fillRoundedRectangle (fb.toFloat(), 6.0f);
        g.setColour (feedbackColour.brighter (0.3f));
        g.setFont (Fonts::ui (17.0f));
        g.drawFittedText (feedback, fb.reduced (10, 2), juce::Justification::centredLeft, 2);
    }
}

void ExerciseView::resized()
{
    auto r = getLocalBounds().reduced (16, 10);
    backButton.setBounds (r.getX(), r.getY() + 4, 100, 32);
    r.removeFromTop (40 + 14 + 6 + 34 + 8);

    auto bottom = r.removeFromBottom (96);
    bottom.removeFromTop (52);
    nextButton.setBounds (bottom.removeFromRight (150).reduced (0, 2));
    bottom.removeFromRight (10);
    listenButton.setBounds (bottom.removeFromRight (120).reduced (0, 2));
    againButton.setBounds (getLocalBounds().withSizeKeepingCentre (180, 40).translated (0, 90));

    if (finished)
        return;

    // choices at the bottom of the work area
    if (choiceButtons.size() > 0)
    {
        const int n = choiceButtons.size();
        const int cols = q.choiceColumns > 0 ? juce::jmin (q.choiceColumns, n) : juce::jmin (n, 7);
        const int rows = (n + cols - 1) / cols;
        auto area = r.removeFromBottom (rows * 46);
        const int w = juce::jmin (150, area.getWidth() / cols);
        for (int i = 0; i < n; ++i)
        {
            const int row = i / cols, col = i % cols;
            const int inRow = juce::jmin (cols, n - row * cols);
            const int x0 = area.getX() + (area.getWidth() - inRow * w) / 2;
            choiceButtons[i]->setBounds (juce::Rectangle<int> (x0 + col * w, area.getY() + row * 46, w, 46).reduced (3));
        }
        r.removeFromBottom (8);
    }

    const bool s = staff.isVisible(), f = fretboard.isVisible();
    if (s && f)
    {
        auto st = r.removeFromTop (juce::jmin (190, r.getHeight() / 2));
        staff.setBounds (st.withSizeKeepingCentre (juce::jmin (st.getWidth(), 560), st.getHeight()));
        r.removeFromTop (8);
        fretboard.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 230)));
    }
    else if (s)
    {
        auto st = r;
        if (q.staffAccidentalButtons)
        {
            auto acc = st.removeFromBottom (44).withSizeKeepingCentre (3 * 64, 40);
            flatButton.setBounds (acc.removeFromLeft (64).reduced (3));
            naturalButton.setBounds (acc.removeFromLeft (64).reduced (3));
            sharpButton.setBounds (acc.removeFromLeft (64).reduced (3));
        }
        staff.setBounds (st.withSizeKeepingCentre (juce::jmin (st.getWidth(), 560), juce::jmin (st.getHeight(), 230)));
    }
    else if (f)
        fretboard.setBounds (r.withSizeKeepingCentre (r.getWidth(), juce::jmin (r.getHeight(), 240)));
}
