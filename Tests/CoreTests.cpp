// GuitarCoachTests: theory engine, lesson generators, pitch/note detection on synthetic guitar notes, progress file.
#include "core/Theory.h"
#include "core/Lessons.h"
#include "core/InputAnalyzer.h"
#include "core/GuitarVoice.h"
#include "core/Progress.h"
#include "core/Fingerings.h"
#include <cstdio>
#include <set>

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
    CHECK (lessons.size() == 10, "10 lessons, got %d", (int) lessons.size());
    std::set<juce::String> ids;
    for (auto& l : lessons)
        CHECK (ids.insert (l.id).second && l.level >= 1 && l.level <= 2 && ! l.exercises.empty(), "%s: unique id, level, exercises", l.id.toRawUTF8());
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
                if (! q.sequence.empty())
                {
                    CHECK (q.sequenceGiven < (int) q.sequence.size(), "%s: something left to answer", what);
                    if (! q.choices.isEmpty())
                        for (size_t k = (size_t) q.sequenceGiven; k < q.sequence.size(); ++k)
                            CHECK (q.choices.contains (q.sequence[k].name()), "%s: %s is a choice", what, q.sequence[k].name().toRawUTF8());
                    if (q.staffClick)
                        CHECK (q.staffAccidentalButtons || [&] { for (auto& n : q.sequence) if (n.acc != 0) return false; return true; }(), "%s: accidentals can be written", what);
                    if (q.acceptsPlaying)
                        CHECK (q.judgePlayed && q.judgeFret, "%s: playing and clicking both work", what);
                    for (auto& n : q.sequence)
                        CHECK (std::abs (n.acc) <= 1, "%s: no double accidentals (%s)", what, n.name().toRawUTF8());
                    if (ex.type == "spellScale" || ex.type == "writeScale" || ex.type == "playScale")
                        CHECK (q.sequence.size() == 8 && q.sequence.front().pc() == q.sequence.back().pc(), "%s: root to root", what);
                    if (ex.type == "spellTriad" || ex.type == "playTriad")
                        CHECK (q.sequence.size() == 3, "%s: three notes", what);
                    for (size_t k = 1; k < q.sequence.size(); ++k)
                        CHECK (q.sequence[k].midi() > q.sequence[k - 1].midi(), "%s: ascending", what);
                }
                if (q.showKeySignature)
                    CHECK (q.choices[q.correctChoice] == keyName (q.keySignature, (bool) ex.params.getProperty ("minor", false)), "%s: key matches", what);
                if (ex.type == "countKeySig" || ex.type == "circle" || ex.type == "earScale" || ex.type == "triadQuality" || ex.type == "earTriad")
                    CHECK (q.choices.size() >= 2 && q.choices.size() == juce::StringArray (q.choices).size(), "%s: choices", what);
                if (ex.type == "triadQuality" || ex.type == "inversionName")
                    CHECK (q.staffChord && q.staffNotes.size() == 3, "%s: a chord on the staff", what);
                if (ex.type == "inversionName")
                {
                    CHECK (q.staffNotes.front().midi() >= 48 - 12 && q.staffNotes.back().midi() <= 69, "%s: fits the staff", what);
                    auto chordPcs = std::set<int> { q.staffNotes[0].pc(), q.staffNotes[1].pc(), q.staffNotes[2].pc() };
                    CHECK (chordPcs.size() == 3, "%s: three different notes", what);
                }
                if (ex.type == "triadShape" || ex.type == "intervalShape")
                {
                    CHECK (! q.fretMarks.empty() && q.fretMarks.size() == q.revealMarks.size(), "%s: marks", what);
                    int lo = 99, hi = 0;
                    for (auto& m : q.fretMarks) { lo = juce::jmin (lo, m.fret); hi = juce::jmax (hi, m.fret); }
                    CHECK (hi - lo <= 4 && hi <= 15, "%s: playable span", what);
                }
                if (ex.type == "triadShape")
                {
                    juce::StringArray shown;
                    for (auto& m : q.fretMarks) shown.add (m.label);
                    CHECK (q.choices[q.correctChoice].startsWith (shown[0]) || q.choices[q.correctChoice].startsWith (shown[1]) || q.choices[q.correctChoice].startsWith (shown[2]),
                           "%s: the root is one of the shown notes", what);
                }
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

static juce::String names (const std::vector<Note>& notes)
{
    juce::StringArray s;
    for (auto& n : notes) s.add (n.name (false, false));
    return s.joinIntoString (" ");
}

static void testScalesAndChords()
{
    const auto n = [] (const char* s) { return *parseNote (s); };
    CHECK (names (scaleNotes (n ("D3"), scaleType ("major"))) == "D E F# G A B C# D", "D major: %s", names (scaleNotes (n ("D3"), scaleType ("major"))).toRawUTF8());
    CHECK (names (scaleNotes (n ("F3"), scaleType ("major"))) == "F G A Bb C D E F", "F major");
    CHECK (names (scaleNotes (n ("Eb3"), scaleType ("major"), false)) == "Eb F G Ab Bb C D", "Eb major");
    CHECK (names (scaleNotes (n ("A3"), scaleType ("harmonicMinor"))) == "A B C D E F G# A", "A harmonic minor");
    CHECK (names (scaleNotes (n ("F#3"), scaleType ("naturalMinor"), false)) == "F# G# A B C# D E", "F# minor");
    CHECK (names (scaleNotes (n ("A3"), scaleType ("blues"), false)) == "A C D Eb E G", "A blues");
    CHECK (stepPattern (scaleNotes (n ("G3"), scaleType ("major"))) == "W W H W W W H", "major steps");
    CHECK (stepPattern (scaleNotes (n ("E3"), scaleType ("naturalMinor"))) == "W H W W H W W", "minor steps");
    CHECK (scaleType ("naturalMinor").isMinor() && ! scaleType ("major").isMinor() && scaleType ("minorPentatonic").isMinor(), "minor flag");
    CHECK (degreeLabel (intervalFromShort ("m3")) == "b3" && degreeLabel (intervalFromShort ("d5")) == "b5" && degreeLabel (intervalFromShort ("A5")) == "#5"
           && degreeLabel (intervalFromShort ("M7")) == "7", "degree labels");

    CHECK (keyFifths (n ("E"), false) == 4 && keyFifths (n ("Eb"), false) == -3 && keyFifths (n ("C"), false) == 0, "major key signatures");
    CHECK (keyFifths (n ("C#"), true) == 4 && keyFifths (n ("F"), true) == -4 && keyFifths (n ("A"), true) == 0, "minor key signatures");
    for (int f = -7; f <= 7; ++f)
    {
        CHECK (keyFifths (keyTonic (f, false), false) == f, "major tonic for %d", f);
        CHECK (keyFifths (keyTonic (f, true), true) == f, "minor tonic for %d", f);
        CHECK (mod12 (keyTonic (f, false).pc() - keyTonic (f, true).pc()) == 3, "relative minor 3 semitones down (%d)", f);
        CHECK ((int) keySignature (f).size() == std::abs (f), "signature size");
    }
    CHECK (names (keySignature (3)) == "F# C# G#" && names (keySignature (-4)) == "Bb Eb Ab Db", "signature order");
    CHECK (keyName (-3, false) == juce::String (juce::CharPointer_UTF8 ("E\xe2\x99\xad major")), "key name");

    CHECK (names (chordNotes (n ("C3"), chordType ("diminished"))) == "C Eb Gb", "C dim");
    CHECK (names (chordNotes (n ("E3"), chordType ("augmented"))) == "E G# B#", "E aug");
    CHECK (names (chordNotes (n ("Bb3"), chordType ("minor"))) == "Bb Db F", "Bb minor");
    const auto c = chordNotes (n ("C3"), chordType ("major"));
    const auto first = invert (c, 1), second = invert (c, 2);
    CHECK (first[0] == n ("E3") && first[2] == n ("C4"), "1st inversion");
    CHECK (second[0] == n ("G3") && second[1] == n ("C4") && second[2] == n ("E4"), "2nd inversion");
    CHECK (chordSymbol (n ("F#"), chordType ("minor")) == juce::String (juce::CharPointer_UTF8 ("F\xe2\x99\xafm")), "symbol");
    for (auto& t : triadTypes())
        CHECK (t.intervals.size() == 3, "triad size");
}

static void testFingerings()
{
    int fingerings = 0;
    for (auto& st : scaleTypes())
        for (int pc = 0; pc < 12; ++pc)
        {
            std::vector<int> pcs;
            for (auto& i : st.intervals) pcs.push_back (mod12 (pc + i.semitones));
            auto inScale = [&] (int midi) { return std::find (pcs.begin(), pcs.end(), mod12 (midi)) != pcs.end(); };

            const auto pos = positionFingerings (pc, st);
            CHECK (pos.size() == 5, "%s on %d: 5 positions (%d)", st.id.toRawUTF8(), pc, (int) pos.size());
            for (auto& fg : pos)
            {
                ++fingerings;
                std::set<int> degrees;
                for (size_t i = 0; i < fg.notes.size(); ++i)
                {
                    const auto& fn = fg.notes[i];
                    CHECK (fn.midi == fretMidi (fn.string, fn.fret) && inScale (fn.midi), "%s: note in scale", fg.name.toRawUTF8());
                    CHECK (pcs[(size_t) fn.degree] == mod12 (fn.midi), "degree index");
                    if (i > 0) CHECK (fn.midi > fg.notes[i - 1].midi, "ascending, no repeats");
                    degrees.insert (fn.degree);
                }
                CHECK (fg.highFret - fg.lowFret <= 4, "%s %s %d: span %d", st.id.toRawUTF8(), fg.name.toRawUTF8(), pc, fg.highFret - fg.lowFret);
                CHECK ((int) degrees.size() == st.size(), "%s %s: every degree", st.id.toRawUTF8(), fg.name.toRawUTF8());
                CHECK (fg.notes.size() >= 10, "%s: covers two octaves (%d notes)", fg.name.toRawUTF8(), (int) fg.notes.size());
            }

            const auto nps = threeNpsFingerings (pc, st);
            CHECK ((int) nps.size() == (st.size() == 7 ? 7 : 0), "3nps count");
            for (auto& fg : nps)
            {
                ++fingerings;
                CHECK (fg.notes.size() == 18, "18 notes");
                for (size_t i = 0; i < fg.notes.size(); ++i)
                {
                    const auto& fn = fg.notes[i];
                    CHECK (fn.string == 6 - (int) i / 3 && fn.fret >= 0 && inScale (fn.midi), "3nps note");
                    if (i % 3 == 2)
                        CHECK (fn.fret - fg.notes[i - 2].fret <= 5, "3nps stretch");
                }
            }
        }
    std::printf ("  %d fingerings checked\n", fingerings);

    // known shapes
    const auto cMajor = positionFingerings (0, scaleType ("major"));
    CHECK (cMajor[0].name.startsWith ("C shape") && cMajor[0].lowFret == 0 && cMajor[0].notes[0].midi == 40, "C major open C shape");
    bool eShape = false;
    for (auto& f : cMajor) eShape = eShape || (f.name.startsWith ("E shape") && f.lowFret == 7 && f.highFret == 10);
    CHECK (eShape, "C major E shape at frets 7-10");
    const auto am = positionFingerings (9, scaleType ("minorPentatonic"));
    CHECK (am[0].name.startsWith ("Box 1") && am[0].lowFret == 5 && am[0].highFret == 8 && am[0].notes.size() == 12, "A minor pentatonic box 1: %s, %d notes",
           am[0].name.toRawUTF8(), (int) am[0].notes.size());

    // runs and drills: right length, never the same note twice in a row
    auto noRepeats = [] (const std::vector<FretNote>& v) { for (size_t i = 1; i < v.size(); ++i) if (v[i].midi == v[i - 1].midi) return false; return true; };
    const auto& fg = cMajor[3];
    const int nNotes = (int) fg.notes.size();
    CHECK ((int) scaleRun (fg, RunPattern::upDown).size() == 2 * nNotes - 1, "up and down");
    CHECK ((int) scaleRun (fg, RunPattern::threes).size() == 3 * (2 * nNotes - 5), "threes");
    CHECK ((int) scaleRun (fg, RunPattern::fours).size() == 4 * (2 * nNotes - 7), "fours");
    CHECK ((int) scaleRun (fg, RunPattern::thirds).size() == 2 * (2 * nNotes - 5), "thirds");
    for (int p = 0; p < 5; ++p)
        CHECK (noRepeats (scaleRun (fg, (RunPattern) p)), "run %d has no repeated notes", p);
    for (int d = 0; d < 3; ++d)
        for (int f = 0; f <= 12; ++f)
        {
            const auto drill = techniqueDrill ((TechDrill) d, f);
            CHECK ((int) drill.size() == (d == 2 ? 63 : 47) && noRepeats (drill), "technique drill %d from fret %d", d, f);
        }
}

static void testRunScore()
{
    std::vector<double> even;
    for (int i = 0; i < 24; ++i) even.push_back (0.3 * i);   // eighth notes at 100 BPM
    auto s = scoreRun (even, 0, 100.0, 2);
    CHECK (s.clean && s.evenness > 0.99 && std::abs (s.averageBpm - 100.0) < 0.5, "even run: %.2f, %.1f BPM", s.evenness, s.averageBpm);
    s = scoreRun (even, 0, 0.0, 2);
    CHECK (s.clean, "even run without a metronome");
    s = scoreRun (even, 3, 100.0, 2);
    CHECK (! s.clean && s.accuracy < 0.9, "three wrong notes: not clean (%.2f)", s.accuracy);
    s = scoreRun (even, 0, 120.0, 2);
    CHECK (! s.clean, "too slow for the metronome");
    std::vector<double> ragged;
    double t = 0;
    for (int i = 0; i < 24; ++i) { ragged.push_back (t); t += (i % 2 == 0) ? 0.15 : 0.45; }
    s = scoreRun (ragged, 0, 100.0, 2);
    CHECK (! s.clean && s.evenness < 0.75, "uneven run: %.2f", s.evenness);
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
        p.recordTempo ("tech/1/2", 80.0);
        p.recordTempo ("tech/1/2", 75.0);
    }
    Progress p (file);
    CHECK (std::abs (p.bestScore ("L1/0") - 0.9) < 1e-9, "best kept: %f", p.bestScore ("L1/0"));
    CHECK (p.attempts ("L1/0") == 3, "attempts");
    CHECK (p.passed ("L1/0"), "passed");
    CHECK (std::abs (p.bestDrill ("noteFinder") - 4.0) < 1e-9, "drill best");
    CHECK (std::abs (p.bestTempo ("tech/1/2") - 80.0) < 1e-9 && p.bestTempo ("other") == 0.0, "tempo best");
    CHECK (p.streakDays() == 1, "streak today");
    file.deleteFile();
}

int main()
{
    std::printf ("theory\n");    testTheory();
    std::printf ("scales\n");    testScalesAndChords();
    std::printf ("fingerings\n"); testFingerings();
    std::printf ("runs\n");      testRunScore();
    std::printf ("lessons\n");   testLessons();
    std::printf ("pitch\n");     testPitch();
    std::printf ("progress\n");  testProgress();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
