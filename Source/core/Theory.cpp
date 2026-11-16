#include "Theory.h"

namespace theory
{
    static const char* letters = "CDEFGAB";

    juce::String letterName (int letter) { return juce::String::charToString ((juce::juce_wchar) letters[juce::jlimit (0, 6, letter)]); }

    juce::String accidentalText (int acc, bool unicode)
    {
        switch (acc)
        {
            case -2: return unicode ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9d\x84\xab")) : "bb";
            case -1: return unicode ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xad")) : "b";
            case  1: return unicode ? juce::String (juce::CharPointer_UTF8 ("\xe2\x99\xaf")) : "#";
            case  2: return unicode ? juce::String (juce::CharPointer_UTF8 ("\xf0\x9d\x84\xaa")) : "x";
            default: return {};
        }
    }

    juce::String Note::name (bool withOctave, bool unicode) const
    {
        auto s = letterName (letter) + accidentalText (acc, unicode);
        if (withOctave)
            s << octave;
        return s;
    }

    Note noteFromMidi (int midi, bool preferFlats)
    {
        static const int sharpLetter[12] = { 0, 0, 1, 1, 2, 3, 3, 4, 4, 5, 5, 6 };
        static const int sharpAcc[12]    = { 0, 1, 0, 1, 0, 0, 1, 0, 1, 0, 1, 0 };
        static const int flatLetter[12]  = { 0, 1, 1, 2, 2, 3, 4, 4, 5, 5, 6, 6 };
        static const int flatAcc[12]     = { 0,-1, 0,-1, 0, 0,-1, 0,-1, 0,-1, 0 };
        const int pc = mod12 (midi);
        Note n;
        n.letter = preferFlats ? flatLetter[pc] : sharpLetter[pc];
        n.acc = preferFlats ? flatAcc[pc] : sharpAcc[pc];
        n.octave = (midi - naturalPc[n.letter] - n.acc) / 12 - 1;
        return n;
    }

    std::optional<Note> parseNote (const juce::String& textIn)
    {
        auto text = textIn.trim()
                        .replace (juce::CharPointer_UTF8 ("\xe2\x99\xaf"), "#")
                        .replace (juce::CharPointer_UTF8 ("\xe2\x99\xad"), "b");
        if (text.isEmpty())
            return std::nullopt;

        const auto first = juce::CharacterFunctions::toUpperCase (text[0]);
        const int letter = juce::String (letters).indexOfChar (first);
        if (letter < 0)
            return std::nullopt;

        Note n;
        n.letter = letter;
        int i = 1;
        while (i < text.length() && (text[i] == '#' || text[i] == 'b' || text[i] == 'x'))
        {
            n.acc += text[i] == '#' ? 1 : text[i] == 'x' ? 2 : -1;
            ++i;
        }
        const auto rest = text.substring (i);
        n.octave = rest.isEmpty() ? 4 : rest.getIntValue();
        return n;
    }

    juce::String pcName (int pc, bool preferFlats) { return noteFromMidi (60 + mod12 (pc), preferFlats).name(); }

    juce::String pcNameBoth (int pc)
    {
        const auto s = noteFromMidi (60 + mod12 (pc), false);
        if (s.acc == 0)
            return s.name();
        return s.name() + "/" + noteFromMidi (60 + mod12 (pc), true).name();
    }

    static bool isPerfectNumber (int simple) { return simple == 1 || simple == 4 || simple == 5 || simple == 8; }

    static int referenceSemitones (int simple)   // perfect or major
    {
        static const int ref[9] = { 0, 0, 2, 4, 5, 7, 9, 11, 12 };
        return ref[juce::jlimit (1, 8, simple)];
    }

    Interval intervalBetween (const Note& lower, const Note& upper)
    {
        Interval iv;
        iv.number = upper.diatonic() - lower.diatonic() + 1;
        iv.semitones = upper.midi() - lower.midi();

        int simple = iv.number, semis = iv.semitones;
        while (simple > 8) { simple -= 7; semis -= 12; }

        const int diff = semis - referenceSemitones (simple);
        if (isPerfectNumber (simple))
            iv.quality = diff == 0 ? "P" : diff == 1 ? "A" : diff == -1 ? "d" : diff > 0 ? "AA" : "dd";
        else
            iv.quality = diff == 0 ? "M" : diff == -1 ? "m" : diff == 1 ? "A" : diff == -2 ? "d" : diff > 0 ? "AA" : "dd";
        return iv;
    }

    Interval intervalFromShort (const juce::String& s)
    {
        Interval iv;
        iv.quality = s.initialSectionNotContaining ("0123456789");
        iv.number = s.substring (iv.quality.length()).getIntValue();
        int simple = iv.number, octaves = 0;
        while (simple > 8) { simple -= 7; ++octaves; }
        int semis = referenceSemitones (simple);
        if (isPerfectNumber (simple))
            semis += iv.quality == "A" ? 1 : iv.quality == "d" ? -1 : 0;
        else
            semis += iv.quality == "m" ? -1 : iv.quality == "A" ? 1 : iv.quality == "d" ? -2 : 0;
        iv.semitones = semis + 12 * octaves;
        return iv;
    }

    Note transposeUp (const Note& root, const Interval& iv)
    {
        const int targetDiatonic = root.diatonic() + iv.number - 1;
        Note n;
        n.letter = targetDiatonic % 7;
        n.octave = targetDiatonic / 7;
        n.acc = 0;
        n.acc = root.midi() + iv.semitones - n.midi();
        return n;
    }

    juce::String ordinal (int number)
    {
        const int lastTwo = number % 100, last = number % 10;
        const char* suffix = (lastTwo >= 11 && lastTwo <= 13) ? "th" : last == 1 ? "st" : last == 2 ? "nd" : last == 3 ? "rd" : "th";
        return juce::String (number) + suffix;
    }

    juce::String Interval::shortName() const { return quality + juce::String (number); }

    juce::String Interval::longName() const
    {
        if (number == 1 && quality == "P") return "unison";
        if (number == 8 && quality == "P") return "octave";
        if (number == 4 && quality == "A") return "tritone (augmented 4th)";
        if (number == 5 && quality == "d") return "tritone (diminished 5th)";
        juce::String q = quality == "P" ? "perfect" : quality == "M" ? "major" : quality == "m" ? "minor"
                       : quality == "A" ? "augmented" : quality == "d" ? "diminished" : quality;
        return q + " " + ordinal (number);
    }

    const std::vector<Interval>& octaveIntervals()
    {
        static const std::vector<Interval> list = []
        {
            std::vector<Interval> v;
            for (auto s : { "P1", "m2", "M2", "m3", "M3", "P4", "A4", "P5", "m6", "M6", "m7", "M7", "P8" })
                v.push_back (intervalFromShort (s));
            return v;
        }();
        return list;
    }

    juce::String semitonesText (int semitones) { return juce::String (semitones) + (std::abs (semitones) == 1 ? " semitone" : " semitones"); }

    juce::String tonesText (int semitones)
    {
        const int whole = semitones / 2;
        const bool half = (semitones % 2) != 0;
        const juce::String halfSign (juce::CharPointer_UTF8 ("\xc2\xbd"));
        if (semitones == 0) return "0 tones";
        if (whole == 0) return halfSign + " tone";
        juce::String s (whole);
        if (half) s << halfSign;
        return s + (whole == 1 && ! half ? " tone" : " tones");
    }

    juce::String stringName (int string)
    {
        static const char* names[7] = { "", "1st string (high E)", "2nd string (B)", "3rd string (G)",
                                        "4th string (D)", "5th string (A)", "6th string (low E)" };
        return names[juce::jlimit (1, 6, string)];
    }

    std::vector<FretPos> positionsOf (int midi, int maxFret)
    {
        std::vector<FretPos> out;
        for (int s = 1; s <= numStrings; ++s)
        {
            const int f = midi - openMidi[s];
            if (f >= 0 && f <= maxFret)
                out.push_back ({ s, f });
        }
        return out;
    }

    std::vector<FretPos> positionsOfPc (int pc, int maxFret)
    {
        std::vector<FretPos> out;
        for (int s = 1; s <= numStrings; ++s)
            for (int f = 0; f <= maxFret; ++f)
                if (mod12 (openMidi[s] + f) == mod12 (pc))
                    out.push_back ({ s, f });
        return out;
    }

    juce::String stepName (int semitones)
    {
        semitones = std::abs (semitones);
        return semitones == 1 ? "Half step" : semitones == 2 ? "Whole step" : "Neither";
    }

    // ---------------------------------------------------------------- scales and keys

    static std::vector<Interval> ivs (std::initializer_list<const char*> names)
    {
        std::vector<Interval> v;
        for (auto n : names)
            v.push_back (intervalFromShort (n));
        return v;
    }

    bool ScaleType::isMinor() const
    {
        for (auto& i : intervals)
            if (i.number == 3)
                return i.semitones == 3;
        return false;
    }

    const std::vector<ScaleType>& scaleTypes()
    {
        static const std::vector<ScaleType> list {
            { "major",           "Major",             ivs ({ "P1", "M2", "M3", "P4", "P5", "M6", "M7" }), 0 },
            { "naturalMinor",    "Natural minor",     ivs ({ "P1", "M2", "m3", "P4", "P5", "m6", "m7" }), 3 },
            { "harmonicMinor",   "Harmonic minor",    ivs ({ "P1", "M2", "m3", "P4", "P5", "m6", "M7" }), 3 },
            { "melodicMinor",    "Melodic minor",     ivs ({ "P1", "M2", "m3", "P4", "P5", "M6", "M7" }), 3 },
            { "majorPentatonic", "Major pentatonic",  ivs ({ "P1", "M2", "M3", "P5", "M6" }), 0 },
            { "minorPentatonic", "Minor pentatonic",  ivs ({ "P1", "m3", "P4", "P5", "m7" }), 3 },
            { "blues",           "Blues",             ivs ({ "P1", "m3", "P4", "d5", "P5", "m7" }), 3 },
            { "mixolydian",      "Mixolydian",        ivs ({ "P1", "M2", "M3", "P4", "P5", "M6", "m7" }), 5 },
            { "dorian",          "Dorian",            ivs ({ "P1", "M2", "m3", "P4", "P5", "M6", "m7" }), 10 },
            { "phrygian",        "Phrygian",          ivs ({ "P1", "m2", "m3", "P4", "P5", "m6", "m7" }), 8 },
            { "lydian",          "Lydian",            ivs ({ "P1", "M2", "M3", "A4", "P5", "M6", "M7" }), 7 },
            { "locrian",         "Locrian",           ivs ({ "P1", "m2", "m3", "P4", "d5", "m6", "m7" }), 1 },
            { "arpMaj7",         "Maj7 arpeggio",     ivs ({ "P1", "M3", "P5", "M7" }), 0 },
            { "arpDom7",         "Dominant 7 arpeggio", ivs ({ "P1", "M3", "P5", "m7" }), 0 },
            { "arpMin7",         "Minor 7 arpeggio",  ivs ({ "P1", "m3", "P5", "m7" }), 0 },
            { "arpHalfDim",      "m7b5 arpeggio",     ivs ({ "P1", "m3", "d5", "m7" }), 0 } };
        return list;
    }

    const ScaleType& scaleType (const juce::String& id)
    {
        for (auto& t : scaleTypes())
            if (t.id == id)
                return t;
        return scaleTypes().front();
    }

    std::vector<Note> scaleNotes (const Note& root, const ScaleType& t, bool withOctave)
    {
        std::vector<Note> out;
        for (auto& i : t.intervals)
            out.push_back (transposeUp (root, i));
        if (withOctave)
            out.push_back (octaveShift (root, 1));
        return out;
    }

    juce::String degreeLabel (const Interval& iv)
    {
        int simple = iv.number;
        while (simple > 7) simple -= 7;
        const bool perfect = simple == 1 || simple == 4 || simple == 5;
        juce::String prefix;
        if (iv.quality == "m" || (iv.quality == "d" && perfect)) prefix = "b";
        else if (iv.quality == "d") prefix = "bb";
        else if (iv.quality == "A") prefix = "#";
        return prefix + juce::String (simple);
    }

    juce::String stepPattern (const std::vector<Note>& notes)
    {
        juce::StringArray steps;
        for (size_t i = 1; i < notes.size(); ++i)
        {
            const int d = notes[i].midi() - notes[i - 1].midi();
            steps.add (d == 1 ? "H" : d == 2 ? "W" : d == 3 ? juce::String (juce::CharPointer_UTF8 ("1\xc2\xbd")) : juce::String (d));
        }
        return steps.joinIntoString (" ");
    }

    static const char* majorKeys[15] = { "Cb", "Gb", "Db", "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#" };
    static const char* minorKeys[15] = { "Ab", "Eb", "Bb", "F", "C", "G", "D", "A", "E", "B", "F#", "C#", "G#", "D#", "A#" };

    int keyFifths (const Note& tonic, bool minor)
    {
        const auto major = minor ? transposeUp (tonic, intervalFromShort ("m3")) : tonic;
        int sum = 0;
        for (auto& n : scaleNotes (major, scaleType ("major"), false))
            sum += n.acc;
        return sum;
    }

    Note keyTonic (int fifths, bool minor)
    {
        const int i = juce::jlimit (-7, 7, fifths) + 7;
        return *parseNote (minor ? minorKeys[i] : majorKeys[i]);
    }

    juce::String keyName (int fifths, bool minor) { return keyTonic (fifths, minor).name() + (minor ? " minor" : " major"); }

    std::vector<Note> keySignature (int fifths)
    {
        static const char* sharpOrder = "FCGDAEB";
        static const char* flatOrder = "BEADGCF";
        std::vector<Note> out;
        for (int i = 0; i < std::abs (juce::jlimit (-7, 7, fifths)); ++i)
        {
            const char letter = fifths > 0 ? sharpOrder[i] : flatOrder[i];
            out.push_back ({ juce::String (letters).indexOfChar ((juce::juce_wchar) letter), fifths > 0 ? 1 : -1, 4 });
        }
        return out;
    }

    juce::String keySignatureText (int fifths)
    {
        if (fifths == 0)
            return "no sharps or flats";
        const int n = std::abs (fifths);
        juce::String s = juce::String (n) + (fifths > 0 ? (n == 1 ? " sharp" : " sharps") : (n == 1 ? " flat" : " flats")) + ":";
        for (auto& note : keySignature (fifths))
            s << " " << note.name();
        return s;
    }

    juce::String fifthsLabel (int fifths)
    {
        if (fifths == 0) return "0";
        return juce::String (std::abs (fifths)) + accidentalText (fifths > 0 ? 1 : -1);
    }

    // ---------------------------------------------------------------- triads

    const std::vector<ChordType>& triadTypes()
    {
        static const std::vector<ChordType> list {
            { "major",      "major",      "",  ivs ({ "P1", "M3", "P5" }) },
            { "minor",      "minor",      "m", ivs ({ "P1", "m3", "P5" }) },
            { "diminished", "diminished", juce::String (juce::CharPointer_UTF8 ("\xc2\xb0")), ivs ({ "P1", "m3", "d5" }) },
            { "augmented",  "augmented",  "+", ivs ({ "P1", "M3", "A5" }) } };
        return list;
    }

    const std::vector<ChordType>& seventhTypes()
    {
        static const std::vector<ChordType> list {
            { "dominant7",      "dominant 7th",      "7",    ivs ({ "P1", "M3", "P5", "m7" }) },
            { "major7",         "major 7th",         "maj7", ivs ({ "P1", "M3", "P5", "M7" }) },
            { "minor7",         "minor 7th",         "m7",   ivs ({ "P1", "m3", "P5", "m7" }) },
            { "halfDiminished", "half-diminished",   juce::String (juce::CharPointer_UTF8 ("m7\xe2\x99\xad" "5")), ivs ({ "P1", "m3", "d5", "m7" }) },
            { "diminished7",    "diminished 7th",    juce::String (juce::CharPointer_UTF8 ("\xc2\xb0" "7")), ivs ({ "P1", "m3", "d5", "d7" }) } };
        return list;
    }

    const ChordType& chordType (const juce::String& id)
    {
        for (auto& t : triadTypes())
            if (t.id == id)
                return t;
        for (auto& t : seventhTypes())
            if (t.id == id)
                return t;
        return triadTypes().front();
    }

    std::vector<Note> chordNotes (const Note& root, const ChordType& t)
    {
        std::vector<Note> out;
        for (auto& i : t.intervals)
            out.push_back (transposeUp (root, i));
        return out;
    }

    std::vector<Note> invert (std::vector<Note> notes, int inversion)
    {
        for (int i = 0; i < inversion && ! notes.empty(); ++i)
        {
            auto low = notes.front();
            notes.erase (notes.begin());
            while (! notes.empty() && low.midi() <= notes.back().midi())
                low.octave += 1;
            notes.push_back (low);
        }
        return notes;
    }

    juce::String chordName (const Note& root, const ChordType& t) { return root.name() + " " + t.name; }
    juce::String chordSymbol (const Note& root, const ChordType& t) { return root.name() + t.symbol; }

    juce::String inversionName (int inversion)
    {
        return inversion == 0 ? "root position" : inversion == 1 ? "1st inversion" : inversion == 2 ? "2nd inversion" : ordinal (inversion) + " inversion";
    }

    juce::String chordToneLabel (int index, const ChordType& t)
    {
        if (index <= 0) return "R";
        return degreeLabel (t.intervals[(size_t) juce::jlimit (0, (int) t.intervals.size() - 1, index)]);
    }
}

namespace theory
{
    // ---------------------------------------------------------------- diatonic harmony

    const ChordType* identifyChord (const std::vector<Note>& notes)
    {
        if (notes.size() < 3)
            return nullptr;
        std::vector<int> semis;
        for (size_t i = 1; i < notes.size(); ++i)
            semis.push_back (mod12 (notes[i].midi() - notes[0].midi()));
        auto matches = [&] (const ChordType& t)
        {
            if (t.intervals.size() != notes.size())
                return false;
            for (size_t i = 1; i < t.intervals.size(); ++i)
                if (mod12 (t.intervals[i].semitones) != semis[i - 1])
                    return false;
            return true;
        };
        for (auto& t : triadTypes())
            if (matches (t))
                return &t;
        for (auto& t : seventhTypes())
            if (matches (t))
                return &t;
        return nullptr;
    }

    DiatonicChord diatonicChord (const Note& tonic, int degree, bool seventh)
    {
        DiatonicChord c;
        c.degree = ((degree % 7) + 7) % 7;
        const auto scale = scaleNotes (tonic, scaleType ("major"), false);
        for (int k = 0; k < (seventh ? 4 : 3); ++k)
        {
            const int i = c.degree + 2 * k;
            auto n = scale[(size_t) (i % 7)];
            n.octave += i / 7;
            c.notes.push_back (n);
        }
        c.root = c.notes.front();
        c.type = identifyChord (c.notes);
        return c;
    }

    juce::String romanNumeral (int degree, const ChordType& t)
    {
        static const char* upper[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
        static const char* lower[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
        const int d = ((degree % 7) + 7) % 7;
        if (t.intervals.size() == 3)
        {
            if (t.id == "major") return upper[d];
            if (t.id == "minor") return lower[d];
            if (t.id == "diminished") return juce::String (lower[d]) + juce::String (juce::CharPointer_UTF8 ("\xc2\xb0"));
            return juce::String (upper[d]) + "+";
        }
        return juce::String (upper[d]) + t.symbol;
    }

    juce::String harmonicFunction (int degree)
    {
        switch (((degree % 7) + 7) % 7)
        {
            case 0: case 2: case 5: return "tonic";
            case 1: case 3:         return "subdominant";
            default:                return "dominant";
        }
    }

    const std::vector<Progression>& commonProgressions()
    {
        static const std::vector<Progression> list {
            { "I - IV - V - I",      { 0, 3, 4, 0 }, false },
            { "I - vi - IV - V",     { 0, 5, 3, 4 }, false },
            { "I - V - vi - IV",     { 0, 4, 5, 3 }, false },
            { "I - IV - I - V",      { 0, 3, 0, 4 }, false },
            { "vi - IV - I - V",     { 5, 3, 0, 4 }, false },
            { "IIm7 - V7 - Imaj7",   { 1, 4, 0 },    true },
            { "Imaj7 - VIm7 - IIm7 - V7", { 0, 5, 1, 4 }, true },
            { "IIIm7 - VIm7 - IIm7 - V7", { 2, 5, 1, 4 }, true } };
        return list;
    }
}

namespace theory
{
    // ---------------------------------------------------------------- modes, borrowed chords, tensions

    const std::vector<Mode>& majorModes()
    {
        static const std::vector<Mode> list {
            { 0, "Ionian", "major" }, { 1, "Dorian", "dorian" }, { 2, "Phrygian", "phrygian" }, { 3, "Lydian", "lydian" },
            { 4, "Mixolydian", "mixolydian" }, { 5, "Aeolian", "naturalMinor" }, { 6, "Locrian", "locrian" } };
        return list;
    }

    DiatonicChord secondaryDominant (const Note& tonic, int targetDegree)
    {
        const auto target = diatonicChord (tonic, targetDegree, false);
        DiatonicChord c;
        c.degree = targetDegree;
        c.root = transposeUp (target.root, intervalFromShort ("P5"));
        c.root.octave = 3;
        c.type = &chordType ("dominant7");
        c.notes = chordNotes (c.root, *c.type);
        return c;
    }

    std::vector<BorrowedChord> borrowedChords (const Note& tonic)
    {
        // from the parallel minor (same root, natural minor): bIII, iv, bVI, bVII
        const auto minor = scaleNotes (tonic, scaleType ("naturalMinor"), false);
        auto build = [&] (int degree, const char* type, const char* numeral)
        {
            BorrowedChord b;
            b.numeral = numeral;
            b.root = minor[(size_t) degree];
            b.type = &chordType (type);
            return b;
        };
        return { build (2, "major", "bIII"), build (3, "minor", "iv"), build (5, "major", "bVI"), build (6, "major", "bVII") };
    }

    Note tritoneSubstitute (const Note& dominantRoot)
    {
        // a diminished 5th up gives the usual flat spelling (G -> Db)
        auto n = transposeUp (dominantRoot, intervalFromShort ("d5"));
        if (std::abs (n.acc) > 1 || (n.acc == -1 && (n.letter == 3 || n.letter == 0)))
            n = noteFromMidi (n.midi(), false);
        return n;
    }

    const std::vector<Tension>& dominantTensions()
    {
        static const std::vector<Tension> list {
            { "9", "M9" }, { "b9", "m9" }, { "#9", "A9" }, { "#11", "A11" }, { "13", "M13" }, { "b13", "m13" } };
        return list;
    }

    const std::vector<OctaveShape>& octaveShapes()
    {
        // the root on the lower string, the octave on the higher one, `frets` further up the neck (negative = down)
        static const std::vector<OctaveShape> list {
            { "E", 6, 4, 2 }, { "D", 4, 2, 3 }, { "C", 5, 2, -2 }, { "A", 5, 3, 2 }, { "G", 6, 3, -3 } };
        return list;
    }
}
