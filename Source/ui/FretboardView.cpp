#include "FretboardView.h"

juce::Colour FretboardView::markColour (int code)
{
    switch (code)
    {
        case 1:  return Palette::gold;
        case 2:  return Palette::green;
        case 3:  return Palette::red;
        case 4:  return Palette::blue;
        default: return Palette::cream;
    }
}

juce::Rectangle<float> FretboardView::neckArea() const
{
    auto r = getLocalBounds().toFloat().reduced (6.0f, 4.0f);
    r.removeFromBottom (20.0f);   // fret numbers
    return r.withTrimmedLeft (openWidth());
}

float FretboardView::fretX (int fret) const
{
    // frets get closer up the neck, a bit less than on a real guitar so high frets stay usable
    const auto n = neckArea();
    auto pos = [] (int f) { return 1.0f - std::pow (0.5f, (float) f / 17.0f); };
    return n.getX() + n.getWidth() * pos (fret) / pos (maxFret);
}

float FretboardView::noteX (int fret) const
{
    if (fret == 0)
        return neckArea().getX() - openWidth() * 0.5f;
    return (fretX (fret - 1) + fretX (fret)) * 0.5f;
}

float FretboardView::stringY (int string) const
{
    const auto n = neckArea();
    const float gap = n.getHeight() / 6.0f;
    return n.getY() + gap * ((float) string - 0.5f);
}

void FretboardView::paint (juce::Graphics& g)
{
    const auto n = neckArea();

    // wood
    g.setGradientFill (juce::ColourGradient (Palette::rosewood.brighter (0.15f), n.getX(), n.getY(),
                                             Palette::rosewood.darker (0.3f), n.getX(), n.getBottom(), false));
    g.fillRoundedRectangle (n, 3.0f);

    // inlays
    g.setColour (Palette::cream.withAlpha (0.55f));
    for (int f : { 3, 5, 7, 9, 15, 17, 19, 21 })
        if (f <= maxFret)
            g.fillEllipse (noteX (f) - 5.0f, n.getCentreY() - 5.0f, 10.0f, 10.0f);
    if (maxFret >= 12)
        for (float dy : { -n.getHeight() * 0.25f, n.getHeight() * 0.25f })
            g.fillEllipse (noteX (12) - 5.0f, n.getCentreY() + dy - 5.0f, 10.0f, 10.0f);

    // frets and nut
    for (int f = 1; f <= maxFret; ++f)
    {
        g.setColour (juce::Colour (0xffb9bec6));
        g.fillRect (fretX (f) - 1.5f, n.getY(), 3.0f, n.getHeight());
    }
    g.setColour (Palette::cream);
    g.fillRect (n.getX() - 4.0f, n.getY(), 6.0f, n.getHeight());

    // strings, thicker at the bottom
    for (int s = 1; s <= 6; ++s)
    {
        const float th = 0.8f + (float) s * 0.35f;
        g.setColour (s >= 4 ? juce::Colour (0xffd2b48c) : juce::Colour (0xffe6e8ec));
        g.fillRect (n.getX() - openWidth() + 6.0f, stringY (s) - th * 0.5f, n.getRight() - n.getX() + openWidth() - 6.0f, th);
    }

    // fret numbers
    g.setColour (Palette::dim);
    g.setFont (Fonts::ui (13.0f));
    for (int f = 0; f <= maxFret; ++f)
        g.drawText (juce::String (f), juce::Rectangle<float> (noteX (f) - 14.0f, n.getBottom() + 2.0f, 28.0f, 16.0f), juce::Justification::centred);

    const float r = juce::jmin (13.0f, n.getHeight() / 12.0f + 1.5f);

    // where the note being played can be found
    if (liveMidi >= 0)
        for (auto& p : theory::positionsOf (liveMidi, maxFret))
        {
            g.setColour (Palette::blue.withAlpha (0.35f));
            g.fillEllipse (noteX (p.fret) - r * 1.25f, stringY (p.string) - r * 1.25f, r * 2.5f, r * 2.5f);
        }

    if (clickable && hoverFret >= 0)
    {
        g.setColour (Palette::blue.withAlpha (0.5f));
        g.drawEllipse (noteX (hoverFret) - r, stringY (hoverString) - r, r * 2, r * 2, 2.0f);
    }

    for (auto& m : marks)
    {
        if (m.fret > maxFret)
            continue;
        const float x = noteX (m.fret), y = stringY (m.string);
        const auto c = markColour (m.colour);
        g.setColour (c);
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (Palette::ink.withAlpha (0.6f));
        g.drawEllipse (x - r, y - r, r * 2, r * 2, 1.0f);
        g.setColour (m.colour == 3 || m.colour == 4 ? Palette::text : Palette::ink);
        g.setFont (Fonts::ui (juce::jmin (15.0f, r * 1.15f), true));
        g.drawText (m.label, juce::Rectangle<float> (x - r * 1.4f, y - r, r * 2.8f, r * 2), juce::Justification::centred, false);
    }
}

bool FretboardView::hit (juce::Point<float> p, int& string, int& fret) const
{
    const auto n = neckArea();
    if (p.y < n.getY() || p.y > n.getBottom() || p.x < n.getX() - openWidth() || p.x > n.getRight())
        return false;
    string = juce::jlimit (1, 6, 1 + (int) ((p.y - n.getY()) / (n.getHeight() / 6.0f)));
    fret = 0;
    if (p.x >= n.getX())
        for (int f = 1; f <= maxFret; ++f)
            if (p.x <= fretX (f)) { fret = f; break; }
    return true;
}

void FretboardView::mouseMove (const juce::MouseEvent& e)
{
    if (! clickable) return;
    int s = 0, f = -1;
    if (! hit (e.position, s, f)) f = -1;
    if (s != hoverString || f != hoverFret) { hoverString = s; hoverFret = f; repaint(); }
}

void FretboardView::mouseExit (const juce::MouseEvent&) { hoverFret = -1; repaint(); }

void FretboardView::mouseDown (const juce::MouseEvent& e)
{
    int s = 0, f = 0;
    if (clickable && onClick && hit (e.position, s, f))
        onClick (s, f);
}
