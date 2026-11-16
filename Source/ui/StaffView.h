#pragma once
#include "Look.h"
#include "core/Theory.h"
#include <functional>

// A treble staff for guitar (sounds an octave lower than written). Notes are given as sounding pitches
// and drawn as whole notes, one after another. Optionally clickable to write a note.
class StaffView : public juce::Component
{
public:
    void setNotes (std::vector<theory::Note> sounding, std::vector<juce::Colour> colours = {}, bool showLabels = false);
    void setClickable (bool c) { clickable = c; hoverStep = -1000; repaint(); }
    void setAccidental (int a) { accidental = a; repaint(); }
    int getAccidental() const { return accidental; }
    std::function<void (const theory::Note& sounding)> onPlace;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    std::vector<theory::Note> notes;
    std::vector<juce::Colour> colours;
    bool labels = false, clickable = false;
    int accidental = 0;
    int hoverStep = -1000;

    float spacing() const;
    float bottomLineY() const;
    float yForStep (int writtenStep) const;
    int stepForY (float y) const;
    float noteX (int index, int count) const;
    float firstNoteX() const;
    void drawNote (juce::Graphics&, float x, int writtenStep, int acc, juce::Colour c, bool ghost) const;
    void drawClef (juce::Graphics&) const;
};
