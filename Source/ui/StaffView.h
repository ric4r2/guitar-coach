#pragma once
#include "Look.h"
#include "core/Theory.h"
#include <functional>

// A treble staff for guitar (sounds an octave lower than written). Notes are given as sounding pitches
// and drawn as whole notes in columns: one note per column, or a chord stacked in one column.
// Optionally a key signature, labels under the columns, step labels between them, and clickable to write a note.
class StaffView : public juce::Component
{
public:
    // one column per note; labels are the note names
    void setNotes (std::vector<theory::Note> sounding, std::vector<juce::Colour> colours = {}, bool showLabels = false);
    // one column per chord
    void setChords (std::vector<std::vector<theory::Note>> chords, juce::StringArray labels = {}, std::vector<juce::Colour> colours = {});
    void setKeySignature (int fifths) { keyFifths = juce::jlimit (-7, 7, fifths); repaint(); }
    void setStepLabels (juce::StringArray s) { stepLabels = std::move (s); repaint(); }   // between neighbouring columns

    // Rhythm: notes placed in time, with stems, beams, triplets, rests and bar lines (for licks).
    struct RhythmNote
    {
        theory::Note sounding;
        double beat = 0.0, length = 1.0;
        juce::String above;                 // "full", "1/2", "h", "p", "sl."
        juce::Colour colour { 0xff1d1f23 };
    };
    void setRhythm (std::vector<RhythmNote> notes, double totalBeats, int beatsPerBar = 4);
    float xForBeat (double beat) const;       // left edge of the beat's slot
    float headXForBeat (double beat) const;   // where a note head on that beat is drawn

    void setClickable (bool c) { clickable = c; hoverStep = -1000; repaint(); }
    void setAccidental (int a) { accidental = a; repaint(); }
    int getAccidental() const { return accidental; }
    std::function<void (const theory::Note& sounding)> onPlace;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    struct Column
    {
        std::vector<theory::Note> notes;
        juce::Colour colour;
        juce::String label;
    };
    std::vector<Column> columns;
    std::vector<RhythmNote> rhythm;
    double rhythmBeats = 0.0;
    int rhythmBar = 4;
    void paintRhythm (juce::Graphics&) const;
    juce::StringArray stepLabels;
    bool labels = false, clickable = false;
    int accidental = 0, keyFifths = 0;
    int hoverStep = -1000;

    float spacing() const;
    float bottomLineY() const;
    float yForStep (int writtenStep) const;
    int stepForY (float y) const;
    float noteX (int index, int count) const;
    float firstNoteX() const;
    void drawHead (juce::Graphics&, float x, int writtenStep, juce::Colour c, bool ghost, bool filled = false) const;
    void drawAccidental (juce::Graphics&, float x, int writtenStep, int acc, juce::Colour c) const;
    void drawColumn (juce::Graphics&, float x, const Column&) const;
    void drawClef (juce::Graphics&) const;
    void drawKeySignature (juce::Graphics&) const;
};
