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
}
