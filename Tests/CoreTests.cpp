// GuitarCoachTests: theory engine, lesson generators, pitch/note detection on synthetic guitar notes, progress file.
#include "core/Theory.h"
#include "core/Lessons.h"
#include "core/InputAnalyzer.h"
#include "core/GuitarVoice.h"
#include "core/Progress.h"
#include <cstdio>

using namespace theory;

static int failures = 0, checks = 0;
#define CHECK(cond, ...) do { ++checks; if (! (cond)) { ++failures; std::printf ("FAIL %s:%d  %s  ", __FILE__, __LINE__, #cond); std::printf (__VA_ARGS__); std::printf ("\n"); } } while (0)

static void testTheory()
{
    CHECK (noteFromMidi (40).name (true, false) == "E2", "low E");
    CHECK (noteFromMidi (61, true).name (true, false) == "Db4", "Db4");
    CHECK (parseNote ("Bb3")->midi() == 58, "Bb3");
    CHECK (parseNote ("F#4")->midi() == 66, "F#4");
    CHECK (parseNote (juce::String (juce::CharPointer_UTF8 ("C\xe2\x99\xaf" "5")))->midi() == 73, "C#5 unicode");

    auto iv = [] (const char* a, const char* b) { return intervalBetween (*parseNote (a), *parseNote (b)); };
    CHECK (iv ("C4", "E4").shortName() == "M3" && iv ("C4", "E4").semitones == 4, "C-E");
    CHECK (iv ("C4", "Eb4").shortName() == "m3", "C-Eb");
    CHECK (iv ("F4", "B4").shortName() == "A4", "F-B");
    CHECK (iv ("B3", "F4").shortName() == "d5", "B-F");
    CHECK (iv ("E2", "E3").shortName() == "P8", "octave");
    CHECK (iv ("A2", "G3").shortName() == "m7", "A-G");
    CHECK (iv ("C4", "D5").shortName() == "M9", "ninth");

    CHECK (transposeUp (*parseNote ("A2"), intervalFromShort ("P5")) == *parseNote ("E3"), "A + P5");
    CHECK (transposeUp (*parseNote ("Eb4"), intervalFromShort ("M3")) == *parseNote ("G4"), "Eb + M3");
    CHECK (transposeUp (*parseNote ("F#3"), intervalFromShort ("m3")) == *parseNote ("A3"), "F# + m3");
    CHECK (transposeUp (*parseNote ("Bb2"), intervalFromShort ("M7")) == *parseNote ("A3"), "Bb + M7");

    const auto& all = octaveIntervals();
    CHECK (all.size() == 13, "13 intervals");
    for (int i = 0; i < (int) all.size(); ++i)
        CHECK (all[(size_t) i].semitones == i, "interval %d", i);
    CHECK (intervalFromShort ("P4").longName() == "perfect 4th", "name");
    CHECK (intervalFromShort ("m6").longName() == "minor 6th", "name");

    CHECK (tonesText (3) == juce::String (juce::CharPointer_UTF8 ("1\xc2\xbd tones")), "1.5 tones");
    CHECK (tonesText (2) == "1 tone", "1 tone");
    CHECK (tonesText (1) == juce::String (juce::CharPointer_UTF8 ("\xc2\xbd tone")), "half tone");
    CHECK (tonesText (12) == "6 tones", "6 tones");

    CHECK (fretMidi (6, 0) == 40 && fretMidi (1, 12) == 76 && fretMidi (3, 4) == 59, "frets");
    CHECK (positionsOf (55, 12).size() == 3, "G3 has 3 places up to fret 12");
    CHECK (positionsOfPc (4, 12).size() == 8, "E everywhere: %d", (int) positionsOfPc (4, 12).size());
    CHECK (pcNameBoth (1) == juce::String (juce::CharPointer_UTF8 ("C\xe2\x99\xaf/D\xe2\x99\xad")), "C#/Db");
}

static void testLessons()
{
    const auto lessons = loadLessons();
    CHECK (lessons.size() == 4, "4 lessons, got %d", (int) lessons.size());
    juce::Random rng (42);
    int questions = 0;
    for (auto& l : lessons)
    {
        CHECK (l.body.size() > 3, "%s has text", l.id.toRawUTF8());
        for (auto& ex : l.exercises)
        {
            for (int i = 0; i < 300; ++i)
            {
                const auto q = makeQuestion (ex, rng);
                ++questions;
                const auto what = (l.id + " " + ex.title).toRawUTF8();
                if (! q.prompt.isNotEmpty() || q.prompt.startsWith ("Unknown"))
                {
                    CHECK (false, "%s: bad prompt", what);
                    break;
                }
                if (! q.choices.isEmpty())
                    CHECK (q.correctChoice >= 0 && q.correctChoice < q.choices.size(), "%s: correct choice in range (%s)", what, q.prompt.toRawUTF8());
                if (q.judgePlayed)
                    CHECK (q.expectedMidi >= 40 && q.judgePlayed (q.expectedMidi), "%s: expected note is accepted", what);
                if (q.judgeChoice)
                    CHECK (q.judgeChoice (q.correctChoice), "%s: judgeChoice", what);
                if (q.judgeFret)
                {
                    bool any = false;
                    for (int s = 1; s <= 6 && ! any; ++s)
                        for (int f = 0; f <= q.maxFret && ! any; ++f)
                            any = q.judgeFret (s, f);
                    CHECK (any, "%s: some fret is right", what);
                }
                if (q.judgeStaff)
                    CHECK (! q.revealStaff.empty() && q.judgeStaff (q.revealStaff[0]), "%s: staff answer", what);
                if (ex.type == "nameStaff" || ex.type == "playStaff")
                {
                    const auto lo = parseNote (ex.params["lo"].toString())->midi(), hi = parseNote (ex.params["hi"].toString())->midi();
                    CHECK (q.staffNotes.size() == 1 && q.staffNotes[0].midi() >= lo && q.staffNotes[0].midi() <= hi, "%s: in range", what);
                    if (ex.type == "nameStaff")
                        CHECK (q.choices[q.correctChoice] == q.staffNotes[0].name(), "%s: name matches", what);
                }
                if (ex.type == "intervalName" || ex.type == "earInterval")
                {
                    const auto shown = ex.type == "intervalName" ? q.staffNotes : q.revealStaff;
                    CHECK (shown.size() == 2, "%s: two notes", what);
                    if (shown.size() == 2)
                        CHECK (q.choices[q.correctChoice] == intervalBetween (shown[0], shown[1]).longName().upToFirstOccurrenceOf (" (", false, false),
                               "%s: interval name matches", what);
                }
            }
        }
    }
    std::printf ("  %d generated questions checked\n", questions);
}

// Plays a note through the synth voice and feeds it to the analyzer; returns the note events seen.
static std::vector<NoteEvent> detect (InputAnalyzer& a, GuitarVoice& v, double sr, const std::vector<int>& notes, double noteSeconds, double gapSeconds)
{
    std::vector<NoteEvent> events;
    std::vector<float> block (128);
    long long samplesDone = 0;
    auto run = [&] (double seconds)
    {
        const int blocks = (int) (seconds * sr / 128.0);
        for (int b = 0; b < blocks; ++b)
        {
            std::fill (block.begin(), block.end(), 0.0f);
            v.render (block.data(), 128);
            for (auto& x : block) x *= 0.3f;
            a.push (block.data(), 128);
            NoteEvent e;
            bool got = false;
            while (a.analyze (e, got))
                if (got)
                {
                    e.hz = (float) (samplesDone / sr);   // reuse: time of the event in seconds
                    events.push_back (e);
                }
            samplesDone += 128;
        }
    };
    for (int n : notes)
    {
        v.start (n, 0.9f, (int) (noteSeconds * sr));
        run (noteSeconds);
        v.release();
        run (gapSeconds);
    }
    return events;
}

static void testPitch()
{
    for (double sr : { 48000.0, 44100.0 })
    {
        int wrong = 0;
        float worstCents = 0.0f;
        for (int midi = 40; midi <= 84; ++midi)   // low E to the 1st string, 20th fret
        {
            InputAnalyzer a;
            a.prepare (sr);
            GuitarVoice v;
            v.prepare (sr);
            const auto ev = detect (a, v, sr, { midi }, 1.0, 0.0);
            const bool ok = ev.size() == 1 && ev[0].midi == midi;
            if (! ok)
            {
                ++wrong;
                std::printf ("    %.0f Hz: note %d -> %d events, first %d\n", sr, midi, (int) ev.size(), ev.empty() ? -1 : ev[0].midi);
            }
            else
                worstCents = std::max (worstCents, std::abs (ev[0].cents));
        }
        CHECK (wrong == 0, "every note E2..C6 detected once and right at %.0f Hz (%d wrong)", sr, wrong);
        CHECK (worstCents < 10.0f, "tuning within 10 cents (worst %.1f)", worstCents);
        std::printf ("  pitch at %.0f Hz: worst error %.1f cents\n", sr, worstCents);
    }

    // a phrase: each note reported once, and the same note plucked again is reported again
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        GuitarVoice v;
        v.prepare (48000.0);
        const std::vector<int> phrase { 45, 48, 50, 50, 52, 55, 57, 57 };
        const auto ev = detect (a, v, 48000.0, phrase, 0.45, 0.12);
        juce::String got;
        for (auto& e : ev) got << e.midi << " ";
        CHECK (ev.size() == phrase.size(), "phrase gives %d events: %s", (int) ev.size(), got.toRawUTF8());
        for (size_t i = 0; i < std::min (ev.size(), phrase.size()); ++i)
            CHECK (ev[i].midi == phrase[i], "phrase note %d", (int) i);
    }

    // a re-plucked note without silence in between still counts twice
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        GuitarVoice v;
        v.prepare (48000.0);
        const auto ev = detect (a, v, 48000.0, { 50, 50, 50 }, 0.5, 0.0);
        juce::String times;
        for (auto& e : ev) times << juce::String (e.hz, 2) << "s ";
        CHECK (ev.size() == 3, "three plucks of D3 give %d events at %s", (int) ev.size(), times.toRawUTF8());
    }

    // silence and quiet noise: nothing
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        juce::Random r (1);
        std::vector<float> block (128);
        int events = 0;
        for (int b = 0; b < 48000 * 2 / 128; ++b)
        {
            for (auto& x : block) x = (r.nextFloat() - 0.5f) * 0.002f;
            a.push (block.data(), 128);
            NoteEvent e;
            bool got = false;
            while (a.analyze (e, got)) events += got ? 1 : 0;
        }
        CHECK (events == 0, "noise gives no notes (%d)", events);
    }

    // mains hum (60 Hz with harmonics, about -45 dBFS peak) is not a note and not a pitch
    for (double hum : { 50.0, 60.0 })
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        std::vector<float> block (128);
        int events = 0, valid = 0;
        double t = 0.0;
        for (int b = 0; b < 48000 * 2 / 128; ++b)
        {
            for (auto& x : block)
            {
                x = (float) (0.004 * std::sin (2 * 3.14159265 * hum * t) + 0.0015 * std::sin (2 * 3.14159265 * 3 * hum * t));
                t += 1.0 / 48000.0;
            }
            a.push (block.data(), 128);
            NoteEvent e;
            bool got = false;
            while (a.analyze (e, got)) { events += got ? 1 : 0; valid += a.getLatest().valid ? 1 : 0; }
        }
        CHECK (events == 0 && valid == 0, "%.0f Hz hum: %d notes, %d pitch frames", hum, events, valid);
    }
}

static void testProgress()
{
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("guitarcoach-test-progress.json");
    file.deleteFile();
    {
        Progress p (file);
        CHECK (p.bestScore ("L1/0") < 0, "new");
        p.recordExercise ("L1/0", 6, 10);
        p.recordExercise ("L1/0", 9, 10);
        p.recordExercise ("L1/0", 7, 10);
        p.recordDrill ("noteFinder", 4.0);
        p.recordDrill ("noteFinder", 5.0);
    }
    Progress p (file);
    CHECK (std::abs (p.bestScore ("L1/0") - 0.9) < 1e-9, "best kept: %f", p.bestScore ("L1/0"));
    CHECK (p.attempts ("L1/0") == 3, "attempts");
    CHECK (p.passed ("L1/0"), "passed");
    CHECK (std::abs (p.bestDrill ("noteFinder") - 4.0) < 1e-9, "drill best");
    CHECK (p.streakDays() == 1, "streak today");
    file.deleteFile();
}

int main()
{
    std::printf ("theory\n");    testTheory();
    std::printf ("lessons\n");   testLessons();
    std::printf ("pitch\n");     testPitch();
    std::printf ("progress\n");  testProgress();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
