#include "Lessons.h"
#include "BinaryData.h"
#include "Fingerings.h"
#include "BackingTrack.h"
#include "Rhythm.h"

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
            lesson.track = l.getProperty ("track", "theory").toString();
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

    // no double sharps or flats, no E#, B#, Fb, Cb: correct spellings, but not for these levels
    bool plainSpelling (const Note& n)
    {
        if (std::abs (n.acc) > 1) return false;
        if (n.acc == 1 && (n.letter == 2 || n.letter == 6)) return false;
        if (n.acc == -1 && (n.letter == 3 || n.letter == 0)) return false;
        return true;
    }

    bool plainSpelling (const std::vector<Note>& notes)
    {
        for (auto& n : notes)
            if (! plainSpelling (n))
                return false;
        return true;
    }

    // a root from the exercise's "roots" list (or the fallback), at the given octave
    Note pickRoot (const ExerciseSpec& s, juce::Random& rng, juce::StringArray fallback, int octave)
    {
        juce::StringArray names;
        if (auto* a = s.params["roots"].getArray())
            for (auto& v : *a)
                names.add (v.toString());
        if (names.isEmpty())
            names = fallback;
        auto n = parseNote (names[rng.nextInt (names.size())]).value_or (Note {});
        n.octave = octave;
        return n;
    }

    juce::StringArray stringList (const juce::var& v, juce::StringArray fallback)
    {
        juce::StringArray out;
        if (auto* a = v.getArray())
            for (auto& x : *a)
                out.add (x.toString());
        return out.isEmpty() ? fallback : out;
    }

    // the 17 common spellings, plus any the answer needs (E#, B#, Cb, Fb)
    void sequenceChoices (Question& q, const std::vector<Note>& notes)
    {
        q.choices = accidentalChoices();
        for (auto& n : notes)
            q.choices.addIfNotAlreadyThere (n.name());
        q.choiceColumns = 9;
        const int first = juce::jmin (q.sequenceGiven, (int) notes.size() - 1);
        q.correctChoice = q.choices.indexOf (notes[(size_t) first].name());
    }

    juce::String noteList (const std::vector<Note>& notes)
    {
        juce::StringArray s;
        for (auto& n : notes)
            s.add (n.name());
        return s.joinIntoString (" ");
    }

    std::vector<int> midis (const std::vector<Note>& notes)
    {
        std::vector<int> m;
        for (auto& n : notes)
            m.push_back (n.midi());
        return m;
    }

    // keys from lo to hi fifths, as choices
    juce::StringArray keyChoices (int lo, int hi, bool minor)
    {
        juce::StringArray c;
        for (int f = lo; f <= hi; ++f)
            c.add (keyName (f, minor));
        return c;
    }

    // marks for every chord or scale tone in frets lo..hi, labelled by degree
    std::vector<FretMark> toneMarks (const std::vector<Note>& tones, const juce::StringArray& labels, int lo, int hi)
    {
        std::vector<FretMark> marks;
        for (int s = 1; s <= 6; ++s)
            for (int f = lo; f <= hi; ++f)
                for (size_t i = 0; i < tones.size(); ++i)
                    if (mod12 (fretMidi (s, f)) == tones[i].pc())
                        marks.push_back ({ s, f, labels[(int) i], i == 0 ? 1 : 0 });
        return marks;
    }

    // which chord tone (0 root, 1 third, 2 fifth) a pitch is, or -1
    int toneIndex (const std::vector<Note>& chord, int midi)
    {
        for (int i = 0; i < (int) chord.size(); ++i)
            if (mod12 (midi) == chord[(size_t) i].pc())
                return i;
        return -1;
    }

    // a close triad on three adjacent strings (top string `top`), starting from the bass fret; false if it does not fit
    bool triadOnStrings (const std::vector<Note>& chord, int top, int bassFret, std::vector<FretMark>& marks, const ChordType& type)
    {
        const int bassTone = toneIndex (chord, fretMidi (top + 2, bassFret));
        if (bassTone < 0)
            return false;
        marks.clear();
        int midi = fretMidi (top + 2, bassFret);
        int lo = bassFret, hi = bassFret;
        marks.push_back ({ top + 2, bassFret, chordToneLabel (bassTone, type), bassTone == 0 ? 1 : 0 });
        for (int k = 1; k < 3; ++k)
        {
            const int tone = (bassTone + k) % 3;
            int next = midi + 1;
            while (mod12 (next) != chord[(size_t) tone].pc())
                ++next;
            const int s = top + 2 - k;
            const int f = next - openMidi[s];
            if (f < 0 || f > 15)
                return false;
            lo = juce::jmin (lo, f);
            hi = juce::jmax (hi, f);
            marks.push_back ({ s, f, chordToneLabel (tone, type), tone == 0 ? 1 : 0 });
            midi = next;
        }
        return hi - lo <= 4;
    }

    // a stacked triad that sits on the staff: lowest note at least C3, top at most A4 (sounding)
    std::vector<Note> placeOnStaff (std::vector<Note> notes)
    {
        while (notes.back().midi() > 69)
            for (auto& n : notes) n.octave -= 1;
        while (notes.front().midi() < 48 && notes.back().midi() + 12 <= 69)
            for (auto& n : notes) n.octave += 1;
        return notes;
    }

    juce::String toneWord (int tone) { return tone == 0 ? "root" : tone == 1 ? "3rd" : "5th"; }
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
        q.listenBeforeAnswer = true;
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
            q.listenBeforeAnswer = true;
        }
        return q;
    }

    if (t == "spellScale" || t == "writeScale" || t == "playScale")
    {
        const auto& st = scaleType (spec.params.getProperty ("scale", "major").toString());
        const auto root = pickRoot (spec, rng, { "C", "G", "D", "A", "E", "F", "Bb", "Eb", "Ab" }, 3);
        const auto notes = scaleNotes (root, st);
        const auto name = root.name() + " " + st.name.toLowerCase();
        q.sequence = notes;
        q.sequenceGiven = t == "playScale" ? 0 : 1;
        q.listenMidi = midis (notes);
        q.listenGap = 0.45;
        q.revealStaff = notes;
        q.revealStepLabels = juce::StringArray::fromTokens (stepPattern (notes), " ", "");
        q.explanation = name + ": " + noteList (notes) + ". Steps: " + stepPattern (notes) + ".";

        if (t == "spellScale")
        {
            q.prompt = "Spell " + name + ": click the notes in order, from " + root.name() + " up to " + root.name() + ".";
            q.staffNotes = { root };
            sequenceChoices (q, notes);
        }
        else if (t == "writeScale")
        {
            q.prompt = "Write " + name + " on the staff, up to the next " + root.name() + ". Pick " + accidentalText (1) + " or "
                     + accidentalText (-1) + " before a note that needs one.";
            q.staffNotes = { root };
            q.staffClick = true;
            q.staffAccidentalButtons = true;
            q.sequenceAnyOctave = false;
            q.judgeStaff = [notes] (const Note& n) { for (auto& x : notes) if (x == n) return true; return false; };
        }
        else
        {
            q.prompt = "Play " + name + " up one octave, " + root.name() + " to " + root.name() + ", anywhere on the neck (or click the notes).";
            q.acceptsPlaying = true;
            q.showFretboard = true;
            q.fretClick = true;
            q.maxFret = 15;
            q.expectedMidi = root.midi();
            q.judgePlayed = [pc = root.pc()] (int m) { return mod12 (m) == pc; };
            q.judgeFret = [pc = root.pc()] (int s, int f) { return mod12 (fretMidi (s, f)) == pc; };
            // show the shape with the root on the 6th string
            const auto names = scaleSpelling (root, st);
            for (auto& fg : positionFingerings (root.pc(), st))
                if (fg.name.startsWith (st.size() < 7 ? "Box 1" : st.parent == 3 ? "G shape" : st.parent == 5 ? "A shape" : "E shape"))
                    for (auto& n : fg.notes)
                        q.revealMarks.push_back ({ n.string, n.fret, names[(size_t) mod12 (n.midi)], n.degree == 0 ? 1 : 0 });
        }
        return q;
    }

    if (t == "scaleDegree")
    {
        const auto& st = scaleType (spec.params.getProperty ("scale", "major").toString());
        const auto root = pickRoot (spec, rng, { "C", "G", "D", "A", "E", "F", "Bb", "Eb", "Ab" }, 3);
        const auto notes = scaleNotes (root, st, false);
        const int d = 1 + rng.nextInt ((int) notes.size() - 1);
        const auto n = notes[(size_t) d];
        const auto name = root.name() + " " + st.name.toLowerCase();
        q.revealStaff = scaleNotes (root, st);
        q.explanation = name + ": " + noteList (notes) + ". " + n.name() + " is the " + ordinal (d + 1) + " degree.";
        if (rng.nextBool())
        {
            q.prompt = "What is the " + ordinal (d + 1) + " note of " + name + "?";
            q.choices = accidentalChoices();
            q.choices.addIfNotAlreadyThere (n.name());
            q.correctChoice = q.choices.indexOf (n.name());
            q.choiceColumns = 9;
        }
        else
        {
            q.prompt = "In " + name + ", which degree is " + n.name() + "?";
            for (int i = 1; i <= (int) notes.size(); ++i)
                q.choices.add (juce::String (i));
            q.correctChoice = d;
        }
        return q;
    }

    if (t == "nameKeySig" || t == "countKeySig")
    {
        const int lo = (int) spec.params.getProperty ("lo", -4), hi = (int) spec.params.getProperty ("hi", 4);
        const bool minor = (bool) spec.params.getProperty ("minor", false);
        const int f = lo + rng.nextInt (hi - lo + 1);
        const auto key = keyName (f, minor);
        q.explanation = key + " has " + keySignatureText (f) + "."
                      + (minor ? " Its relative major is " + keyName (f, false) + "." : " Its relative minor is " + keyName (f, true) + ".");
        for (int i = lo; i <= hi; ++i)
            q.choices.add (t == "nameKeySig" ? keyName (i, minor) : fifthsLabel (i));
        q.correctChoice = f - lo;
        q.choiceColumns = q.choices.size() > 9 ? (q.choices.size() + 1) / 2 : q.choices.size();
        if (t == "nameKeySig")
        {
            q.prompt = juce::String ("Which ") + (minor ? "minor" : "major") + " key has this key signature?";
            q.showKeySignature = true;
            q.keySignature = f;
        }
        else
            q.prompt = "How many sharps or flats are in " + key + "?";
        return q;
    }

    if (t == "circle")
    {
        const auto modes = stringList (spec.params["modes"], { "fifthUp", "fifthDown", "relMinor", "relMajor" });
        const auto mode = modes[rng.nextInt (modes.size())];
        const int f = -5 + rng.nextInt (11);
        int answer = f;
        bool answerMinor = false;
        if (mode == "fifthUp")
        {
            answer = f + 1;
            q.prompt = "One step clockwise on the circle of fifths (a 5th up) from " + keyName (f, false) + ": which key?";
            q.explanation = "A perfect 5th above " + keyTonic (f, false).name() + " is " + keyTonic (answer, false).name() + ": "
                          + keyName (answer, false) + ", with " + keySignatureText (answer) + ".";
        }
        else if (mode == "fifthDown")
        {
            answer = f - 1;
            q.prompt = "One step counter-clockwise (a 4th up) from " + keyName (f, false) + ": which key?";
            q.explanation = "A perfect 4th above " + keyTonic (f, false).name() + " is " + keyTonic (answer, false).name() + ": "
                          + keyName (answer, false) + ", with " + keySignatureText (answer) + ".";
        }
        else if (mode == "relMinor")
        {
            answerMinor = true;
            q.prompt = "What is the relative minor of " + keyName (f, false) + "?";
            q.explanation = "Go down 3 semitones (a minor 3rd) from " + keyTonic (f, false).name() + ": " + keyName (f, true)
                          + ". Both have " + keySignatureText (f) + ".";
        }
        else
        {
            q.prompt = "What is the relative major of " + keyName (f, true) + "?";
            q.explanation = "Go up 3 semitones (a minor 3rd) from " + keyTonic (f, true).name() + ": " + keyName (f, false)
                          + ". Both have " + keySignatureText (f) + ".";
        }
        // six choices around the answer, in circle order
        std::vector<int> opts { answer };
        while (opts.size() < 6)
        {
            const int o = juce::jlimit (-7, 7, answer - 4 + rng.nextInt (9));
            if (std::find (opts.begin(), opts.end(), o) == opts.end())
                opts.push_back (o);
        }
        std::sort (opts.begin(), opts.end());
        for (int o : opts)
            q.choices.add (keyName (o, answerMinor));
        q.correctChoice = q.choices.indexOf (keyName (answer, answerMinor));
        q.choiceColumns = 6;
        return q;
    }

    if (t == "earScale")
    {
        const auto ids = stringList (spec.params["set"], { "major", "naturalMinor", "harmonicMinor" });
        const int which = rng.nextInt (ids.size());
        const auto& st = scaleType (ids[which]);
        Note root;
        do root = noteFromMidi (48 + rng.nextInt (12), rng.nextBool());
        while (! plainSpelling (scaleNotes (root, st)));
        const auto notes = scaleNotes (root, st);
        q.prompt = "Listen: which scale is it?";
        q.autoListen = true;
        q.listenMidi = midis (notes);
        q.listenGap = 0.42;
        for (auto& id : ids)
            q.choices.add (scaleType (id).name);
        q.correctChoice = which;
        q.revealStaff = notes;
        q.revealStepLabels = juce::StringArray::fromTokens (stepPattern (notes), " ", "");
        q.explanation = root.name() + " " + st.name.toLowerCase() + ": " + stepPattern (notes) + ".";
        return q;
    }

    if (t == "spellTriad" || t == "triadQuality" || t == "earTriad" || t == "playTriad" || t == "inversionName")
    {
        const auto ids = stringList (spec.params["set"], { "major", "minor" });
        const int which = rng.nextInt (ids.size());
        const auto& ct = chordType (ids[which]);
        Note root;
        std::vector<Note> chord;
        do
        {
            root = pickRoot (spec, rng, { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" }, 3);
            chord = chordNotes (root, ct);
        } while (! plainSpelling (ct.id == "diminished7" ? std::vector<Note> (chord.begin(), chord.begin() + 3) : chord));
        const auto name = chordName (root, ct);
        juce::String tones;
        for (int i = 0; i < (int) chord.size(); ++i)
            tones << (i ? ", " : "") << chord[(size_t) i].name() << " (" << chordToneLabel (i, ct) << ")";
        const auto lower = intervalBetween (chord[0], chord[1]), upper = intervalBetween (chord[1], chord[2]);
        q.explanation = name + " = " + tones + (chord.size() == 3 ? ": a " + lower.longName() + " with a " + upper.longName() + " on top." : juce::String ("."));
        q.listenMidi = midis (chord);
        q.listenAsChord = true;
        q.listenGap = 0.5;

        if (t == "spellTriad")
        {
            q.prompt = "Spell " + name + " (" + chordSymbol (root, ct) + "): click the " + (chord.size() == 4 ? "3rd, 5th and 7th." : "3rd and the 5th.");
            q.sequence = chord;
            q.sequenceGiven = 1;
            q.staffNotes = { root };
            q.revealStaff = chord;
            sequenceChoices (q, chord);
        }
        else if (t == "triadQuality" || t == "earTriad")
        {
            for (auto& id : ids)
                q.choices.add (chordType (id).name);
            q.correctChoice = which;
            q.choiceColumns = ids.size();
            q.staffChord = true;
            if (t == "triadQuality")
            {
                q.prompt = "Is this triad major, minor, diminished or augmented?";
                q.staffNotes = chord;
            }
            else
            {
                q.prompt = "Listen: what kind of triad is it?";
                q.autoListen = true;
                q.revealStaff = chord;
            }
        }
        else if (t == "playTriad")
        {
            q.prompt = "Play " + name + " one note at a time: root, 3rd, 5th (any octave, or click them).";
            q.sequence = chord;
            q.acceptsPlaying = true;
            q.showFretboard = true;
            q.fretClick = true;
            q.maxFret = 12;
            q.expectedMidi = root.midi();
            q.judgePlayed = [pc = root.pc()] (int m) { return mod12 (m) == pc; };
            q.judgeFret = [pc = root.pc()] (int s, int f) { return mod12 (fretMidi (s, f)) == pc; };
            juce::StringArray toneLabels;
            for (int i = 0; i < (int) chord.size(); ++i)
                toneLabels.add (chordToneLabel (i, ct));
            q.revealMarks = toneMarks (chord, toneLabels, 0, 5);
        }
        else
        {
            const int inv = rng.nextInt (3);
            const auto voiced = placeOnStaff (invert (chord, inv));
            q.prompt = "Which inversion of " + name + " is this?";
            q.staffNotes = voiced;
            q.staffChord = true;
            q.listenMidi = midis (voiced);
            q.choices = { "Root position", "1st inversion", "2nd inversion" };
            q.correctChoice = inv;
            q.choiceColumns = 3;
            q.explanation = "The lowest note is " + voiced[0].name() + ", the " + toneWord (inv) + " of " + name + ": " + inversionName (inv) + ".";
        }
        return q;
    }

    if (t == "triadShape")
    {
        const auto ids = stringList (spec.params["set"], { "major", "minor" });
        const auto tops = intArray (spec.params["topStrings"], { 1, 2, 3 });
        for (;;)
        {
            const auto& ct = chordType (ids[rng.nextInt (ids.size())]);
            const auto root = pickRoot (spec, rng, { "C", "D", "E", "F", "G", "A", "Bb", "Eb" }, 3);
            const auto chord = chordNotes (root, ct);
            if (! plainSpelling (chord))
                continue;
            const int top = tops[(size_t) rng.nextInt ((int) tops.size())];
            const int bassFret = rng.nextInt (12);
            std::vector<FretMark> marks;
            if (! triadOnStrings (chord, top, bassFret, marks, ct))
                continue;

            const int inv = toneIndex (chord, fretMidi (top + 2, bassFret));   // the bass note tells the inversion
            q.prompt = "Which triad is this, on strings " + juce::String (top) + "-" + juce::String (top + 1) + "-" + juce::String (top + 2) + "?";
            q.showFretboard = true;
            q.maxFret = 15;
            for (auto m : marks)
            {
                m.label = chord[(size_t) toneIndex (chord, fretMidi (m.string, m.fret))].name();
                m.colour = 0;
                q.fretMarks.push_back (m);
            }
            q.revealMarks = marks;
            // choices: each of the three notes as a root, in each quality of the set
            juce::StringArray c;
            for (auto& n : chord)
                for (auto& id : ids)
                    if (plainSpelling (chordNotes (n, chordType (id))))
                        c.addIfNotAlreadyThere (chordSymbol (n, chordType (id)));
            c.addIfNotAlreadyThere (chordSymbol (root, ct));
            q.choices = c;
            q.correctChoice = c.indexOf (chordSymbol (root, ct));
            q.choiceColumns = juce::jmin (6, c.size());
            q.listenMidi = { fretMidi (marks[0].string, marks[0].fret), fretMidi (marks[1].string, marks[1].fret), fretMidi (marks[2].string, marks[2].fret) };
            q.listenAsChord = true;
            q.listenGap = 0.4;
            q.explanation = noteList (chord) + " is " + chordName (root, ct) + " (" + chordSymbol (root, ct) + "), here in " + inversionName (inv)
                          + ": the " + toneWord (inv) + " is in the bass.";
            return q;
        }
    }

    if (t == "intervalShape")
    {
        const auto set = intervalSet (spec.params["set"]);
        for (;;)
        {
            const auto iv = set[(size_t) rng.nextInt ((int) set.size())];
            const int s = 3 + rng.nextInt (4);
            const int f = 1 + rng.nextInt (10);
            const int s2 = s - 1 - rng.nextInt (2);
            const int f2 = fretMidi (s, f) + iv.semitones - openMidi[s2];
            if (f2 < 0 || f2 > 15 || std::abs (f2 - f) > 4)
                continue;
            const auto root = noteFromMidi (fretMidi (s, f), rng.nextBool());
            const auto top = transposeUp (root, iv);
            if (! plainSpelling (root) || ! plainSpelling (top))
                continue;
            q.prompt = "Name the interval from the root (gold) up to the other note, by its shape.";
            q.showFretboard = true;
            q.maxFret = 15;
            q.fretMarks = { { s, f, "R", 1 }, { s2, f2, "?", 4 } };
            q.revealMarks = { { s, f, "R", 1 }, { s2, f2, iv.shortName(), 2 } };
            for (auto& i : set)
                q.choices.add (i.longName().upToFirstOccurrenceOf (" (", false, false));
            q.correctChoice = (int) (std::find_if (set.begin(), set.end(), [&] (auto& i) { return i.shortName() == iv.shortName(); }) - set.begin());
            q.choiceColumns = set.size() > 8 ? 6 : (int) set.size();
            q.listenMidi = { root.midi(), top.midi() };
            const bool crossesB = s >= 3 && s2 <= 2;
            q.explanation = root.name() + " up to " + top.name() + ": " + iv.longName() + ", " + semitonesText (iv.semitones) + " = " + tonesText (iv.semitones) + "."
                          + (crossesB ? " Crossing onto the B string moves the shape one fret higher." : "");
            return q;
        }
    }

    if (t == "bluesChart" || t == "bluesNumbers")
    {
        const auto forms = stringList (spec.params["forms"], { "standard" });
        const auto& form = bluesForm (forms[rng.nextInt (forms.size())]);
        const auto key = pickRoot (spec, rng, { "E", "A", "G", "C", "D", "Bb", "F" }, 3);
        const int keyPc = key.pc();
        // the different chords of the form, in order of appearance
        std::vector<BluesChord> chords;
        for (auto& b : form.bars)
            for (auto& c : b)
                if (std::find_if (chords.begin(), chords.end(), [&] (auto& x) { return x.offset == c.offset && x.quality == c.quality; }) == chords.end())
                    chords.push_back (c);
        auto indexOf = [&] (const BluesChord& c)
        {
            return (int) (std::find_if (chords.begin(), chords.end(), [&] (auto& x) { return x.offset == c.offset && x.quality == c.quality; }) - chords.begin());
        };
        juce::StringArray chart;
        for (int i = 0; i < 12; ++i)
        {
            juce::StringArray inBar;
            for (auto& c : form.bars[(size_t) i]) inBar.add (bluesChordName (keyPc, c));
            chart.add (inBar.joinIntoString (" "));
        }
        const auto formText = form.name + " in " + key.name() + ": | " + chart.joinIntoString (" | ") + " |";

        if (t == "bluesChart")
        {
            const int barI = rng.nextInt (12);
            const auto& c = form.bars[(size_t) barI][0];
            q.prompt = form.name + " in " + key.name() + ": which chord starts bar " + juce::String (barI + 1) + "?";
            for (auto& x : chords)
                q.choices.add (bluesChordName (keyPc, x) + "  (" + bluesRoman (x, form.minor) + ")");
            q.correctChoice = indexOf (c);
            q.explanation = formText;
        }
        else
        {
            const auto& c = chords[(size_t) rng.nextInt ((int) chords.size())];
            if (rng.nextBool())
            {
                q.prompt = "In a blues in " + key.name() + ", which chord is the " + bluesRoman (c, form.minor) + "?";
                for (auto& x : chords)
                    q.choices.add (bluesChordName (keyPc, x));
                q.correctChoice = indexOf (c);
            }
            else
            {
                q.prompt = "In a blues in " + key.name() + ", " + bluesChordName (keyPc, c) + " is which chord?";
                for (auto& x : chords)
                    q.choices.add (bluesRoman (x, form.minor));
                q.correctChoice = indexOf (c);
            }
            q.explanation = "Count up the major scale of " + key.name() + ": " + bluesChordName (keyPc, c) + " is the " + bluesRoman (c, form.minor) + ". " + formText;
        }
        q.choiceColumns = juce::jmin (4, q.choices.size());
        return q;
    }

    if (t == "earBluesChange")
    {
        const int keyPc = rng.nextInt (12);
        const int moves[3] = { 5, 7, 0 };
        const int which = rng.nextInt (3);
        auto voicing = [&] (int offset)
        {
            const int r = 45 + mod12 (keyPc + offset - 9);   // A2..G#3
            return std::vector<int> { r - 12 + (r - 12 < 40 ? 12 : 0), r + 4, r + 10, r + 14 };
        };
        q.prompt = "Listen: the I chord, then the next chord. Where did it go?";
        q.listenChords = { voicing (0), voicing (moves[which]) };
        q.listenGap = 1.3;
        q.autoListen = true;
        q.choices = { "Up to the IV", "Up to the V", "Stayed on the I" };
        q.correctChoice = which;
        const BluesChord i7 { 0, "7" }, next { moves[which], "7" };
        q.explanation = juce::String (which == 2 ? "It stayed on " : "It moved from ") + bluesChordName (keyPc, i7)
                      + (which == 2 ? "." : " to " + bluesChordName (keyPc, next) + (which == 0 ? ": the bass goes up a 4th, a calm, open move." : ": the bass goes up a 5th, a stronger pull that wants to go home."));
        return q;
    }

    if (t == "earFeel")
    {
        const auto set = intArray (spec.params["feels"], { 0, 1 });
        const int which = rng.nextInt ((int) set.size());
        q.prompt = "Listen to the backing band: which feel is it?";
        q.listenBacking = set[(size_t) which];
        q.listenBackingKey = rng.nextInt (12);
        q.listenBackingBpm = set[(size_t) which] == 2 ? 60.0 : 85.0 + rng.nextInt (30);
        q.autoListen = true;
        for (int f : set)
            q.choices.add (feelNames()[f]);
        q.correctChoice = which;
        static const char* why[4] = {
            "Shuffle: each beat splits into a long and a short note (the 1st and 3rd notes of a triplet). Listen to the hi-hat: da-ga da-ga.",
            "Straight: each beat splits into two equal eighth notes, as in rock and early Chuck Berry.",
            "Slow blues: four slow beats per bar, each split into three, so you count 12 in a bar (12/8).",
            "Funk: sixteenth notes on the hi-hat and a busy, syncopated bass." };
        q.explanation = why[juce::jlimit (0, 3, set[(size_t) which])];
        return q;
    }

    if (t == "boxDegree")
    {
        const auto& st = scaleType (spec.params.getProperty ("scale", "minorPentatonic").toString());
        const auto root = pickRoot (spec, rng, { "A", "E", "G", "C", "D" }, 3);
        const auto boxes = positionFingerings (root.pc(), st);
        const auto allowed = intArray (spec.params["boxes"], { 0, 1, 2, 3, 4 });
        const auto& box = boxes[(size_t) juce::jlimit (0, (int) boxes.size() - 1, allowed[(size_t) rng.nextInt ((int) allowed.size())])];
        const auto& n = box.notes[(size_t) rng.nextInt ((int) box.notes.size())];
        const auto names = scaleSpelling (root, st);
        q.prompt = root.name() + " " + st.name.toLowerCase() + ", " + box.name.upToFirstOccurrenceOf (" (", false, false)
                 + ": which degree is the blue note?";
        q.showFretboard = true;
        q.maxFret = juce::jmax (15, box.highFret + 1);
        juce::StringArray labels;
        for (int d = 0; d < st.size(); ++d)
            labels.add (d == 0 ? juce::String ("R") : degreeLabel (st.intervals[(size_t) d]));
        for (auto& x : box.notes)
        {
            const bool target = x.string == n.string && x.fret == n.fret;
            q.fretMarks.push_back ({ x.string, x.fret, target ? "?" : juce::String(), target ? 4 : (x.degree == 0 ? 1 : 0) });
            q.revealMarks.push_back ({ x.string, x.fret, labels[x.degree], target ? 2 : (x.degree == 0 ? 1 : 0) });
        }
        // the root marks stay gold so the box can be found from them
        for (auto& m : q.fretMarks)
            if (m.colour == 1) m.label = "R";
        q.choices = labels;
        q.correctChoice = n.degree;
        q.choiceColumns = labels.size();
        q.listenMidi = { fretMidi (n.string, n.fret) };
        q.explanation = "That note is " + names[(size_t) mod12 (n.midi)] + ", the " + labels[n.degree] + " of " + root.name() + ".";
        return q;
    }

    if (t == "bendTarget" || t == "playBend")
    {
        // a bend on the top three strings of box 1, a half or whole step
        const auto root = pickRoot (spec, rng, { "A", "E", "G", "C", "D" }, 3);
        const auto& st = scaleType ("minorPentatonic");
        const auto boxes = positionFingerings (root.pc(), st);
        std::vector<FretNote> candidates;
        for (auto& x : boxes.front().notes)
            if (x.string <= 3 && x.degree != 0)
                candidates.push_back (x);
        const auto n = candidates[(size_t) rng.nextInt ((int) candidates.size())];
        // the musical bends: b3 up a half step to the 3, 4 up a whole step to the 5, b7 up to the root
        const juce::String label = degreeLabel (st.intervals[(size_t) n.degree]);
        const int amount = label == "b3" ? (rng.nextBool() ? 1 : 2) : 2;
        const int target = n.midi + amount;
        const auto names = scaleSpelling (root, scaleType ("major"));
        auto spell = [&] (int midi)
        {
            const auto& s = names[(size_t) mod12 (midi)];
            return s.isNotEmpty() ? s : noteFromMidi (midi, true).name();
        };
        const juce::String size = amount == 1 ? "a half step" : "a whole step";
        const int semis = mod12 (target - root.pc());
        const juce::String degree = semis == 0 ? "root" : octaveIntervals()[(size_t) semis].longName().replace ("perfect ", "").replace ("major ", "");
        q.explanation = "Bending " + spell (n.midi) + " " + size + " reaches " + spell (target) + ", the " + degree + " of " + root.name() + ".";
        q.showFretboard = true;
        q.maxFret = 15;
        q.fretMarks = { { n.string, n.fret, spell (n.midi), 4 } };
        if (t == "bendTarget")
        {
            q.prompt = "Key of " + root.name() + ": you bend this note (" + stringName (n.string) + ", fret " + juce::String (n.fret) + ") " + size + ". Which note do you reach?";
            q.choices = accidentalChoices();
            q.choices.addIfNotAlreadyThere (spell (target));
            q.correctChoice = q.choices.indexOf (spell (target));
            q.choiceColumns = 9;
            q.listenMidi = { n.midi, target };
            q.revealMarks = { { n.string, n.fret, spell (n.midi), 0 }, { n.string, n.fret + amount, spell (target), 2 } };
        }
        else
        {
            q.prompt = "Bend the blue dot (" + spell (n.midi) + ", " + stringName (n.string) + ", fret " + juce::String (n.fret) + ") " + size + " up to " + spell (target)
                     + ". Hold it there until it turns green.";
            q.acceptsPlaying = true;
            q.expectedMidi = target;
            q.judgePlayed = [target] (int m) { return m == target; };
            const int start = n.midi;
            q.ignorePlayed = [start, target] (int m) { return m >= start && m < target; };   // on the way up
            q.listenMidi = { target };
            q.listenBeforeAnswer = true;
            q.revealMarks = { { n.string, n.fret, spell (n.midi), 0 }, { n.string, n.fret + amount, spell (target), 2 } };
            q.explanation << " Check it: the target is the same pitch as fret " << (n.fret + amount) << " on that string.";
        }
        return q;
    }

    if (t == "chordTone")
    {
        const auto forms = stringList (spec.params["forms"], { "standard" });
        const auto& form = bluesForm (forms[rng.nextInt (forms.size())]);
        const auto key = pickRoot (spec, rng, { "E", "A", "G", "C", "D", "F" }, 3);
        std::vector<BluesChord> chords;
        for (auto& b : form.bars)
            for (auto& c : b)
                if (std::find_if (chords.begin(), chords.end(), [&] (auto& x) { return x.offset == c.offset && x.quality == c.quality; }) == chords.end())
                    chords.push_back (c);
        const auto& c = chords[(size_t) rng.nextInt ((int) chords.size())];
        const auto tones = bluesChordTones (c);
        static const char* toneNames7[4] = { "root", "3rd", "5th", "7th" };
        auto semisLabel = [] (int s) { return s == 0 ? "R" : s == 3 ? "b3" : s == 4 ? "3" : s == 6 ? "b5" : s == 7 ? "5" : s == 9 ? "bb7" : s == 10 ? "b7" : "7"; };
        juce::StringArray labels;
        for (int k = 0; k < 4; ++k)
            labels.add (semisLabel (tones[(size_t) k]));
        // spell the chord from its root
        const auto& ct = chordType (c.quality == "m7" ? "minor7" : c.quality == "dim7" ? "diminished7" : "dominant7");
        const auto notes = chordNotes (bluesChordRoot (key.pc(), c), ct);
        const int i = 1 + rng.nextInt (3);
        const auto name = bluesChordName (key.pc(), c);
        q.explanation = name + " = " + noteList (notes) + " (R " + labels[1] + " " + labels[2] + " " + labels[3] + "). Over the " + bluesRoman (c, form.minor)
                      + " chord, aim for its 3rd and 7th: they are the notes that change from chord to chord.";
        if (rng.nextBool())
        {
            q.prompt = "Blues in " + key.name() + ", over " + name + " (the " + bluesRoman (c, form.minor) + "): which note is the " + toneNames7[i] + "?";
            q.choices = accidentalChoices();
            for (auto& n : notes) q.choices.addIfNotAlreadyThere (n.name());
            q.correctChoice = q.choices.indexOf (notes[(size_t) i].name());
            q.choiceColumns = 9;
        }
        else
        {
            q.prompt = "Blues in " + key.name() + ": over " + name + ", what is " + notes[(size_t) i].name() + "?";
            for (int k = 0; k < 4; ++k)
                q.choices.add (juce::String (toneNames7[k]) + " (" + labels[k] + ")");
            q.correctChoice = i;
            q.choiceColumns = 4;
        }
        q.listenMidi = midis (notes);
        q.listenAsChord = true;
        q.listenGap = 0.35;
        return q;
    }

    if (t == "diatonicChord")
    {
        const bool sevenths = (bool) spec.params.getProperty ("sevenths", false);
        const auto key = pickRoot (spec, rng, { "C", "G", "D", "A", "E", "F", "Bb", "Eb" }, 3);
        std::vector<DiatonicChord> all;
        for (int d = 0; d < 7; ++d)
            all.push_back (diatonicChord (key, d, sevenths));
        const int d = rng.nextInt (7);
        const auto& c = all[(size_t) d];
        juce::String list;
        for (auto& x : all)
            list << (list.isEmpty() ? "" : "   ") << romanNumeral (x.degree, *x.type) << " " << chordSymbol (x.root, *x.type);
        q.explanation = romanNumeral (d, *c.type) + " in " + key.name() + " major is " + chordSymbol (c.root, *c.type) + " (" + noteList (c.notes) + "), "
                      + c.type->name + ". All of them: " + list;
        q.listenMidi = midis (c.notes);
        q.listenAsChord = true;
        q.listenGap = 0.35;
        const int mode = rng.nextInt (3);
        if (mode == 0)
        {
            q.prompt = "In " + key.name() + " major, which chord is " + romanNumeral (d, *c.type) + "?";
            for (auto& x : all) q.choices.add (chordSymbol (x.root, *x.type));
        }
        else if (mode == 1)
        {
            q.prompt = "In " + key.name() + " major, " + chordSymbol (c.root, *c.type) + " is which chord?";
            for (auto& x : all) q.choices.add (romanNumeral (x.degree, *x.type));
        }
        else
        {
            q.prompt = "In any major key, what kind of chord is built on the " + ordinal (d + 1) + " degree" + (sevenths ? " (with its 7th)?" : "?");
            const auto& types = sevenths ? std::vector<juce::String> { "major7", "minor7", "dominant7", "halfDiminished" }
                                         : std::vector<juce::String> { "major", "minor", "diminished" };
            for (auto& id : types) q.choices.add (chordType (id).name);
            q.correctChoice = q.choices.indexOf (c.type->name);
            q.choiceColumns = q.choices.size();
            return q;
        }
        q.correctChoice = d;
        q.choiceColumns = 7;
        return q;
    }

    if (t == "chordFunction")
    {
        const auto key = pickRoot (spec, rng, { "C", "G", "D", "F", "Bb", "A" }, 3);
        const int d = rng.nextInt (7);
        const auto c = diatonicChord (key, d, (bool) spec.params.getProperty ("sevenths", true));
        const auto fn = harmonicFunction (d);
        q.prompt = "In " + key.name() + " major, what job does " + chordSymbol (c.root, *c.type) + " (" + romanNumeral (d, *c.type) + ") do?";
        q.choices = { "Tonic (home)", "Subdominant (away)", "Dominant (tension)" };
        q.correctChoice = fn == "tonic" ? 0 : fn == "subdominant" ? 1 : 2;
        q.choiceColumns = 3;
        static const char* why[3] = { "I, iii and vi share most of their notes with the I chord: they feel at rest.",
                                      "ii and IV move away from home and lead naturally to the dominant.",
                                      "V and vii contain the 7th degree (the leading tone) and the tritone: they pull back to I." };
        q.explanation = romanNumeral (d, *c.type) + " is " + fn + ". " + why[q.correctChoice];
        q.listenMidi = midis (c.notes);
        q.listenAsChord = true;
        q.listenGap = 0.35;
        return q;
    }

    if (t == "progressionName" || t == "earProgression")
    {
        const auto& all = commonProgressions();
        std::vector<int> pool;
        const bool wantSevenths = (bool) spec.params.getProperty ("sevenths", false);
        for (int i = 0; i < (int) all.size(); ++i)
            if (all[(size_t) i].sevenths == wantSevenths || (bool) spec.params.getProperty ("mixed", false))
                pool.push_back (i);
        // four different progressions as choices, one of them right
        std::vector<int> opts;
        while (opts.size() < juce::jmin<size_t> (4, pool.size()))
        {
            const int o = pool[(size_t) rng.nextInt ((int) pool.size())];
            if (std::find (opts.begin(), opts.end(), o) == opts.end())
                opts.push_back (o);
        }
        const int which = rng.nextInt ((int) opts.size());
        const auto& p = all[(size_t) opts[(size_t) which]];
        const auto key = pickRoot (spec, rng, { "C", "G", "D", "F", "Bb", "A", "E", "Eb" }, 3);
        juce::StringArray symbols;
        for (int d : p.degrees)
        {
            const auto c = diatonicChord (key, d, p.sevenths);
            symbols.add (chordSymbol (c.root, *c.type));
            // voiced around the middle of the guitar, with the root below
            std::vector<int> v;
            int bassNote = c.root.midi();
            while (bassNote > 50) bassNote -= 12;
            while (bassNote < 40) bassNote += 12;
            v.push_back (bassNote);
            for (size_t k = 1; k < c.notes.size(); ++k)
            {
                int m = c.notes[k].midi();
                while (m <= bassNote + 2) m += 12;
                while (m > bassNote + 19) m -= 12;
                v.push_back (m);
            }
            q.listenChords.push_back (v);
        }
        for (int o : opts)
            q.choices.add (all[(size_t) o].name);
        q.correctChoice = which;
        q.choiceColumns = juce::jmin (2, q.choices.size());
        q.listenGap = 1.1;
        q.explanation = p.name + " in " + key.name() + ": " + symbols.joinIntoString ("  ") + ".";
        if (t == "progressionName")
            q.prompt = "In " + key.name() + " major: " + symbols.joinIntoString ("  -  ") + ". Which progression is it?";
        else
        {
            q.prompt = "Listen: which progression is it? Follow the bass.";
            q.autoListen = true;
        }
        return q;
    }

    if (t == "rhythmValue")
    {
        const auto& values = noteValues();
        const int which = rng.nextInt ((int) values.size());
        const auto& v = values[(size_t) which];
        q.rhythm = { { 0.0, v.beats } };
        q.rhythmBeats = 4.0;
        for (auto& x : values)
            q.choices.add (x.beats == 0.25 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xbc")) : x.beats == 0.5 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xbd"))
                           : x.beats == 0.75 ? juce::String (juce::CharPointer_UTF8 ("\xc2\xbe")) : x.beats == 1.5 ? juce::String (juce::CharPointer_UTF8 ("1\xc2\xbd"))
                           : juce::String ((int) x.beats));
        q.prompt = "How many beats does this note last in 4/4?";
        q.correctChoice = which;
        q.choiceColumns = 8;
        q.explanation = "It is a " + v.name + ": " + q.choices[which] + (v.beats == 1.0 ? " beat." : " beats.")
                      + (v.name.startsWith ("dotted") ? " A dot adds half the note's value again." : "");
        return q;
    }

    if (t == "tapRhythm")
    {
        const int level = (int) spec.params.getProperty ("level", 1);
        const int beats = (int) spec.params.getProperty ("beats", 8);
        q.rhythm = randomRhythm (level, beats, rng);
        q.rhythmBeats = beats;
        q.tapRhythm = true;
        q.tapBpm = (double) spec.params.getProperty ("bpm", level >= 3 ? 66 : 80);
        q.prompt = "Tap this rhythm with the click: Space, the pad, or strum muted strings. Count out loud.";
        // Listen: four clicks to count in, then the rhythm over the clicks
        const double spb = 60.0 / q.tapBpm;
        for (int b = 0; b < 4 + beats; ++b)
        {
            q.listenMidi.push_back (b < 4 ? 88 : 84);
            q.listenTimes.push_back (b * spb);
        }
        for (auto& e : q.rhythm)
        {
            q.listenMidi.push_back (69);
            q.listenTimes.push_back ((4.0 + e.beat) * spb);
        }
        q.listenBeforeAnswer = true;
        q.explanation = juce::String ((int) q.rhythm.size()) + " notes at " + juce::String ((int) q.tapBpm) + " BPM.";
        return q;
    }

    if (t == "earCell")
    {
        const bool three = spec.params.getProperty ("pulses", 3).toString() == "3";
        const auto& cells = three ? threePulseCells() : fourPulseCells();
        const int which = rng.nextInt ((int) cells.size());
        const auto& c = cells[(size_t) which];
        const double spb = 60.0 / 70.0;
        // a click on each of four beats, the cell on every beat
        for (int b = 0; b < 4; ++b)
        {
            q.listenMidi.push_back (88);
            q.listenTimes.push_back (b * spb);
            for (auto& e : cellEvents (c, b))
            {
                q.listenMidi.push_back (69);
                q.listenTimes.push_back (e.beat * spb + 0.004);
            }
        }
        q.autoListen = true;
        q.prompt = juce::String ("Listen: which ") + (three ? "three" : "four") + "-pulse cell is played on every beat?";
        for (auto& x : cells)
            q.choices.add (x.name + "   " + x.syllables);
        q.correctChoice = which;
        q.choiceColumns = three ? 2 : 3;
        q.explanation = c.name + ": " + c.syllables + ". Say it while you tap it.";
        return q;
    }

    q.prompt = "Unknown exercise type: " + t;
    return q;
}

std::vector<FretMark> triadShapeMarks (const Note& root, const ChordType& type, int top, int maxFret)
{
    const auto chord = chordNotes (root, type);
    std::vector<FretMark> all, shape;
    for (int f = 0; f <= maxFret; ++f)
        if (triadOnStrings (chord, top, f, shape, type))
            for (auto& m : shape)
                if (m.fret <= maxFret)
                    all.push_back (m);
    return all;
}

juce::String describeExerciseInput (const ExerciseSpec& spec)
{
    const auto& t = spec.type;
    if (t == "playStaff" || t == "playInterval" || t == "findFret" || t == "playScale" || t == "playTriad") return "play or click";
    if (t == "nameStaff") return "click or play";
    if (t == "earInterval" || t == "earScale" || t == "earTriad" || t == "earBluesChange" || t == "earFeel" || t == "earProgression" || t == "earCell") return "listen";
    if (t == "tapRhythm") return "tap or strum";
    if (t == "playBend") return "play";
    if (t == "writeStaff" || t == "writeScale") return "write";
    return "click";
}
