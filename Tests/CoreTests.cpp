// GuitarCoachTests: theory engine, lesson generators, pitch/note detection on synthetic guitar notes, progress file.
#include "core/Theory.h"
#include "core/Lessons.h"
#include "core/InputAnalyzer.h"
#include "core/GuitarVoice.h"
#include "core/Progress.h"
#include "core/Fingerings.h"
#include "core/BackingTrack.h"
#include "core/Licks.h"
#include "core/Rhythm.h"
#include "core/Planner.h"
#include "core/MonitorTone.h"
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
    CHECK (lessons.size() == 33, "33 lessons, got %d", (int) lessons.size());
    int blues = 0;
    for (auto& l : lessons) blues += l.track == "blues" ? 1 : 0;
    CHECK (blues == 9, "9 blues lessons (%d)", blues);
    std::set<juce::String> ids;
    for (auto& l : lessons)
        CHECK (ids.insert (l.id).second && l.level >= 1 && l.level <= 4 && (l.track == "theory" || l.track == "blues") && ! l.exercises.empty(), "%s: unique id, level, exercises", l.id.toRawUTF8());
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
                        CHECK ((int) q.sequence.size() == scaleType (ex.params.getProperty ("scale", "major").toString()).size() + 1
                               && q.sequence.front().pc() == q.sequence.back().pc(), "%s: root to root", what);
                    if (ex.type == "spellTriad" || ex.type == "playTriad")
                        CHECK (q.sequence.size() == 3 || q.sequence.size() == 4, "%s: chord tones", what);
                    if (ex.type.startsWith ("spell") || ex.type.startsWith ("play") || ex.type == "writeScale")
                        for (size_t k = 1; k < q.sequence.size(); ++k)
                            CHECK (q.sequence[k].midi() > q.sequence[k - 1].midi() || (ex.type == "playCell" && q.sequence[k].midi() != q.sequence[k - 1].midi()),
                                   "%s: in order", what);
                    for (size_t k = 1; k < q.sequence.size(); ++k)
                        CHECK (q.sequence[k].midi() != q.sequence[k - 1].midi(), "%s: no repeated note (the detector needs a change)", what);
                }
                if (q.tapRhythm)
                    CHECK (q.rhythm.size() >= 3 && q.rhythmBeats > 0 && q.listenTimes.size() == q.listenMidi.size() && q.tapBpm > 30, "%s: tap question", what);
                if (! q.listenChords.empty())
                    for (auto& c : q.listenChords)
                        CHECK (c.size() >= 3 && c.front() >= 30 && c.back() <= 84, "%s: chord voicing", what);
                if (ex.type == "diatonicChord" || ex.type == "progressionName" || ex.type == "chordFunction")
                    CHECK (q.choices.size() >= 3 && q.choices.size() == juce::StringArray (q.choices).size(), "%s: choices", what);
                if (q.showKeySignature)
                    CHECK (q.choices[q.correctChoice] == keyName (q.keySignature, (bool) ex.params.getProperty ("minor", false)), "%s: key matches", what);
                if (ex.type == "countKeySig" || ex.type == "circle" || ex.type == "earScale" || ex.type == "triadQuality" || ex.type == "earTriad")
                    CHECK (q.choices.size() >= 2 && q.choices.size() == juce::StringArray (q.choices).size(), "%s: choices", what);
                if (ex.type == "triadQuality" || ex.type == "inversionName")
                    CHECK (q.staffChord && q.staffNotes.size() >= 3, "%s: a chord on the staff", what);
                if (ex.type == "inversionName")
                {
                    CHECK (q.staffNotes.front().midi() >= 48 - 12 && q.staffNotes.back().midi() <= 69, "%s: fits the staff", what);
                    std::set<int> chordPcs;
                    for (auto& sn : q.staffNotes) chordPcs.insert (sn.pc());
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
            int expected = 5;
            if (st.size() < 7)
            {
                expected = 0;
                for (auto& iv : st.intervals) expected += (iv.quality == "d" || iv.quality == "A") ? 0 : 1;
            }
            CHECK ((int) pos.size() == expected, "%s on %d: %d positions (%d)", st.id.toRawUTF8(), pc, expected, (int) pos.size());
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
                CHECK ((int) fg.notes.size() >= 2 * st.size(), "%s %s: covers two octaves (%d notes)", st.id.toRawUTF8(), fg.name.toRawUTF8(), (int) fg.notes.size());
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

static void testBackingTrack()
{
    // every form and feel: plays, stays in range, follows the bars
    for (int form = 0; form < (int) bluesForms().size(); ++form)
        for (int feel = 0; feel < 4; ++feel)
        {
            BackingTrack t;
            t.prepare (48000.0);
            BackingSettings s;
            s.form = form;
            s.feel = feel;
            s.bpm = 120.0;          // a bar is 2 seconds
            s.keyPc = (form * 5 + feel) % 12;
            t.setSettings (s);
            t.start();
            std::vector<float> buf (480);
            double sumSq = 0, peak = 0;
            bool finite = true;
            int barAt3s = -9, barAt27s = -9, chorusAt27s = -9;
            const int blocks = 48000 * 28 / 480;
            for (int b = 0; b < blocks; ++b)
            {
                std::fill (buf.begin(), buf.end(), 0.0f);
                t.render (buf.data(), (int) buf.size());
                for (auto x : buf)
                {
                    finite = finite && std::isfinite (x);
                    sumSq += (double) x * x;
                    peak = std::max (peak, (double) std::abs (x));
                }
                const double secs = (b + 1) * 480 / 48000.0;
                if (std::abs (secs - 1.0) < 0.005) CHECK (t.getBar() == -1, "count-in first (bar %d)", t.getBar());
                if (std::abs (secs - 3.0) < 0.005) barAt3s = t.getBar();
                if (std::abs (secs - 27.0) < 0.005) { barAt27s = t.getBar(); chorusAt27s = t.getChorus(); }
            }
            const double rms = std::sqrt (sumSq / (blocks * 480.0));
            CHECK (finite && peak < 1.5 && rms > 0.02, "form %d feel %d: rms %.3f peak %.2f", form, feel, rms, peak);
            CHECK (barAt3s == 0 && barAt27s == 0 && chorusAt27s == 1, "form %d feel %d: bars %d %d, chorus %d", form, feel, barAt3s, barAt27s, chorusAt27s);
        }

    // a two-bar excerpt stops by itself; muted parts make no sound
    {
        BackingTrack t;
        t.prepare (48000.0);
        BackingSettings s;
        s.countIn = false;
        s.stopAfterBars = 2;
        s.bpm = 120.0;
        t.setSettings (s);
        t.start();
        std::vector<float> buf (480);
        for (int b = 0; b < 100 * 5; ++b) { std::fill (buf.begin(), buf.end(), 0.0f); t.render (buf.data(), 480); }
        CHECK (! t.isPlaying(), "excerpt stopped after 2 bars");

        BackingTrack q;
        q.prepare (48000.0);
        s.stopAfterBars = 0;
        s.drums = s.bass = s.guitar = s.keys = false;
        q.setSettings (s);
        q.start();
        double sum = 0;
        for (int b = 0; b < 400; ++b) { std::fill (buf.begin(), buf.end(), 0.0f); q.render (buf.data(), 480); for (auto x : buf) sum += std::abs (x); }
        CHECK (sum < 1.0e-3, "all parts muted: silence (%.4f)", sum);
    }

    CHECK (bluesChordName (9, { 5, "7" }) == "D7" && bluesChordName (4, { 7, "7" }) == "B7", "chord names");
    CHECK (bluesChordName (5, { 6, "dim7" }) == "Bdim7" && bluesChordName (9, { 8, "7" }) == "F7", "jazz and minor names");
    CHECK (bluesRoman ({ 5, "7" }, false) == "IV7" && bluesRoman ({ 8, "7" }, true) == "bVI7", "numerals");
    for (auto& f : bluesForms())
        CHECK (f.bars.size() == 12, "%s has 12 bars", f.id.toRawUTF8());
}

static void testLicks()
{
    const auto licks = loadLicks();
    CHECK (licks.size() >= 12, "%d licks", (int) licks.size());
    std::set<juce::String> ids;
    for (auto& l : licks)
    {
        const auto what = l.id.toRawUTF8();
        CHECK (ids.insert (l.id).second && l.name.isNotEmpty() && l.description.isNotEmpty() && l.notes.size() >= 3, "%s: complete", what);
        double last = -1;
        for (size_t i = 0; i < l.notes.size(); ++i)
        {
            const auto& n = l.notes[i];
            CHECK (n.string >= 1 && n.string <= 6 && n.fret >= 0 && n.fret <= 17 && n.beat > last && n.beat + n.length <= l.beats + 1.0e-6, "%s: note %d", what, (int) i);
            CHECK ((n.tech != "b" && n.tech != "r") || n.bend == 1 || n.bend == 2, "%s: bend size", what);
            if (i > 0)
                CHECK (n.targetMidi() != l.notes[i - 1].targetMidi(), "%s: notes %d and %d are different pitches (the detector needs that)", what, (int) i - 1, (int) i);
            last = n.beat;
        }
        // in every key it stays on the neck, and moves by the right amount
        for (int pc = 0; pc < 12; ++pc)
        {
            const auto t = transposeLick (l, pc);
            for (size_t i = 0; i < t.notes.size(); ++i)
            {
                CHECK (t.notes[i].fret >= 0 && t.notes[i].fret <= 17, "%s in %d: fret %d", what, pc, t.notes[i].fret);
                CHECK (mod12 (t.notes[i].targetMidi() - l.notes[i].targetMidi() - (pc - 9)) == 0, "%s: transposed pitch", what);
            }
        }
        // played perfectly (with the fretted note heard before each bend), the follower finishes with no mistakes
        LickFollower f;
        f.reset (l);
        double time = 0;
        for (auto& n : l.notes)
        {
            if (n.tech == "b") CHECK (f.feed (n.fretMidi(), time) == LickFollower::Result::ignored, "%s: pre-bend note forgiven", what);
            f.feed (n.targetMidi(), time += 0.3);
        }
        CHECK (f.finished() && f.mistakes() == 0, "%s: clean run (%d mistakes)", what, f.mistakes());
    }

    // slides and wrong notes
    Lick slide;
    slide.notes = { { 3, 5, 0, 0.5 }, { 3, 9, 0.5, 1, 0, "s" }, { 2, 8, 1.5, 0.5 } };
    LickFollower f;
    f.reset (slide);
    f.feed (60, 0.0);
    CHECK (f.feed (62, 0.1) == LickFollower::Result::ignored, "slid-through note forgiven");
    CHECK (f.feed (64, 0.2) == LickFollower::Result::hit, "slide target");
    CHECK (f.feed (70, 0.4) == LickFollower::Result::wrong && f.mistakes() == 1, "wrong note counted");
    CHECK (f.feed (67, 0.6) == LickFollower::Result::finished, "finished");

    // rhythm: exact at any tempo scores 1; random timing scores low
    const std::vector<double> written { 0, 0.5, 1, 1.5, 2, 2.5, 3 };
    std::vector<double> exact, slowExact, sloppy;
    juce::Random r (5);
    for (auto b : written) { exact.push_back (0.25 + b * 0.5); slowExact.push_back (1.0 + b * 0.9); sloppy.push_back (b * 0.5 + (r.nextFloat() - 0.5) * 0.4); }
    CHECK (rhythmScore (written, exact) > 0.99 && rhythmScore (written, slowExact) > 0.99, "exact rhythm");
    CHECK (rhythmScore (written, sloppy) < 0.7, "sloppy rhythm: %.2f", rhythmScore (written, sloppy));
    CHECK (std::abs (swingBeat (1.5, true) - (1.0 + 2.0 / 3.0)) < 1.0e-9 && swingBeat (1.5, false) == 1.5 && swingBeat (2.0, true) == 2.0, "swing");

    // a bent note played by the synth is heard at its target pitch
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        GuitarVoice v;
        v.prepare (48000.0);
        v.start (62, 0.9f, 48000);
        v.bend (2.0, 4800, 4800);   // after 0.1 s, a whole step over 0.1 s
        std::vector<float> block (128);
        std::vector<int> heard;
        for (int b = 0; b < 48000 / 128; ++b)
        {
            std::fill (block.begin(), block.end(), 0.0f);
            v.render (block.data(), 128);
            for (auto& x : block) x *= 0.3f;
            a.push (block.data(), 128);
            NoteEvent e;
            bool got = false;
            while (a.analyze (e, got)) if (got) heard.push_back (e.midi);
        }
        CHECK (! heard.empty() && heard.back() == 64, "bend D3 to E3 heard as E3 (last %d, %d events)", heard.empty() ? -1 : heard.back(), (int) heard.size());
    }

    CHECK (chordNotes (*parseNote ("A3"), chordType ("dominant7")).size() == 4 && names (chordNotes (*parseNote ("A3"), chordType ("dominant7"))) == "A C# E G", "A7");
    CHECK (names (chordNotes (*parseNote ("B3"), chordType ("halfDiminished"))) == "B D F A", "Bm7b5");
    CHECK (names (chordNotes (*parseNote ("C3"), chordType ("diminished7"))) == "C Eb Gb Bbb", "Cdim7");
    CHECK (names (scaleNotes (*parseNote ("A3"), scaleType ("mixolydian"), false)) == "A B C# D E F# G", "A Mixolydian");
}

static void testHarmony()
{
    const auto n = [] (const char* s) { return *parseNote (s); };
    juce::StringArray cTriads, ebSevenths, gNumerals;
    for (int d = 0; d < 7; ++d)
    {
        const auto c = diatonicChord (n ("C3"), d, false);
        CHECK (c.type != nullptr, "C major degree %d identified", d);
        if (c.type != nullptr) cTriads.add (chordSymbol (c.root, *c.type));
        const auto e = diatonicChord (n ("Eb3"), d, true);
        CHECK (e.type != nullptr && e.notes.size() == 4, "Eb major 7th on degree %d", d);
        if (e.type != nullptr) ebSevenths.add (chordSymbol (e.root, *e.type));
        const auto g = diatonicChord (n ("G3"), d, false);
        if (g.type != nullptr) gNumerals.add (romanNumeral (d, *g.type));
    }
    const auto deg = juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
    const auto flat = juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad"));
    CHECK (cTriads.joinIntoString (" ") == "C Dm Em F G Am B" + deg, "C major triads: %s", cTriads.joinIntoString (" ").toRawUTF8());
    CHECK (ebSevenths.joinIntoString (" ") == "E" + flat + "maj7 Fm7 Gm7 A" + flat + "maj7 B" + flat + "7 Cm7 Dm7" + flat + "5",
           "Eb major sevenths: %s", ebSevenths.joinIntoString (" ").toRawUTF8());
    CHECK (gNumerals.joinIntoString (" ") == "I ii iii IV V vi vii" + deg, "numerals: %s", gNumerals.joinIntoString (" ").toRawUTF8());
    CHECK (romanNumeral (4, chordType ("dominant7")) == "V7" && romanNumeral (1, chordType ("minor7")) == "IIm7", "seventh numerals");
    CHECK (harmonicFunction (0) == "tonic" && harmonicFunction (3) == "subdominant" && harmonicFunction (4) == "dominant" && harmonicFunction (5) == "tonic", "functions");
    CHECK (identifyChord (chordNotes (n ("F#3"), chordType ("halfDiminished"))) == &chordType ("halfDiminished"), "identify half-diminished");
    for (auto& p : commonProgressions())
        for (int d : p.degrees)
            CHECK (d >= 0 && d < 7, "%s degrees", p.name.toRawUTF8());
}

static void testRhythm()
{
    juce::Random rng (9);
    const std::vector<double> allowed { 0.25, 1.0 / 3.0, 0.5, 0.75, 1.0, 1.5, 2.0 };
    for (int level = 1; level <= 3; ++level)
        for (int i = 0; i < 300; ++i)
        {
            const auto r = randomRhythm (level, 8, rng);
            bool ok = ! r.empty() && r.front().beat == 0.0;
            for (size_t k = 0; k < r.size(); ++k)
            {
                ok = ok && r[k].beat + r[k].length <= 8.0 + 1.0e-6;
                if (k > 0) ok = ok && r[k].beat >= r[k - 1].beat + r[k - 1].length - 1.0e-6;
                bool known = false;
                for (auto a : allowed) known = known || std::abs (a - r[k].length) < 0.01;
                ok = ok && known && (level >= 3 || r[k].length >= 0.5);
            }
            CHECK (ok && r.size() >= 4, "random rhythm level %d", level);
        }
    for (auto* cells : { &threePulseCells(), &fourPulseCells() })
        for (auto& c : *cells)
        {
            double total = 0;
            for (auto& e : cellEvents (c, 2.0)) { total += e.length; CHECK (e.beat >= 2.0 && e.beat < 3.0, "%s inside its beat", c.id.toRawUTF8()); }
            CHECK (std::abs (total + cellEvents (c, 0).front().beat - 1.0) < 1.0e-9, "%s fills the beat", c.id.toRawUTF8());
        }

    // taps: right but late by any amount scores 1; a missing tap and sloppy taps fail
    const auto written = randomRhythm (2, 8, rng);
    std::vector<double> exact, missing, sloppy;
    juce::Random j (4);
    for (auto& e : written)
    {
        exact.push_back (3.137 + e.beat * 0.75);   // 80 BPM
        sloppy.push_back (3.0 + (e.beat + (j.nextDouble() - 0.5) * 0.9) * 0.75);
    }
    missing = exact;
    missing.pop_back();
    CHECK (scoreTaps (written, exact, 80.0).score > 0.99, "exact taps");
    const auto m = scoreTaps (written, missing, 80.0);
    CHECK (m.missing == 1 && m.score < 1.0, "a missing tap counts");
    CHECK (scoreTaps (written, sloppy, 80.0).score < 0.75, "sloppy taps fail: %.2f", scoreTaps (written, sloppy, 80.0).score);
    CHECK (scoreTaps (written, exact, 100.0).score < 0.75, "right rhythm at the wrong tempo fails");

    // onsets: plucked notes in eighths at 120 BPM, then muted strums (noise bursts)
    {
        InputAnalyzer a;
        a.prepare (48000.0);
        GuitarVoice v;
        v.prepare (48000.0);
        std::vector<float> block (128);
        int onsets = 0;
        auto run = [&] (double seconds, bool noise, juce::Random* r)
        {
            for (int b = 0; b < (int) (seconds * 48000 / 128); ++b)
            {
                std::fill (block.begin(), block.end(), 0.0f);
                v.render (block.data(), 128);
                for (auto& x : block) x *= 0.3f;
                if (noise && r != nullptr && b < 12)
                    for (auto& x : block) x += (r->nextFloat() - 0.5f) * 0.5f * (1.0f - (float) b / 12.0f);
                a.push (block.data(), 128);
                NoteEvent e;
                bool got = false;
                while (a.analyze (e, got)) {}
                double t;
                while (a.popOnset (t)) ++onsets;
            }
        };
        const int notes[] = { 45, 48, 50, 52, 55, 57, 55, 52 };
        for (int nn : notes) { v.start (nn, 0.9f, 48000 / 4); run (0.25, false, nullptr); }
        CHECK (onsets == 8, "8 plucked eighths give %d onsets", onsets);
        v.release();
        run (0.5, false, nullptr);
        onsets = 0;
        juce::Random r (2);
        for (int k = 0; k < 6; ++k) run (0.3, true, &r);
        CHECK (onsets == 6, "6 muted strums give %d onsets", onsets);
    }
}


static void testLevel4()
{
    const auto n = [] (const char* s) { return *parseNote (s); };
    CHECK (names (scaleNotes (n ("D3"), scaleType ("dorian"), false)) == "D E F G A B C", "D Dorian");
    CHECK (names (scaleNotes (n ("E3"), scaleType ("phrygian"), false)) == "E F G A B C D", "E Phrygian");
    CHECK (names (scaleNotes (n ("F3"), scaleType ("lydian"), false)) == "F G A B C D E", "F Lydian");
    CHECK (names (scaleNotes (n ("B3"), scaleType ("locrian"), false)) == "B C D E F G A", "B Locrian");
    // each mode has the notes of its parent major scale
    for (auto& m : majorModes())
        for (auto* key : { "C", "G", "F", "Bb", "D" })
        {
            auto tonic = n (key);
            tonic.octave = 3;
            auto root = scaleNotes (tonic, scaleType ("major"), false)[(size_t) m.degree];
            std::set<int> a, b;
            for (auto& x : scaleNotes (tonic, scaleType ("major"), false)) a.insert (x.pc());
            for (auto& x : scaleNotes (root, scaleType (m.scaleId), false)) b.insert (x.pc());
            CHECK (a == b && mod12 (root.pc() + scaleType (m.scaleId).parent) == tonic.pc(), "%s from %s", m.name.toRawUTF8(), key);
        }
    const auto c = n ("C3");
    juce::StringArray sec;
    for (int d = 1; d <= 5; ++d) { const auto x = secondaryDominant (c, d); sec.add (chordSymbol (x.root, *x.type)); }
    CHECK (sec.joinIntoString (" ") == "A7 B7 C7 D7 E7", "secondary dominants in C: %s", sec.joinIntoString (" ").toRawUTF8());
    juce::StringArray bor;
    for (auto& b : borrowedChords (c)) bor.add (chordSymbol (b.root, *b.type));
    const auto flat = juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad"));
    CHECK (bor.joinIntoString (" ") == "E" + flat + " Fm A" + flat + " B" + flat, "borrowed chords in C: %s", bor.joinIntoString (" ").toRawUTF8());
    CHECK (tritoneSubstitute (n ("G3")).name (false, false) == "Db" && tritoneSubstitute (n ("E3")).name (false, false) == "Bb"
           && tritoneSubstitute (n ("F3")).name (false, false) == "B", "tritone substitutes");
    for (auto& sh : octaveShapes())
        for (int f = 3; f <= 9; ++f)
            CHECK (fretMidi (sh.highString, f + sh.frets) == fretMidi (sh.lowString, f) + 12, "%s shape octave", sh.name.toRawUTF8());
    for (auto& tn : dominantTensions())
        CHECK (intervalFromShort (tn.interval).semitones > 12, "%s is above the octave", tn.label.toRawUTF8());
}


static void testPlanner()
{
    auto file = juce::File::getSpecialLocation (juce::File::tempDirectory).getChildFile ("guitarcoach-test-plan.json");
    file.deleteFile();
    Progress p (file);
    p.setToday ("2026-11-16");
    CHECK (Progress::addDays ("2026-11-30", 2) == "2026-12-02" && Progress::daysBetween ("2026-11-16", "2026-12-16") == 30, "date arithmetic");

    // spaced repetition: pass -> box 0, due tomorrow; pass again next day -> box 1, 3 days; fail -> box 0, tomorrow
    p.recordExercise ("L1-staff/0", 9, 10);
    CHECK (p.box ("L1-staff/0") == 0 && p.dueDate ("L1-staff/0") == "2026-11-17" && ! p.isDue ("L1-staff/0"), "first pass");
    p.recordExercise ("L1-staff/0", 10, 10);
    CHECK (p.box ("L1-staff/0") == 0, "a second pass the same day does not move it up");
    p.setToday ("2026-11-17");
    CHECK (p.isDue ("L1-staff/0") && p.dueKeys().contains ("L1-staff/0"), "due the next day");
    p.recordExercise ("L1-staff/0", 8, 10);
    CHECK (p.box ("L1-staff/0") == 1 && p.dueDate ("L1-staff/0") == "2026-11-20", "box 1: 3 days");
    p.setToday ("2026-11-20");
    p.recordExercise ("L1-staff/0", 5, 10);
    CHECK (p.box ("L1-staff/0") == 0 && p.dueDate ("L1-staff/0") == "2026-11-21" && p.passed ("L1-staff/0"), "a slip sends it back to box 0");
    p.recordExercise ("L1-staff/1", 4, 10);
    CHECK (p.dueDate ("L1-staff/1").isEmpty(), "never passed: not reviewed");
    const auto d = p.day ("2026-11-20");
    CHECK (d.exercises == 2 && d.correct == 9 && d.questions == 20, "history of the day");
    p.addPracticeSeconds (95.0);
    CHECK (std::abs (p.day ("2026-11-20").seconds - 95.0) < 1e-6 && p.totalPracticeSeconds() >= 95.0, "practice time");
    p.recordRun ("tech");
    CHECK (p.day ("2026-11-20").techRuns == 1, "technique run counted");

    // the plan
    const auto all = loadLessons();
    std::vector<Lesson> theory, blues;
    for (auto& l : all) (l.track == "blues" ? blues : theory).push_back (l);
    const auto licks = loadLicks();
    const PlanContext ctx { theory, blues, licks };

    Progress fresh (juce::File{});
    fresh.setToday ("2026-11-16");
    const auto plan = makePlan (ctx, fresh, 45);
    juce::StringArray kinds;
    for (auto& item : plan) kinds.add (juce::String ((int) item.kind));
    CHECK (plan.size() >= 5 && plan.front().kind == PlanItem::Kind::warmup, "45-minute plan: %s", kinds.joinIntoString (",").toRawUTF8());
    bool hasNew = false, hasBlues = false, hasLick = false;
    for (auto& item : plan)
    {
        hasNew = hasNew || (item.kind == PlanItem::Kind::lesson && item.lessonId == "L1-staff" && item.exercise == 0);
        hasBlues = hasBlues || (item.kind == PlanItem::Kind::blues && item.lessonId == "B1-form");
        hasLick = hasLick || (item.kind == PlanItem::Kind::improv && item.lickId == licks.front().id);
        CHECK (! isDone (item, fresh) && item.title.isNotEmpty() && item.minutes > 0, "%s: open and complete", item.title.toRawUTF8());
        const auto back = PlanItem::fromVar (item.toVar());
        CHECK (back.title == item.title && back.kind == item.kind && back.exercise == item.exercise && back.lickId == item.lickId, "plan item round trip");
    }
    CHECK (hasNew && hasBlues && hasLick, "new exercise, blues lesson and lick in the plan");
    int total30 = 0, total60 = 0;
    for (auto& item : makePlan (ctx, fresh, 30)) total30 += item.minutes;
    for (auto& item : makePlan (ctx, fresh, 60)) total60 += item.minutes;
    CHECK (total30 >= 20 && total30 <= 35 && total60 >= 45 && total60 <= 70, "lengths: %d and %d minutes", total30, total60);

    // reviews show up when due, and the plan is kept for the day
    p.setToday ("2026-11-21");
    const auto withReview = todaysPlan (ctx, p, 45);
    bool review = false;
    for (auto& item : withReview) review = review || (item.kind == PlanItem::Kind::review && item.lessonId == "L1-staff" && item.exercise == 0);
    CHECK (review, "a due exercise is reviewed");
    p.recordExercise ("L1-staff/0", 10, 10);
    const auto again = todaysPlan (ctx, p, 45);
    CHECK (again.size() == withReview.size(), "the same plan all day");
    for (auto& item : again)
        if (item.kind == PlanItem::Kind::review && item.lessonId == "L1-staff")
            CHECK (isDone (item, p), "the review is done once practised");
    file.deleteFile();
}

static void testMonitor()
{
    // the clean-amp tone: about the same level as dry for a normal guitar signal, bounded for a loud one
    for (float amp : { 0.1f, 0.25f, 1.0f })
    {
        MonitorTone t;
        t.prepare (48000.0);
        double inSq = 0, outSq = 0, peak = 0;
        bool finite = true;
        for (int i = 0; i < 48000; ++i)
        {
            const float x = amp * (float) std::sin (2 * 3.14159265 * 220.0 * i / 48000.0);
            const float y = t.process (x, true);
            finite = finite && std::isfinite (y);
            if (i > 4800) { inSq += x * x; outSq += y * y; peak = std::max (peak, (double) std::abs (y)); }
        }
        const double gainDb = 10.0 * std::log10 (outSq / inSq);
        CHECK (finite && peak < 1.2 && (amp > 0.5 || std::abs (gainDb) < 4.0), "tone at %.2f: %.1f dB, peak %.2f", amp, gainDb, peak);
    }
    MonitorTone d;
    d.prepare (48000.0);
    CHECK (d.process (0.3f, false) == 0.3f, "dry is untouched");
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
    std::printf ("backing\n");   testBackingTrack();
    std::printf ("licks\n");     testLicks();
    std::printf ("harmony\n");   testHarmony();
    std::printf ("rhythm\n");    testRhythm();
    std::printf ("level 4\n");   testLevel4();
    std::printf ("planner\n");   testPlanner();
    std::printf ("monitor\n");   testMonitor();
    std::printf ("lessons\n");   testLessons();
    std::printf ("pitch\n");     testPitch();
    std::printf ("progress\n");  testProgress();
    std::printf ("%d checks, %d failed\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
