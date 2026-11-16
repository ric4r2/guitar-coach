#include "Lessons.h"
#include "BinaryData.h"

using namespace theory;

std::vector<Lesson> parseLessons (const juce::String& json)
{
    std::vector<Lesson> out;
    const auto root = juce::JSON::parse (json);
    if (auto* arr = root["lessons"].getArray())
    {
        for (const auto& l : *arr)
        {
            Lesson lesson;
            lesson.id = l["id"].toString();
            lesson.level = (int) l["level"];
            lesson.strand = l["strand"].toString();
            lesson.title = l["title"].toString();
            lesson.summary = l["summary"].toString();
            lesson.diagram = l["diagram"];
            if (auto* body = l["body"].getArray())
                for (const auto& line : *body)
                    lesson.body.add (line.toString());
            if (auto* ex = l["exercises"].getArray())
                for (const auto& e : *ex)
                    lesson.exercises.push_back ({ e["type"].toString(), e["title"].toString(), e });
            out.push_back (std::move (lesson));
        }
    }
    return out;
}

std::vector<Lesson> loadLessons()
{
    return parseLessons (juce::String::fromUTF8 (BinaryData::lessons_json, BinaryData::lessons_jsonSize));
}

juce::StringArray naturalChoices() { return { "C", "D", "E", "F", "G", "A", "B" }; }

juce::StringArray accidentalChoices()
{
    juce::StringArray s;
    for (auto n : { "C", "C#", "Db", "D", "D#", "Eb", "E", "F", "F#", "Gb", "G", "G#", "Ab", "A", "A#", "Bb", "B" })
        s.add (parseNote (n)->name());
    return s;
}

juce::String Question::wrongPlayHint (int playedMidi) const
{
    juce::String s = "You played " + noteFromMidi (playedMidi).name (true);
    if (expectedMidi >= 0 && mod12 (playedMidi) == mod12 (expectedMidi) && playedMidi != expectedMidi)
        s << ": right note, wrong octave";
    return s + ".";
}

namespace
{
    juce::String noteWithPlace (const Note& n)
    {
        auto pos = positionsOf (n.midi(), 12);
        juce::String where;
        if (! pos.empty())
        {
            // the lowest fret is the open-position fingering
            auto best = pos.front();
            for (auto& p : pos)
                if (p.fret < best.fret)
                    best = p;
            where = " (" + stringName (best.string) + ", " + (best.fret == 0 ? juce::String ("open") : "fret " + juce::String (best.fret)) + ")";
        }
        return n.name (true) + where;
    }

    // A random note between lo and hi (sounding pitches), naturals only or with one sharp/flat.
    Note randomNote (juce::Random& rng, const Note& lo, const Note& hi, bool accidentals)
    {
        for (;;)
        {
            const int step = lo.diatonic() + rng.nextInt (hi.diatonic() - lo.diatonic() + 1);
            Note n { step % 7, 0, step / 7 };
            if (accidentals && rng.nextInt (3) != 0)
            {
                n.acc = rng.nextBool() ? 1 : -1;
                // skip E#, Fb, B#, Cb: correct, but not for level 1
                if ((n.acc == 1 && (n.letter == 2 || n.letter == 6)) || (n.acc == -1 && (n.letter == 3 || n.letter == 0)))
                    continue;
            }
            if (n.midi() >= lo.midi() && n.midi() <= hi.midi())
                return n;
        }
    }

    std::vector<int> intArray (const juce::var& v, std::vector<int> fallback)
    {
        if (auto* a = v.getArray())
        {
            std::vector<int> out;
            for (auto& x : *a)
                out.push_back ((int) x);
            if (! out.empty())
                return out;
        }
        return fallback;
    }

    std::vector<Interval> intervalSet (const juce::var& v)
    {
        std::vector<Interval> out;
        if (auto* a = v.getArray())
            for (auto& x : *a)
                out.push_back (intervalFromShort (x.toString()));
        if (out.empty())
            out = octaveIntervals();
        return out;
    }

    Note spec2note (const ExerciseSpec& s, const char* key, const char* fallback)
    {
        auto n = parseNote (s.params[key].toString());
        return n ? *n : *parseNote (fallback);
    }

    // Choices for a note name: the 7 letters or the 17 common spellings.
    void noteChoices (Question& q, const Note& answer, bool accidentals)
    {
        q.choices = accidentals ? accidentalChoices() : naturalChoices();
        q.correctChoice = q.choices.indexOf (answer.name());
        q.choiceColumns = accidentals ? 9 : 7;
    }

    // a root that leaves room for an octave above it, on strings 6 to 4, frets 0 to 7
    Note randomRoot (juce::Random& rng)
    {
        static const int strings[] = { 6, 5, 4 };
        const int s = strings[rng.nextInt (3)];
        const int f = rng.nextInt (8);
        // black keys as the common key names: Db Eb F# Ab Bb
        const int midi = fretMidi (s, f);
        return noteFromMidi (midi, mod12 (midi) != 6);
    }

    FretPos lowestPosition (int midi, int maxFret, int preferString = 0)
    {
        auto pos = positionsOf (midi, maxFret);
        if (pos.empty())
            return { 6, 0 };
        for (auto& p : pos)
            if (p.string == preferString)
                return p;
        FretPos best = pos.front();
        for (auto& p : pos)
            if (p.fret < best.fret)
                best = p;
        return best;
    }
}

Question makeQuestion (const ExerciseSpec& spec, juce::Random& rng)
{
    Question q;
    const auto& t = spec.type;
    const bool acc = (bool) spec.params.getProperty ("accidentals", false);

    if (t == "nameStaff" || t == "playStaff" || t == "writeStaff")
    {
        const auto lo = spec2note (spec, "lo", "E3"), hi = spec2note (spec, "hi", "F4");
        const auto n = randomNote (rng, lo, hi, acc);
        const int midi = n.midi();
        q.expectedMidi = midi;
        q.explanation = "It is " + noteWithPlace (n) + ".";
        const auto pos = lowestPosition (midi, 12);
        q.revealMarks = { { pos.string, pos.fret, n.name(), 2 } };

        if (t == "writeStaff")
        {
            q.prompt = "Write " + noteWithPlace (n) + " on the staff." + (acc ? " Pick the sharp or flat first." : "");
            q.staffClick = true;
            q.staffAccidentalButtons = acc;
            q.judgeStaff = [n] (const Note& placed) { return placed.letter == n.letter && placed.acc == n.acc && placed.octave == n.octave; };
            q.revealStaff = { n };
        }
        else
        {
            q.staffNotes = { n };
            q.acceptsPlaying = true;
            q.judgePlayed = [midi] (int m) { return m == midi; };
            if (t == "nameStaff")
            {
                q.prompt = "What is this note? Click its name, or play it.";
                noteChoices (q, n, acc);
            }
            else
            {
                q.prompt = "Play this note on the guitar (or click where it is on the neck).";
                q.showFretboard = true;
                q.fretClick = true;
                q.judgeFret = [midi] (int s, int f) { return fretMidi (s, f) == midi; };
            }
        }
        return q;
    }

    if (t == "nameFret" || t == "findFret")
    {
        const auto strings = intArray (spec.params["strings"], { 6, 5 });
        const int maxFret = (int) spec.params.getProperty ("maxFret", 12);
        int s = 6, f = 0;
        Note n;
        for (;;)
        {
            s = strings[(size_t) rng.nextInt ((int) strings.size())];
            f = rng.nextInt (maxFret + 1);
            n = noteFromMidi (fretMidi (s, f), rng.nextBool());
            if (acc || n.acc == 0)
                break;
        }
        q.showFretboard = true;
        q.maxFret = maxFret;
        q.expectedMidi = n.midi();

        if (t == "nameFret")
        {
            q.prompt = "Name the note at the dot (" + stringName (s) + ", " + (f == 0 ? juce::String ("open") : "fret " + juce::String (f)) + ").";
            q.fretMarks = { { s, f, "?", 4 } };
            noteChoices (q, n, acc);
            if (acc && n.acc != 0)
            {
                // either spelling of a black key is right
                const auto otherName = noteFromMidi (n.midi(), n.acc != -1).name();
                const int alt = q.choices.indexOf (otherName), main = q.correctChoice;
                q.judgeChoice = [alt, main] (int c) { return c == main || c == alt; };
            }
            const auto other = noteFromMidi (n.midi(), n.acc != -1);
            q.explanation = other.name() != n.name() ? "It is " + n.name() + " (also called " + other.name() + ")." : "It is " + n.name() + ".";
            q.revealMarks = { { s, f, n.name(), 2 } };
        }
        else
        {
            q.prompt = "Find " + n.name() + " on the " + stringName (s) + ". Click it or play it.";
            q.fretClick = true;
            q.acceptsPlaying = true;
            const int pc = n.pc();
            const int midi = n.midi();
            q.judgeFret = [s, pc] (int cs, int cf) { return cs == s && mod12 (fretMidi (cs, cf)) == pc; };
            q.judgePlayed = [midi] (int m) { return m == midi || m == midi + 12 || m == midi - 12; };
            for (int ff = 0; ff <= maxFret; ++ff)
                if (mod12 (fretMidi (s, ff)) == pc)
                    q.revealMarks.push_back ({ s, ff, n.name(), 2 });
            q.explanation = n.name() + " on the " + stringName (s) + " is at fret " + juce::String (f) + (f + 12 <= maxFret ? " and " + juce::String (f + 12) : f >= 12 ? " and " + juce::String (f - 12) : juce::String()) + ".";
        }
        return q;
    }

    if (t == "steps")
    {
        // two notes on the fretboard, 1, 2 or 3 semitones apart, sometimes across strings
        const int dist = 1 + rng.nextInt (3);
        int s = 2 + rng.nextInt (5);
        int f = rng.nextInt (9);
        const int midiA = fretMidi (s, f);
        const int midiB = midiA + dist;
        FretPos b { s, f + dist };
        if (rng.nextInt (3) == 0 && s > 1)
        {
            // the second note on the next higher string
            const int nf = midiB - openMidi[s - 1];
            if (nf >= 0)
                b = { s - 1, nf };
        }
        const auto na = noteFromMidi (midiA), nb = noteFromMidi (midiB, rng.nextBool());
        q.prompt = "From " + na.name() + " up to " + nb.name() + ": half step, whole step, or neither?";
        q.showFretboard = true;
        q.fretMarks = { { s, f, na.name(), 1 }, { b.string, b.fret, nb.name(), 0 } };
        q.choices = { "Half step", "Whole step", "Neither" };
        q.correctChoice = dist == 1 ? 0 : dist == 2 ? 1 : 2;
        q.listenMidi = { midiA, midiB };
        q.explanation = semitonesText (dist) + " = " + tonesText (dist) + ": " + stepName (dist).toLowerCase() + ".";
        return q;
    }

    if (t == "intervalSemis")
    {
        const auto set = intervalSet (spec.params["set"]);
        const auto iv = set[(size_t) rng.nextInt ((int) set.size())];
        q.prompt = "How many semitones are in a " + iv.longName() + " (" + iv.shortName() + ")?";
        for (int i = 0; i <= 12; ++i)
            q.choices.add (juce::String (i));
        q.correctChoice = iv.semitones;
        q.choiceColumns = 13;
        const auto root = *parseNote ("A2");
        const auto top = transposeUp (root, iv);
        q.showFretboard = true;
        q.revealMarks = { { 5, 0, "R", 1 }, { 5, iv.semitones, top.name(), 2 } };
        q.listenMidi = { root.midi(), top.midi() };
        q.explanation = iv.longName() + " = " + semitonesText (iv.semitones) + " = " + tonesText (iv.semitones) + ". From A: " + top.name() + ".";
        return q;
    }

    if (t == "intervalName" || t == "playInterval" || t == "earInterval")
    {
        const auto set = intervalSet (spec.params["set"]);
        const auto iv = set[(size_t) rng.nextInt ((int) set.size())];
        Note root, top;
        do
        {
            // keep spellings simple at this level: no double sharps, no E#, B#, Fb, Cb
            root = randomRoot (rng);
            top = transposeUp (root, iv);
        } while (std::abs (top.acc) > 1 || (top.acc == 1 && (top.letter == 2 || top.letter == 6))
                 || (top.acc == -1 && (top.letter == 3 || top.letter == 0)));
        const auto rootPos = lowestPosition (root.midi(), 12, root.midi() <= 47 ? 6 : root.midi() <= 52 ? 5 : 4);

        // the upper note on the same string or the next ones up, closest to the root's fret
        FretPos topPos = rootPos;
        int bestDist = 99;
        for (auto& p : positionsOf (top.midi(), 15))
            if (p.string <= rootPos.string && std::abs (p.fret - rootPos.fret) < bestDist)
            {
                bestDist = std::abs (p.fret - rootPos.fret);
                topPos = p;
            }

        const juce::String detail = semitonesText (iv.semitones) + " = " + tonesText (iv.semitones);
        q.listenMidi = { root.midi(), top.midi() };
        q.maxFret = 15;

        if (t == "intervalName" || t == "earInterval")
        {
            for (auto& i : set)
                q.choices.add (i.longName().upToFirstOccurrenceOf (" (", false, false));
            q.correctChoice = (int) (std::find_if (set.begin(), set.end(), [&] (auto& i) { return i.shortName() == iv.shortName(); }) - set.begin());
            q.choiceColumns = set.size() > 8 ? 6 : (int) set.size();
            q.explanation = root.name() + " up to " + top.name() + " is a " + iv.longName() + ": " + detail + ".";
            q.revealMarks = { { rootPos.string, rootPos.fret, "R", 1 }, { topPos.string, topPos.fret, top.name(), 2 } };
            if (t == "intervalName")
            {
                q.prompt = "Name the interval from " + root.name() + " up to " + top.name() + ".";
                q.staffNotes = { root, top };
                q.showFretboard = true;
                q.fretMarks = { { rootPos.string, rootPos.fret, root.name(), 1 }, { topPos.string, topPos.fret, top.name(), 0 } };
                q.revealMarks.clear();
            }
            else
            {
                q.prompt = "Listen: which interval is it?";
                q.autoListen = true;
                q.showFretboard = true;
                q.revealStaff = { root, top };
            }
        }
        else
        {
            q.prompt = "Play a " + iv.longName() + " above " + root.name() + " (the dot). Play the upper note, or click it.";
            q.showFretboard = true;
            q.fretMarks = { { rootPos.string, rootPos.fret, root.name(), 1 } };
            q.fretClick = true;
            q.acceptsPlaying = true;
            const int target = top.midi();
            q.expectedMidi = target;
            q.judgePlayed = [target] (int m) { return mod12 (m) == mod12 (target); };
            q.judgeFret = [target] (int s, int f) { return mod12 (fretMidi (s, f)) == mod12 (target); };
            q.revealMarks = { { rootPos.string, rootPos.fret, "R", 1 }, { topPos.string, topPos.fret, top.name(), 2 } };
            q.explanation = "A " + iv.longName() + " above " + root.name() + " is " + top.name() + ": " + detail + ".";
            q.listenMidi = { root.midi() };
        }
        return q;
    }

    q.prompt = "Unknown exercise type: " + t;
    return q;
}

juce::String describeExerciseInput (const ExerciseSpec& spec)
{
    const auto& t = spec.type;
    if (t == "playStaff" || t == "playInterval" || t == "findFret") return "play or click";
    if (t == "nameStaff") return "click or play";
    if (t == "earInterval") return "listen";
    if (t == "writeStaff") return "write";
    return "click";
}
