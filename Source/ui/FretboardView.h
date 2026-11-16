#pragma once
#include "Look.h"
#include "core/Lessons.h"

// A guitar neck seen from the player, high E string on top (as in tab), open strings left of the nut.
class FretboardView : public juce::Component
{
public:
    void setMaxFret (int f) { maxFret = juce::jlimit (5, 22, f); repaint(); }
    int getMaxFret() const { return maxFret; }
    void setMarks (std::vector<FretMark> m) { marks = std::move (m); repaint(); }
    void setLiveMidi (int midi) { if (midi != liveMidi) { liveMidi = midi; repaint(); } }   // -1 = none
    void setClickable (bool c) { clickable = c; repaint(); }
    std::function<void (int string, int fret)> onClick;

    void paint (juce::Graphics&) override;
    void mouseMove (const juce::MouseEvent&) override;
    void mouseExit (const juce::MouseEvent&) override;
    void mouseDown (const juce::MouseEvent&) override;

    static juce::Colour markColour (int code);

private:
    int maxFret = 12, liveMidi = -1;
    bool clickable = false;
    std::vector<FretMark> marks;
    int hoverString = 0, hoverFret = -1;

    juce::Rectangle<float> neckArea() const;
    float openWidth() const { return 34.0f; }
    float fretX (int fret) const;            // x of the fret wire after this fret (0 = nut)
    float noteX (int fret) const;            // centre of the space where this fret is played
    float stringY (int string) const;
    bool hit (juce::Point<float> p, int& string, int& fret) const;
};
