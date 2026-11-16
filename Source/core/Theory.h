#pragma once
#include <juce_core/juce_core.h>
#include <optional>
#include <vector>

// Music theory engine: spelled notes, intervals, guitar fretboard. No UI, no audio.
namespace theory
{
    constexpr int naturalPc[7] = { 0, 2, 4, 5, 7, 9, 11 };   // C D E F G A B

    inline int mod12 (int x) { return ((x % 12) + 12) % 12; }

    // A note with its spelling: letter 0..6 (C..B), accidental -2..+2 (flats/sharps), octave (C4 = middle C).
    struct Note
    {
        int letter = 0, acc = 0, octave = 4;

        int midi() const      { return 12 * (octave + 1) + naturalPc[letter] + acc; }
        int pc() const        { return mod12 (naturalPc[letter] + acc); }
        int diatonic() const  { return letter + 7 * octave; }   // staff step: one per line or space

        // "C♯4" (unicode) or "C#4"
        juce::String name (bool withOctave = false, bool unicode = true) const;

        bool operator== (const Note& o) const { return letter == o.letter && acc == o.acc && octave == o.octave; }
        bool operator!= (const Note& o) const { return ! (*this == o); }
    };

    Note noteFromMidi (int midi, bool preferFlats = false);
    std::optional<Note> parseNote (const juce::String& text);   // "E2", "Bb3", "F#4", "C♯5"
    juce::String accidentalText (int acc, bool unicode = true);
    juce::String pcName (int pc, bool preferFlats = false);
    juce::String pcNameBoth (int pc);                            // "C♯/D♭" for black keys, "C" for white
    juce::String letterName (int letter);

    // Intervals
    struct Interval
    {
        int number = 1;        // 1 = unison, 3 = third, 8 = octave
        int semitones = 0;
        juce::String quality;  // "P", "M", "m", "A", "d"

        juce::String shortName() const;   // "m3", "P5", "A4"
        juce::String longName() const;    // "minor 3rd", "perfect 5th", "tritone (augmented 4th)"
    };

    Interval intervalBetween (const Note& lower, const Note& upper);
    Interval intervalFromShort (const juce::String& shortName);   // "m3" -> {3, 3, "m"}
    Note transposeUp (const Note& root, const Interval& interval);
    juce::String semitonesText (int semitones);                  // 1 -> "1 semitone", 3 -> "3 semitones"
    juce::String tonesText (int semitones);                      // 3 -> "1½ tones"
    juce::String ordinal (int number);                           // 3 -> "3rd"

    // the 13 intervals inside one octave, with their usual spelling (tritone as A4)
    const std::vector<Interval>& octaveIntervals();

    // Guitar in standard tuning. String 1 = high E, string 6 = low E.
    constexpr int numStrings = 6;
    constexpr int openMidi[7] = { 0, 64, 59, 55, 50, 45, 40 };   // index by string number
    inline int fretMidi (int string, int fret) { return openMidi[string] + fret; }
    juce::String stringName (int string);                        // "6th string (low E)"

    struct FretPos { int string = 6, fret = 0; };
    std::vector<FretPos> positionsOf (int midi, int maxFret);
    std::vector<FretPos> positionsOfPc (int pc, int maxFret);

    // Half and whole steps between two pitches: 1 = half, 2 = whole, other = neither
    juce::String stepName (int semitones);

    // ---------------------------------------------------------------- scales and keys
    struct ScaleType
    {
        juce::String id, name;
        std::vector<Interval> intervals;   // from the root, ascending, without the octave
        int parent = 0;                    // semitones up to the major scale with the same notes (minor: 3, Mixolydian: 5)
        bool isMinor() const;              // has a minor 3rd: its relative major is 3 semitones up
        int size() const { return (int) intervals.size(); }
    };

    // major, naturalMinor, harmonicMinor, melodicMinor, majorPentatonic, minorPentatonic, blues, mixolydian
    const std::vector<ScaleType>& scaleTypes();
    const ScaleType& scaleType (const juce::String& id);              // unknown ids give major
    std::vector<Note> scaleNotes (const Note& root, const ScaleType&, bool withOctave = true);
    juce::String degreeLabel (const Interval&);                       // "1", "b3", "#4"
    juce::String stepPattern (const std::vector<Note>& ascending);     // "W W H W W W H"

    // Key signatures as positions on the circle of fifths: +3 = three sharps, -2 = two flats.
    int keyFifths (const Note& tonic, bool minor);                    // may go past 7 for theoretical keys
    Note keyTonic (int fifths, bool minor);                           // -7..7, octave 4
    juce::String keyName (int fifths, bool minor);                    // "E♭ major", "C♯ minor"
    std::vector<Note> keySignature (int fifths);                       // the sharps or flats, in order
    juce::String keySignatureText (int fifths);                       // "3 sharps: F♯ C♯ G♯"
    juce::String fifthsLabel (int fifths);                            // "3♯", "2♭", "0"

    // ---------------------------------------------------------------- triads
    struct ChordType
    {
        juce::String id, name, symbol;     // "minor", "m"
        std::vector<Interval> intervals;
    };
    const std::vector<ChordType>& triadTypes();                        // major, minor, diminished, augmented
    const std::vector<ChordType>& seventhTypes();                      // dominant7, major7, minor7, halfDiminished, diminished7
    const ChordType& chordType (const juce::String& id);              // triads and sevenths
    std::vector<Note> chordNotes (const Note& root, const ChordType&);  // root position, close
    std::vector<Note> invert (std::vector<Note> rootPosition, int inversion);   // lowest notes go up an octave
    juce::String chordName (const Note& root, const ChordType&);       // "C♯ minor"
    juce::String chordSymbol (const Note& root, const ChordType&);     // "C♯m"
    juce::String inversionName (int inversion);                        // "root position", "1st inversion"
    juce::String chordToneLabel (int index, const ChordType&);         // "R", "3", "b3", "5", "b5", "#5"

    // ---------------------------------------------------------------- diatonic harmony (major keys)
    struct DiatonicChord
    {
        int degree = 0;                    // 0 = I ... 6 = vii
        Note root;
        const ChordType* type = nullptr;
        std::vector<Note> notes;
    };
    // the chord built on a degree of a major key, by stacking 3rds from the scale (three or four notes)
    DiatonicChord diatonicChord (const Note& tonic, int degree, bool seventh);
    const ChordType* identifyChord (const std::vector<Note>& stackedThirds);   // nullptr when unknown
    juce::String romanNumeral (int degree, const ChordType&);  // triads: I ii iii IV V vi vii°; sevenths: Imaj7 IIm7 V7 VIIm7b5
    juce::String harmonicFunction (int degree);                // "tonic", "subdominant", "dominant"

    struct Progression { juce::String name; std::vector<int> degrees; bool sevenths = false; };
    const std::vector<Progression>& commonProgressions();      // I-IV-V-I, I-vi-IV-V, I-V-vi-IV, II-V-I, I-vi-ii-V ...

    // a note an octave higher or lower
    inline Note octaveShift (Note n, int octaves) { n.octave += octaves; return n; }
}
