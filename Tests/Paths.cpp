// GuitarCoachPaths: walks every user path through the real UI, without an audio device or a window on screen.
// Buttons are pressed only when a user could press them (visible, enabled, on screen), answers go in through the same
// callbacks a click, a written note or a played note would use, and after every step the layout is checked.
// Prints each problem found and exits with the number of failed checks.
#include "ui/MainComponent.h"
#include <set>

static int checks = 0, failures = 0;
static std::set<std::string> reported;

static void fail (const juce::String& what)
{
    ++failures;
    if (reported.insert (what.toStdString()).second && reported.size() < 400)
        std::printf ("FAIL  %s\n", what.toRawUTF8());
}

#define EXPECT(cond, ...) do { ++checks; if (! (cond)) fail (juce::String::formatted (__VA_ARGS__)); } while (0)

// ---------------------------------------------------------------- layout

static bool insideViewport (juce::Component& c)
{
    return dynamic_cast<juce::Viewport*> (&c) != nullptr || c.findParentComponentOfClass<juce::Viewport>() != nullptr;
}

static bool interactive (juce::Component* c)
{
    return dynamic_cast<juce::Button*> (c) != nullptr || dynamic_cast<juce::ComboBox*> (c) != nullptr || dynamic_cast<juce::Slider*> (c) != nullptr;
}

static void checkLayout (juce::Component& c, const juce::String& where)
{
    if (dynamic_cast<juce::AudioDeviceSelectorComponent*> (&c) != nullptr)
        return;   // JUCE's own
    std::vector<juce::Component*> shown;
    for (auto* ch : c.getChildren())
    {
        if (! ch->isVisible())
            continue;
        const auto name = where + " > " + (ch->getName().isNotEmpty() ? ch->getName() : juce::String (typeid (*ch).name()).fromLastOccurrenceOf (" ", false, false));
        auto* tb = dynamic_cast<juce::TextButton*> (ch);
        const auto label = tb != nullptr ? " \"" + tb->getButtonText() + "\"" : juce::String();
        ++checks;
        if (ch->getWidth() <= 0 || ch->getHeight() <= 0)
        {
            fail (name + label + ": visible but has no size");
            continue;
        }
        if (! insideViewport (c))
        {
            ++checks;
            if (! c.getLocalBounds().contains (ch->getBounds()))
                fail (name + label + ": sticks out of its parent (" + ch->getBounds().toString() + " in " + c.getLocalBounds().toString() + ")");
        }
        if (tb != nullptr && tb->getButtonText().isNotEmpty())
        {
            const auto font = c.getLookAndFeel().getTextButtonFont (*tb, tb->getHeight());
            juce::GlyphArrangement ga;
            ga.addLineOfText (font, tb->getButtonText(), 0, 0);
            const float w = ga.getBoundingBox (0, -1, true).getWidth();
            // JUCE squeezes text up to 30% narrower and wraps to two lines on tall buttons; beyond that it is cut off
            const int lines = tb->getHeight() >= 2.6f * font.getHeight() ? 2 : 1;
            ++checks;
            if (w * 0.7f > (float) (tb->getWidth() - 8) * lines)
                fail (name + label + ": text does not fit (" + juce::String (juce::roundToInt (w)) + " px of text in " + juce::String (tb->getWidth()) + " px)");
        }
        if (interactive (ch))
            shown.push_back (ch);
        checkLayout (*ch, name);
    }
    for (size_t i = 0; i < shown.size(); ++i)
        for (size_t j = i + 1; j < shown.size(); ++j)
        {
            const auto overlap = shown[i]->getBounds().getIntersection (shown[j]->getBounds());
            ++checks;
            if (overlap.getWidth() > 3 && overlap.getHeight() > 3)
            {
                auto text = [] (juce::Component* x) { auto* b = dynamic_cast<juce::Button*> (x); return b != nullptr ? b->getButtonText() : juce::String (typeid (*x).name()); };
                fail (where + ": \"" + text (shown[i]) + "\" and \"" + text (shown[j]) + "\" overlap");
            }
        }
}

// a user can press it: shown on screen and enabled
static bool click (juce::Button& b, const juce::String& where)
{
    ++checks;
    if (! b.isShowing() || ! b.isEnabled())
    {
        fail (where + ": button \"" + b.getButtonText() + "\" cannot be pressed (" + (b.isShowing() ? "disabled" : "not on screen") + ")");
        return false;
    }
    if (b.getClickingTogglesState())
        b.setToggleState (! b.getToggleState(), juce::dontSendNotification);
    if (b.onClick) b.onClick();
    return true;
}

static void choose (juce::ComboBox& c, int itemIndex)
{
    c.setSelectedItemIndex (itemIndex, juce::sendNotificationSync);
}

static float hzOf (int midi) { return (float) (440.0 * std::pow (2.0, (midi - 69) / 12.0)); }
static NoteEvent note (int midi, double t = 0.0) { NoteEvent e; e.midi = midi; e.hz = hzOf (midi); e.time = t; return e; }

// ---------------------------------------------------------------- the tester

struct PathTester
{
    juce::File progressFile;
    std::unique_ptr<Progress> progress;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<MainComponent> main;

    void open()
    {
        main = nullptr;
        progress = std::make_unique<Progress> (progressFile);
        if (engine == nullptr) engine = std::make_unique<AudioEngine>();   // never started: no device
        main = std::make_unique<MainComponent> (*engine, *progress);
        main->setSize (1280, 820);
        main->setVisible (true);
        main->addToDesktop (0);   // so isShowing() is true; the window is never made visible on screen
        main->setVisible (true);
    }

    // ------------------------------------------------ one exercise, all ten questions

    enum class Way { right, wrongThenRight, showAnswer };

    static bool hasMode (const Question& q)
    {
        return ! q.choices.isEmpty() || q.fretClick || q.staffClick || (q.acceptsPlaying && q.judgePlayed) || q.tapRhythm;
    }

    // give one answer through the way a user would; returns false when no such answer exists
    bool answer (ExerciseView& ex, TheoryPage& page, bool right, const juce::String& where)
    {
        auto& q = ex.q;
        ex.questionShownAt = juce::Time::getMillisecondCounter() - 1000;   // as if a second had passed

        if (q.tapRhythm)
            return false;   // tapped with tapRhythm()
        if (! q.sequence.empty())
        {
            const auto& want = q.sequence[(size_t) ex.seqPos];
            if (! q.choices.isEmpty())
            {
                for (int i = 0; i < q.choices.size(); ++i)
                    if ((q.choices[i] == want.name()) == right)
                        return click (*ex.choiceButtons[i], where);
                return false;
            }
            if (q.fretClick)
            {
                for (int s = 1; s <= 6; ++s)
                    for (int f = 0; f <= q.maxFret; ++f)
                    {
                        const int m = theory::fretMidi (s, f);
                        const bool ok = q.sequenceAnyOctave ? theory::mod12 (m) == want.pc() : m == want.midi();
                        if (ok == right) { ex.fretboard.onClick (s, f); return true; }
                    }
                return false;
            }
            if (q.staffClick)
            {
                auto n = want;
                if (! right) n.letter = (n.letter + 1) % 7;
                ex.staff.onPlace (n);
                return true;
            }
            if (q.acceptsPlaying)
            {
                const int m = right ? want.midi() : want.midi() + 1;
                page.notePlayed (note (m));
                return true;
            }
            return false;
        }
        if (! q.choices.isEmpty())
        {
            for (int i = 0; i < q.choices.size(); ++i)
            {
                const bool ok = q.judgeChoice ? q.judgeChoice (i) : i == q.correctChoice;
                if (ok == right)
                    return click (*ex.choiceButtons[i], where + " choice \"" + q.choices[i] + "\"");
            }
            return false;
        }
        if (q.fretClick && q.judgeFret)
        {
            for (int s = 1; s <= 6; ++s)
                for (int f = 0; f <= q.maxFret; ++f)
                    if (q.judgeFret (s, f) == right) { ex.fretboard.onClick (s, f); return true; }
            return false;
        }
        if (q.staffClick && q.judgeStaff)
        {
            for (int oct = 2; oct <= 6; ++oct)
                for (int l = 0; l < 7; ++l)
                    for (int a : { 0, -1, 1, -2, 2 })
                    {
                        const theory::Note n { l, a, oct };
                        if (q.judgeStaff (n) == right) { ex.staff.onPlace (n); return true; }
                    }
            return false;
        }
        if (q.acceptsPlaying && q.judgePlayed)
        {
            for (int m = 40; m <= 88; ++m)
            {
                if (q.ignorePlayed && q.ignorePlayed (m)) continue;
                if (q.judgePlayed (m) == right) { page.notePlayed (note (m)); return true; }
            }
            return false;
        }
        return false;
    }

    void tapRhythm (ExerciseView& ex, double jitterBeats)
    {
        auto& q = ex.q;
        ex.tapPad.onTap();   // the count-in
        const double spb = 60.0 / q.tapBpm;
        juce::Random r (5);
        for (auto& e : q.rhythm)
            if (! ex.done)
                ex.tap (100.0 + (e.beat + (r.nextDouble() - 0.5) * 2.0 * jitterBeats) * spb, 0);
    }

    void runExercise (TheoryPage& page, int lessonIndex, int exIndex, const Lesson& lesson, Way way, juce::int64 seed)
    {
        auto& ex = page.exercise;
        const auto where = lesson.id + "/" + juce::String (exIndex) + " (" + lesson.exercises[(size_t) exIndex].title + ") "
                         + (way == Way::right ? "[right]" : way == Way::wrongThenRight ? "[wrong, then right]" : "[show answer]");
        // open it the way a user does: lesson button, then the exercise button
        page.openLesson (lessonIndex);
        EXPECT (page.exerciseButtons.size() == (int) lesson.exercises.size(), "%s: %d exercise buttons for %d exercises", where.toRawUTF8(), page.exerciseButtons.size(), (int) lesson.exercises.size());
        if (exIndex >= page.exerciseButtons.size()) return;
        checkLayout (*main, "lesson " + lesson.id);
        ex.rng.setSeed (seed);
        if (! click (*page.exerciseButtons[exIndex], where)) return;
        ex.rng.setSeed (seed);
        ex.start (lesson, exIndex, seed);
        EXPECT (ex.isShowing(), "%s: exercise not on screen after clicking it", where.toRawUTF8());

        int expectScore = 0;
        for (int qn = 1; qn <= ExerciseView::questionsPerRound; ++qn)
        {
            const auto qwhere = where + " q" + juce::String (qn) + " \"" + ex.q.prompt + "\"";
            EXPECT (ex.questionNumber == qn, "%s: shows question %d", qwhere.toRawUTF8(), ex.questionNumber);
            EXPECT (hasMode (ex.q), "%s: no way to answer", qwhere.toRawUTF8());
            EXPECT (ex.q.prompt.isNotEmpty(), "%s: empty prompt", qwhere.toRawUTF8());
            checkLayout (ex, "exercise " + lesson.id + "/" + juce::String (exIndex));
            {
                // the prompt is drawn on one line: it must fit (JUCE squeezes up to 30%)
                const auto w = juce::GlyphArrangement::getStringWidth (Fonts::ui (20.0f), ex.q.prompt);
                EXPECT (w * 0.7f < (float) ex.getWidth() - 32, "%s: prompt too long for one line (%d px)", qwhere.toRawUTF8(), juce::roundToInt (w));
            }
            if (ex.q.tapRhythm)
            {
                if (way == Way::showAnswer) { if (click (ex.nextButton, qwhere)) {} }
                else
                {
                    if (way == Way::wrongThenRight) { tapRhythm (ex, 0.6); EXPECT (! ex.done || ex.missed, "%s: sloppy taps accepted as first try", qwhere.toRawUTF8()); }
                    if (! ex.done) tapRhythm (ex, 0.02);
                    if (way == Way::right) ++expectScore;
                }
            }
            else if (way == Way::showAnswer)
            {
                if (ex.nextButton.isVisible())
                    click (ex.nextButton, qwhere + " Show answer");
                else
                    answer (ex, page, false, qwhere);   // choice questions: a wrong choice shows the answer
            }
            else
            {
                const bool isChoice = ex.q.sequence.empty() && ! ex.q.choices.isEmpty();
                if (way == Way::wrongThenRight)
                {
                    const bool gave = answer (ex, page, false, qwhere);
                    EXPECT (gave, "%s: could not give a wrong answer", qwhere.toRawUTF8());
                    if (gave)
                    {
                        EXPECT (ex.missed, "%s: a wrong answer was not marked", qwhere.toRawUTF8());
                        EXPECT (ex.feedback.isNotEmpty(), "%s: no feedback after a wrong answer", qwhere.toRawUTF8());
                        EXPECT (isChoice == ex.done, "%s: after a wrong %s the question is %s", qwhere.toRawUTF8(), isChoice ? "choice" : "answer", ex.done ? "over" : "still open");
                    }
                }
                int guard = 0;
                while (! ex.done && guard++ < 40)
                    if (! answer (ex, page, true, qwhere)) { fail (qwhere + ": could not give the right answer"); break; }
                if (way == Way::right) ++expectScore;
            }
            EXPECT (ex.done, "%s: question not over", qwhere.toRawUTF8());
            EXPECT (ex.score == expectScore, "%s: score %d, expected %d", qwhere.toRawUTF8(), ex.score, expectScore);
            checkLayout (ex, "answered " + lesson.id + "/" + juce::String (exIndex));

            // after a right answer it moves on by itself; otherwise Next/Finish is there to press
            const bool gotItRight = way != Way::showAnswer && ex.feedbackColour == Palette::green;
            if (gotItRight)
            {
                EXPECT (ex.isTimerRunning(), "%s: right answer but no automatic move to the next question", qwhere.toRawUTF8());
                EXPECT (ex.nextButton.isVisible(), "%s: Next hidden while waiting", qwhere.toRawUTF8());
                if (qn % 2 == 0 && ex.nextButton.isVisible()) click (ex.nextButton, qwhere + " Next (early)");
                else ex.timerCallback();
            }
            else
            {
                EXPECT (ex.nextButton.getButtonText() == (qn == 10 ? "Finish" : "Next"), "%s: button says \"%s\"", qwhere.toRawUTF8(), ex.nextButton.getButtonText().toRawUTF8());
                click (ex.nextButton, qwhere);
            }
        }
        EXPECT (ex.finished, "%s: round not finished after 10 questions", where.toRawUTF8());
        EXPECT (ex.score == expectScore, "%s: final score %d, expected %d", where.toRawUTF8(), ex.score, expectScore);
        const auto key = lesson.exerciseKey (exIndex);
        EXPECT (progress->lastScore (key) >= 0 && std::abs (progress->lastScore (key) - expectScore / 10.0) < 1e-9, "%s: saved score %.2f", where.toRawUTF8(), progress->lastScore (key));
        EXPECT (progress->lastDate (key) == progress->today(), "%s: not dated today", where.toRawUTF8());
        checkLayout (ex, "finished " + lesson.id);
        EXPECT (ex.doneButton.isShowing() && ex.againButton.isShowing(), "%s: Done/Try again not shown at the end", where.toRawUTF8());
        EXPECT (! ex.nextButton.isVisible() && ! ex.listenButton.isVisible(), "%s: question buttons left on the summary", where.toRawUTF8());

        if (way == Way::wrongThenRight)
        {
            click (ex.againButton, where + " Try again");
            EXPECT (! ex.finished && ex.questionNumber == 1 && ex.score == 0, "%s: Try again did not restart", where.toRawUTF8());
            click (ex.backButton, where + " < Lesson");
        }
        else
            click (ex.doneButton, where + " Done");
        EXPECT (! ex.isVisible(), "%s: still in the exercise after leaving it", where.toRawUTF8());
        EXPECT (page.exerciseButtons.size() > 0 && page.exerciseButtons[0]->isShowing(), "%s: lesson page not back", where.toRawUTF8());
        EXPECT (page.exerciseButtons[exIndex]->getButtonText().contains (juce::String (expectScore * 10) + "%") || way != Way::right,
                "%s: exercise button does not show the score: %s", where.toRawUTF8(), page.exerciseButtons[exIndex]->getButtonText().toRawUTF8());
    }

    void allExercises (TheoryPage& page, const std::vector<Lesson>& lessons, const char* track)
    {
        int n = 0;
        for (int l = 0; l < (int) lessons.size(); ++l)
            for (int x = 0; x < (int) lessons[(size_t) l].exercises.size(); ++x)
                for (auto way : { Way::wrongThenRight, Way::showAnswer, Way::right })
                    for (juce::int64 seed : { 11, 12345 })
                    {
                        runExercise (page, l, x, lessons[(size_t) l], way, seed + l * 100 + x);
                        ++n;
                    }
        std::printf ("  %s: %d rounds played through\n", track, n);
    }

    // ------------------------------------------------ the rest of the app

    void tabs()
    {
        for (int p = 0; p < MainComponent::numPages; ++p)
        {
            click (*main->tabs[p], "tab " + juce::String (p));
            EXPECT (main->current == p, "tab %d did not open its page", p);
            checkLayout (*main, "page " + main->tabs[p]->getButtonText());
        }
    }

    void home()
    {
        click (*main->tabs[MainComponent::home], "Home tab");
        auto& h = main->homePage;
        for (int i = 0; i < h.length.getNumItems(); ++i)
        {
            choose (h.length, i);
            EXPECT (h.rows.size() > 0, "home: no plan for %s", h.length.getText().toRawUTF8());
            int minutes = 0;
            for (auto* r : h.rows) minutes += r->item.minutes;
            EXPECT (std::abs (minutes - h.length.getSelectedId()) <= 10, "home: %s plan adds up to %d min", h.length.getText().toRawUTF8(), minutes);
            checkLayout (*main, "home " + h.length.getText());
            const int bottom = h.getHeight() - 16 - 14 - 40;
            for (auto* r : h.rows)
                EXPECT (r->getBottom() <= bottom, "home %s: plan row \"%s\" runs into the note under the list", h.length.getText().toRawUTF8(), r->item.title.toRawUTF8());
        }
        choose (h.length, 1);

        // every plan item opens the right place, and doing it ticks it off
        const auto items = h.plan;
        for (size_t i = 0; i < items.size(); ++i)
        {
            click (*main->tabs[MainComponent::home], "Home tab");
            const auto item = main->homePage.plan[i];
            const auto where = "plan item \"" + item.title + "\"";
            EXPECT (main->homePage.rows[(int) i]->isShowing(), "%s: row not on screen", where.toRawUTF8());
            main->homePage.rows[(int) i]->onClick();
            checkLayout (*main, where);
            switch (item.kind)
            {
                case PlanItem::Kind::warmup:
                    EXPECT (main->current == MainComponent::practice && main->practicePage.currentTab == PracticePage::techniqueTab, "%s: Practice > Technique not open", where.toRawUTF8());
                    playRun (main->practicePage.technique, where);
                    break;
                case PlanItem::Kind::scales:
                    EXPECT (main->current == MainComponent::practice && main->practicePage.currentTab == PracticePage::scalesTab, "%s: Practice > Scales not open", where.toRawUTF8());
                    playRun (main->practicePage.scales, where);
                    break;
                case PlanItem::Kind::improv:
                    EXPECT (main->current == MainComponent::improv && main->improvPage.currentTab == ImprovPage::licksTab, "%s: Licks not open", where.toRawUTF8());
                    EXPECT (main->improvPage.licksPanel.licks[(size_t) main->improvPage.licksPanel.current].id == item.lickId, "%s: wrong lick selected", where.toRawUTF8());
                    playLick (main->improvPage.licksPanel, 0, where);
                    break;
                default:
                {
                    auto& page = main->current == MainComponent::improv ? main->improvPage.lessons : main->theoryPage;
                    EXPECT (page.exercise.isShowing(), "%s: exercise not open (page %d)", where.toRawUTF8(), main->current);
                    if (! page.exercise.isShowing()) break;
                    EXPECT (page.exercise.lesson->id == item.lessonId && page.exercise.exerciseIndex == item.exercise, "%s: opened %s/%d", where.toRawUTF8(),
                            page.exercise.lesson->id.toRawUTF8(), page.exercise.exerciseIndex);
                    auto& ex = page.exercise;
                    for (int g = 0; g < 400 && ! ex.finished; ++g)
                    {
                        if (ex.done) { ex.timerCallback(); continue; }
                        if (ex.q.tapRhythm) tapRhythm (ex, 0.0);
                        else if (! answer (ex, page, true, where)) break;
                    }
                    EXPECT (ex.finished, "%s: could not finish the round", where.toRawUTF8());
                    if (ex.finished) click (ex.doneButton, where + " Done");
                }
            }
            click (*main->tabs[MainComponent::home], "Home tab");
            EXPECT (main->homePage.rows.size() == (int) items.size(), "%s: the plan changed after doing an item (%d items, was %d)", where.toRawUTF8(),
                    main->homePage.rows.size(), (int) items.size());
            if ((int) i < main->homePage.rows.size())
            {
                EXPECT (main->homePage.rows[(int) i]->item.title == item.title, "%s: plan reshuffled after doing it", where.toRawUTF8());
                EXPECT (main->homePage.rows[(int) i]->done, "%s: done, but not ticked off on Home", where.toRawUTF8());
            }
        }
        // the ticked-off plan must survive closing and opening the app
        int doneBefore = 0;
        for (auto* r : main->homePage.rows) doneBefore += r->done ? 1 : 0;
        open();
        int doneAfter = 0;
        for (auto* r : main->homePage.rows) doneAfter += r->done ? 1 : 0;
        EXPECT (doneBefore == (int) items.size() && doneAfter == doneBefore, "home: %d of %d done before reopening, %d after", doneBefore, (int) items.size(), doneAfter);
        checkLayout (*main, "home all done");
        // and changing the session length after finishing it keeps today's work
        choose (main->homePage.length, 0);
        choose (main->homePage.length, 1);
        int doneAfterLength = 0;
        for (auto* r : main->homePage.rows) doneAfterLength += r->done ? 1 : 0;
        EXPECT (doneAfterLength == doneBefore, "home: after switching the length to 30 and back, %d of %d done", doneAfterLength, doneBefore);
        click (main->homePage.continueButton, "Continue");
        EXPECT (main->current == MainComponent::theory, "Continue did not open Music Theory");
        click (*main->tabs[MainComponent::home], "Home tab");
        click (main->homePage.practiceButton, "Open Practice");
        EXPECT (main->current == MainComponent::practice, "Open Practice did not open Practice");
    }

    void playRun (PatternTrainer& t, const juce::String& where, int mistakes = 0)
    {
        if (! t.startButton.isShowing()) { fail (where + ": Start not on screen"); return; }
        click (t.startButton, where + " Start");
        EXPECT (t.running, "%s: run did not start", where.toRawUTF8());
        double time = 50.0;
        const double gap = 60.0 / (double) engine->metronomeBpm.load() / t.npb();
        const auto targets = t.targets;
        for (size_t i = 0; i < targets.size() && t.running; ++i)
        {
            if ((int) i < mistakes) t.notePlayed (note (targets[i].midi + 1, time += gap));
            t.notePlayed (note (targets[i].midi, time += gap));
        }
        EXPECT (! t.running, "%s: run did not end after the last note (%d of %d)", where.toRawUTF8(), t.pos, (int) targets.size());
        EXPECT (t.result.isNotEmpty(), "%s: no result after the run", where.toRawUTF8());
        checkLayout (t, where + " result");
    }

    void practice()
    {
        click (*main->tabs[MainComponent::practice], "Practice tab");
        auto& p = main->practicePage;
        for (int tab = 0; tab < 3; ++tab)
        {
            click (p.tabButtons[tab], "practice tab " + juce::String (tab));
            EXPECT (p.currentTab == tab, "practice tab %d did not open", tab);
            checkLayout (*main, "practice tab " + juce::String (tab));
        }

        // scales: every scale, every fingering, every pattern, a few roots; a clean run and one with mistakes
        click (p.tabButtons[PracticePage::scalesTab], "Scales");
        auto& s = p.scales;
        int runs = 0;
        for (int scale = 0; scale < s.scale.getNumItems(); ++scale)
            for (int root : { 0, 4, 7, 10 })
            {
                choose (s.root, root);
                choose (s.scale, scale);
                for (int f = 0; f < s.fingering.getNumItems(); ++f)
                {
                    choose (s.fingering, f);
                    for (int pt = 0; pt < s.pattern.getNumItems(); ++pt)
                    {
                        choose (s.pattern, pt);
                        const auto where = "scales " + s.root.getText() + " " + s.scale.getText() + " / " + s.fingering.getText() + " / " + s.pattern.getText();
                        EXPECT (! s.targets.empty(), "%s: nothing to play", where.toRawUTF8());
                        for (auto& n : s.targets)
                            EXPECT (n.fret >= 0 && n.fret <= 22 && theory::fretMidi (n.string, n.fret) == n.midi, "%s: target off the neck or wrong pitch", where.toRawUTF8());
                        for (size_t i = 1; i < s.targets.size(); ++i)
                            EXPECT (s.targets[i].midi != s.targets[i - 1].midi, "%s: the same note twice in a row at %d", where.toRawUTF8(), (int) i);
                        if ((runs++ % 7) == 0) playRun (s, where, (runs % 3 == 0) ? 2 : 0);
                        checkLayout (s, where);
                    }
                }
            }
        std::printf ("  scales: %d combinations\n", runs);

        click (p.tabButtons[PracticePage::techniqueTab], "Technique");
        auto& t = p.technique;
        for (int d = 0; d < t.scale.getNumItems(); ++d)
        {
            choose (t.scale, d);
            for (int npb = 0; npb < t.notesPerBeat.getNumItems(); ++npb)
            {
                choose (t.notesPerBeat, npb);
                for (int sf = 0; sf < t.startFret.getNumItems(); sf += 3)
                {
                    choose (t.startFret, sf);
                    const auto where = "technique " + t.scale.getText() + " / " + t.notesPerBeat.getText() + " / " + t.startFret.getText();
                    EXPECT (! t.targets.empty(), "%s: nothing to play", where.toRawUTF8());
                    for (size_t i = 1; i < t.targets.size(); ++i)
                        EXPECT (t.targets[i].midi != t.targets[i - 1].midi, "%s: the same note twice in a row at %d", where.toRawUTF8(), (int) i);
                    for (auto& n : t.targets)
                        EXPECT (n.fret >= 0 && n.fret <= 22, "%s: target off the neck", where.toRawUTF8());
                    playRun (t, where);
                }
            }
        }
        // the tempo ladder speeds up after a clean run
        if (! t.ladder.getToggleState()) click (t.ladder, "ladder");
        t.engine.metronomeBpm.store (80.0f);
        const double before = t.engine.metronomeBpm.load();
        playRun (t, "technique ladder");
        EXPECT (t.engine.metronomeBpm.load() > before, "tempo ladder did not raise the tempo (%.0f -> %.0f)", before, t.engine.metronomeBpm.load());
        // stop in the middle
        click (t.startButton, "technique start");
        t.notePlayed (note (t.targets[0].midi, 1.0));
        click (t.startButton, "technique stop");
        EXPECT (! t.running, "Stop did not stop the run");
        // a run left running when changing tab must not go on counting
        click (t.startButton, "technique start again");
        click (*main->tabs[MainComponent::theory], "Theory tab");
        click (*main->tabs[MainComponent::practice], "Practice tab");
        EXPECT (! t.running || t.startButton.getButtonText() == "Stop", "run state and button disagree after changing tab");
        if (t.running) click (t.startButton, "technique stop");

        // fretboard explorer: every root/labels/show, then the note finder
        click (p.tabButtons[PracticePage::fretboardTab], "Fretboard");
        auto& e = p.explorer;
        for (auto* c : { &e.root, &e.labels, &e.show })
            for (int i = 0; i < c->getNumItems(); ++i)
            {
                choose (*c, i);
                checkLayout (e, "explorer " + c->getText());
            }
        click (e.drillButton, "note finder");
        EXPECT (e.drilling, "note finder did not start");
        for (int str = 6; str >= 1 && e.drilling; --str)
        {
            e.notePlayed (note (theory::fretMidi (str, 0) + 13));                     // a wrong note first
            for (int f = 0; f <= 12; ++f)
                if (theory::mod12 (theory::fretMidi (str, f)) == e.drillPc) { e.notePlayed (note (theory::fretMidi (str, f))); break; }
        }
        EXPECT (! e.drilling && e.drillResult.isNotEmpty(), "note finder did not finish after all six strings");
        EXPECT (progress->bestDrill ("noteFinder") > 0, "note finder time not saved");

        // metronome
        auto& m = p.metronome;
        if (engine->metronomeOn.load()) click (m.startButton, "metronome stop (left on by a drill)");
        click (m.startButton, "metronome start");
        EXPECT (engine->metronomeOn.load(), "metronome did not start");
        click (m.faster, "+5");
        click (m.slower, "-5");
        click (m.slower, "-5");
        for (int i = 0; i < m.beats.getNumItems(); ++i) choose (m.beats, i);
        click (m.tapButton, "tap");
        click (m.startButton, "metronome stop");
        EXPECT (! engine->metronomeOn.load(), "metronome did not stop");
        m.bpm.setValue (m.bpm.getMinimum(), juce::sendNotificationSync);
        click (m.slower, "-5 at the slowest");
        EXPECT (m.bpm.getValue() >= m.bpm.getMinimum(), "tempo below the minimum");
        m.bpm.setValue (m.bpm.getMaximum(), juce::sendNotificationSync);
        click (m.faster, "+5 at the fastest");
        EXPECT (m.bpm.getValue() <= m.bpm.getMaximum(), "tempo above the maximum");
        m.bpm.setValue (80, juce::sendNotificationSync);
        checkLayout (*main, "practice end");
    }

    void playLick (LickPanel& lp, int wrong, const juce::String& where)
    {
        click (lp.turnButton, where + " Your turn");
        EXPECT (lp.running, "%s: turn did not start", where.toRawUTF8());
        const double spb = 60.0 / lp.tempo.getValue();
        int wrongLeft = wrong;
        double last = 0;
        for (auto& n : lp.lick.notes)
        {
            if (! lp.running) break;
            const double t = 20.0 + swingBeat (n.beat, lp.swung()) * spb;
            if (wrongLeft-- > 0) lp.notePlayed (note (n.targetMidi() + 5, t - 0.05));
            if (n.tech == "b" || n.tech == "pb") lp.notePlayed (note (n.fretMidi(), t));
            lp.notePlayed (note (n.targetMidi(), t + 0.01));
            last = t;
        }
        juce::ignoreUnused (last);
        EXPECT (! lp.running, "%s: the turn did not end after the last note (at %d of %d)", where.toRawUTF8(), lp.follower.position(), (int) lp.lick.notes.size());
        if (lp.running) click (lp.turnButton, where + " Stop");
        EXPECT (lp.result.isNotEmpty(), "%s: no result", where.toRawUTF8());
        if (wrong == 0)
            EXPECT (progress->passed ("lick/" + lp.licks[(size_t) lp.current].id), "%s: played perfectly but not learned (%s)", where.toRawUTF8(), lp.result.toRawUTF8());
        checkLayout (lp, where + " result");
    }

    void improv()
    {
        click (*main->tabs[MainComponent::improv], "Improvisation tab");
        auto& ip = main->improvPage;
        for (int tab = 0; tab < 3; ++tab)
        {
            click (ip.tabButtons[tab], "improv tab " + juce::String (tab));
            EXPECT (ip.currentTab == tab, "improv tab %d did not open", tab);
            checkLayout (*main, "improv tab " + juce::String (tab));
        }
        // backing: every setting, play and stop
        click (ip.tabButtons[ImprovPage::backingTab], "Backing");
        auto& b = ip.backing;
        for (auto* c : { &b.key, &b.form, &b.feel, &b.scaleMap })
            for (int i = 0; i < c->getNumItems(); ++i)
            {
                choose (*c, i);
                checkLayout (b, "backing " + c->getText());
                for (int bar = 0; bar < 16; bar += 5) { b.showBar (bar); }
            }
        for (auto* t : { &b.drums, &b.bass, &b.guitar, &b.keys }) { click (*t, "backing toggle"); click (*t, "backing toggle"); }
        // (the band starts and stops on the audio thread, which does not run here)
        click (b.playButton, "backing play");
        click (b.playButton, "backing stop");
        click (*main->tabs[MainComponent::theory], "Theory");
        click (*main->tabs[MainComponent::improv], "back to Improvisation");

        // licks: each lick, in a few keys, played right and with mistakes
        click (ip.tabButtons[ImprovPage::licksTab], "Licks");
        auto& lp = ip.licksPanel;
        for (int l = 0; l < (int) lp.licks.size(); ++l)
        {
            click (*lp.lickButtons[l], "lick " + lp.licks[(size_t) l].name);
            EXPECT (lp.current == l, "lick button %d did not select it", l);
            for (int k : { 9, 4, 0, 7 })
            {
                lp.key.setSelectedId (k + 1, juce::sendNotificationSync);
                for (auto& n : lp.lick.notes)
                    EXPECT (n.fret >= 0 && n.fret <= 22, "lick %s in key %d: fret %d", lp.lick.name.toRawUTF8(), k, n.fret);
                checkLayout (lp, "lick " + lp.lick.name + " key " + juce::String (k));
            }
            lp.key.setSelectedId (10, juce::sendNotificationSync);
            click (lp.listenButton, "lick listen");
            playLick (lp, 3, "lick " + lp.lick.name + " with mistakes");
            playLick (lp, 0, "lick " + lp.lick.name);
        }
        if (! lp.withBacking.getToggleState()) click (lp.withBacking, "with backing");
        playLick (lp, 0, "lick with backing");
        engine->getBacking().stop();
    }

    void audioAndStats()
    {
        click (*main->tabs[MainComponent::audio], "Audio tab");
        auto& a = main->audioPage;
        checkLayout (*main, "audio page");
        click (a.monitorToggle, "Hear my guitar");
        EXPECT (engine->monitorOn.load() == a.monitorToggle.getToggleState(), "monitor toggle not applied");
        choose (a.monitorTone, 1);
        EXPECT (! engine->monitorWarm.load(), "dry tone not applied");
        choose (a.noteGate, 2);
        EXPECT (engine->getNoteGate() > 0.4f, "strict note gate not applied");
        a.monitorVolume.setValue (0.25, juce::sendNotificationSync);
        // the header pill switches the monitor too, and the page follows
        const bool before = engine->monitorOn.load();
        const auto pos = main->livePill().getCentre().toFloat();
        const auto now = juce::Time::getCurrentTime();
        juce::MouseEvent e (juce::Desktop::getInstance().getMainMouseSource(), pos, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, main.get(), main.get(), now, pos, now, 1, false);
        main->mouseDown (e);
        EXPECT (engine->monitorOn.load() != before && a.monitorToggle.getToggleState() == engine->monitorOn.load(), "header pill did not switch the monitor");
        // saved: reopen and check
        const bool on = engine->monitorOn.load();
        open();
        EXPECT (engine->monitorOn.load() == on && ! engine->monitorWarm.load() && engine->getNoteGate() > 0.4f && std::abs (engine->monitorVolume.load() - 0.25f) < 0.01f,
                "audio settings not kept after reopening");
        auto& a2 = main->audioPage;
        EXPECT (a2.noteGate.getSelectedId() == 3 && a2.monitorTone.getSelectedId() == 2, "audio page does not show the saved settings");
        choose (a2.noteGate, 1);
        choose (a2.monitorTone, 0);
        if (! engine->monitorOn.load()) click (a2.monitorToggle, "monitor back on");

        click (*main->tabs[MainComponent::stats], "Progress tab");
        checkLayout (*main, "progress page");
        EXPECT (main->statsPage.tiles.size() > 0, "progress page has no tiles");
    }

    void sizes()
    {
        for (auto size : { juce::Point<int> (1100, 720), juce::Point<int> (1280, 820), juce::Point<int> (1920, 1080), juce::Point<int> (2400, 1600) })
        {
            main->setSize (size.x, size.y);
            for (int p = 0; p < MainComponent::numPages; ++p)
            {
                click (*main->tabs[p], "tab");
                checkLayout (*main, "size " + juce::String (size.x) + "x" + juce::String (size.y) + " page " + juce::String (p));
            }
            for (int tab = 0; tab < 3; ++tab)
            {
                click (*main->tabs[MainComponent::practice], "Practice");
                click (main->practicePage.tabButtons[tab], "practice tab");
                checkLayout (*main, "size " + juce::String (size.x) + " practice tab " + juce::String (tab));
                click (*main->tabs[MainComponent::improv], "Improv");
                click (main->improvPage.tabButtons[tab], "improv tab");
                checkLayout (*main, "size " + juce::String (size.x) + " improv tab " + juce::String (tab));
            }
            click (*main->tabs[MainComponent::theory], "Theory");
            for (int l = 0; l < (int) main->lessons.size(); ++l)
            {
                main->theoryPage.openLesson (l);
                checkLayout (*main, "size " + juce::String (size.x) + " lesson " + juce::String (l + 1));
            }
            // a few exercises at each size
            for (int l : { 0, 3, 7, 14, 21 })
            {
                main->theoryPage.openExercise (l, 0, 3);
                checkLayout (*main, "size " + juce::String (size.x) + " exercise L" + juce::String (l + 1));
                main->theoryPage.exercise.simulateFinish (8);
                checkLayout (*main, "size " + juce::String (size.x) + " finished L" + juce::String (l + 1));
                click (main->theoryPage.exercise.doneButton, "Done");
            }
        }
        main->setSize (1280, 820);
    }

    // leaving an exercise while it waits to move on, live input on a page that is not showing, and other odd orders
    void oddOrders()
    {
        auto& tp = main->theoryPage;
        click (*main->tabs[MainComponent::theory], "Theory");
        tp.openExercise (0, 0, 9);
        auto& ex = tp.exercise;
        answer (ex, tp, true, "odd: right answer");
        EXPECT (ex.isTimerRunning(), "odd: no timer after a right answer");
        click (*main->tabs[MainComponent::practice], "leave for Practice while waiting");
        if (ex.isTimerRunning()) ex.timerCallback();
        click (*main->tabs[MainComponent::theory], "back to Theory");
        EXPECT (ex.questionNumber == 2 || ! ex.isVisible(), "odd: question %d after coming back", ex.questionNumber);
        click (ex.backButton, "< Lesson while waiting");
        EXPECT (! ex.isTimerRunning(), "odd: timer still running after leaving the exercise");
        // notes arriving on every page, with nothing open
        for (int p = 0; p < MainComponent::numPages; ++p)
        {
            click (*main->tabs[p], "tab");
            if (auto* in = main->pageInput (p))
                for (int m = 40; m < 90; m += 7) { in->notePlayed (note (m)); in->pitchChanged ({ true, hzOf (m), (float) m, 0.95f, 0.1f }); in->onsetDetected (1.0); }
        }
        // a round finished, then Try again twice, then Done
        click (*main->tabs[MainComponent::theory], "Theory");
        tp.openExercise (1, 0, 4);
        tp.exercise.simulateFinish (10);
        click (tp.exercise.againButton, "Try again");
        tp.exercise.simulateFinish (3);
        click (tp.exercise.againButton, "Try again");
        tp.exercise.simulateFinish (9);
        click (tp.exercise.doneButton, "Done");
        EXPECT (std::abs (progress->bestScore ("L2-neck/0") - 0.9) < 1e-9 || progress->bestScore ("L2-neck/0") >= 0.9, "odd: best score lost (%f)", progress->bestScore ("L2-neck/0"));
        // switching lessons quickly
        for (int l = 0; l < (int) main->lessons.size(); ++l) click (*tp.lessonButtons[l], "lesson button " + juce::String (l + 1));
        checkLayout (*main, "after clicking through lessons");
    }

    void runAll()
    {
        open();
        std::printf ("tabs\n");         tabs();
        std::printf ("home plan\n");    home();
        click (*main->tabs[MainComponent::theory], "Music Theory");
        std::printf ("theory\n");       allExercises (main->theoryPage, main->lessons, "theory");
        std::printf ("blues\n");        click (*main->tabs[MainComponent::improv], "Improvisation");
                                        click (main->improvPage.tabButtons[0], "Lessons");
                                        allExercises (main->improvPage.lessons, main->bluesLessons, "blues");
        std::printf ("practice\n");     practice();
        std::printf ("improv\n");       improv();
        std::printf ("audio, stats\n"); audioAndStats();
        std::printf ("odd orders\n");   oddOrders();
        std::printf ("sizes\n");        sizes();

        // everything done is still there after closing and opening again
        progress->flush();
        const auto before = progressFile.loadFileAsString();
        open();
        progress->flush();
        const auto after = progressFile.loadFileAsString();
        EXPECT (before == after, "progress changed by closing and opening the app");
        main->removeFromDesktop();
        main = nullptr;
    }
};

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File folder (argc >= 2 ? juce::String (argv[1]) : juce::File::getSpecialLocation (juce::File::tempDirectory).getFullPathName());
    folder.createDirectory();

    PathTester t;
    t.progressFile = folder.getChildFile ("paths-progress.json");
    t.progressFile.deleteFile();
    t.runAll();
    std::printf ("%d checks, %d failed (%d different problems)\n", checks, failures, (int) reported.size());
    return failures == 0 ? 0 : 1;
}
