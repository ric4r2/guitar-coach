#include "Lessons.h"
#include "BinaryData.h"
#include "Fingerings.h"

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
                if (fg.name.startsWith (st.isMinor() ? "G shape" : "E shape"))
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
        } while (! plainSpelling (chord));
        const auto name = chordName (root, ct);
        juce::String tones;
        for (int i = 0; i < 3; ++i)
            tones << (i ? ", " : "") << chord[(size_t) i].name() << " (" << chordToneLabel (i, ct) << ")";
        const auto lower = intervalBetween (chord[0], chord[1]), upper = intervalBetween (chord[1], chord[2]);
        q.explanation = name + " = " + tones + ": a " + lower.longName() + " with a " + upper.longName() + " on top.";
        q.listenMidi = midis (chord);
        q.listenAsChord = true;
        q.listenGap = 0.5;

        if (t == "spellTriad")
        {
            q.prompt = "Spell " + name + " (" + chordSymbol (root, ct) + "): click the 3rd and the 5th.";
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
            q.revealMarks = toneMarks (chord, { "R", chordToneLabel (1, ct), chordToneLabel (2, ct) }, 0, 5);
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
    if (t == "earInterval" || t == "earScale" || t == "earTriad") return "listen";
    if (t == "writeStaff" || t == "writeScale") return "write";
    return "click";
}
