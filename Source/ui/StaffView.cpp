#include "StaffView.h"

using theory::Note;

static constexpr int bottomLineStep = 2 + 7 * 4;   // written E4
static constexpr int topLineStep = bottomLineStep + 8;   // written F5

void StaffView::setNotes (std::vector<Note> sounding, std::vector<juce::Colour> c, bool showLabels)
{
    notes = std::move (sounding);
    colours = std::move (c);
    labels = showLabels;
    repaint();
}

float StaffView::spacing() const { return juce::jlimit (8.0f, 16.0f, getHeight() / 14.0f); }
float StaffView::bottomLineY() const { return getHeight() * 0.5f + 2.0f * spacing() + (labels ? -spacing() : 0.0f); }
float StaffView::yForStep (int step) const { return bottomLineY() - (float) (step - bottomLineStep) * spacing() * 0.5f; }
int StaffView::stepForY (float y) const { return bottomLineStep + juce::roundToInt ((bottomLineY() - y) / (spacing() * 0.5f)); }
float StaffView::firstNoteX() const { return spacing() * 7.0f; }

float StaffView::noteX (int index, int count) const
{
    const float left = firstNoteX(), right = (float) getWidth() - spacing() * 2.0f;
    if (count <= 1)
        return (left + right) * 0.5f;
    const float span = juce::jmin (right - left, (float) count * spacing() * 6.0f);
    const float start = (left + right) * 0.5f - span * 0.5f;
    return start + span * ((float) index + 0.5f) / (float) count;
}

void StaffView::drawClef (juce::Graphics& g) const
{
    const float sp = spacing();
    juce::GlyphArrangement ga;
    ga.addLineOfText (Fonts::music (100.0f), juce::String (juce::CharPointer_UTF8 ("\xf0\x9d\x84\x9e")), 0.0f, 0.0f);
    juce::Path p;
    ga.createPath (p);
    if (p.isEmpty())
        return;
    // the clef reaches about 1.6 spaces below the bottom line and 1.8 above the top line
    const auto target = juce::Rectangle<float> (sp * 0.8f, yForStep (topLineStep) - sp * 1.8f, sp * 3.2f, sp * 7.4f);
    p.applyTransform (p.getTransformToScaleToFit (target, true, juce::Justification::centred));
    g.setColour (Palette::ink);
    g.fillPath (p);
    g.setFont (Fonts::ui (sp * 1.1f, true));
    g.drawText ("8", juce::Rectangle<float> (target.getCentreX() - sp, target.getBottom() - sp * 0.2f, sp * 2.0f, sp * 1.2f), juce::Justification::centred);
}

void StaffView::drawNote (juce::Graphics& g, float x, int step, int acc, juce::Colour c, bool ghost) const
{
    const float sp = spacing();
    const float y = yForStep (step);

    // ledger lines
    g.setColour (ghost ? Palette::ink.withAlpha (0.35f) : Palette::ink);
    for (int s = bottomLineStep - 2; s >= step; s -= 2)
        g.fillRect (x - sp * 1.1f, yForStep (s) - 0.75f, sp * 2.2f, 1.5f);
    for (int s = topLineStep + 2; s <= step; s += 2)
        g.fillRect (x - sp * 1.1f, yForStep (s) - 0.75f, sp * 2.2f, 1.5f);

    // whole note: an oval ring with a tilted hollow
    const auto colour = ghost ? c.withAlpha (0.45f) : c;
    juce::Path head;
    head.addEllipse (-sp * 0.72f, -sp * 0.5f, sp * 1.44f, sp * 1.0f);
    juce::Path hole;
    hole.addEllipse (-sp * 0.42f, -sp * 0.28f, sp * 0.84f, sp * 0.56f);
    hole.applyTransform (juce::AffineTransform::rotation (-0.6f));
    head.addPath (hole);
    head.setUsingNonZeroWinding (false);
    g.setColour (colour);
    g.fillPath (head, juce::AffineTransform::translation (x, y));

    if (acc != 0)
    {
        g.setFont (Fonts::music (sp * 2.6f));
        g.drawText (theory::accidentalText (acc), juce::Rectangle<float> (x - sp * 2.9f, y - sp * 1.6f, sp * 1.8f, sp * 2.6f),
                    juce::Justification::centred, false);
    }
}

void StaffView::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (Palette::cream);
    g.fillRoundedRectangle (r, 8.0f);

    const float sp = spacing();
    g.setColour (Palette::ink);
    for (int i = 0; i < 5; ++i)
        g.fillRect (sp * 0.6f, yForStep (bottomLineStep + 2 * i) - 0.6f, r.getWidth() - sp * 1.2f, 1.2f);
    g.fillRect (sp * 0.6f, yForStep (topLineStep), 1.5f, yForStep (bottomLineStep) - yForStep (topLineStep));
    g.fillRect (r.getWidth() - sp * 0.6f - 1.5f, yForStep (topLineStep), 1.5f, yForStep (bottomLineStep) - yForStep (topLineStep));
    drawClef (g);

    const int n = (int) notes.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& note = notes[(size_t) i];
        const int written = note.diatonic() + 7;
        const auto c = i < (int) colours.size() ? colours[(size_t) i] : Palette::ink;
        drawNote (g, noteX (i, n), written, note.acc, c, false);
        if (labels)
        {
            g.setColour (Palette::ink.withAlpha (0.8f));
            g.setFont (Fonts::ui (sp * 1.25f, true));
            g.drawText (note.name(), juce::Rectangle<float> (noteX (i, n) - sp * 2, r.getBottom() - sp * 2.0f, sp * 4, sp * 1.6f),
                        juce::Justification::centred);
        }
    }

    if (clickable && hoverStep > -1000)
        drawNote (g, noteX (n, n + 1), hoverStep, accidental, Palette::blue, true);
}

void StaffView::mouseMove (const juce::MouseEvent& e)
{
    if (! clickable)
        return;
    const int s = juce::jlimit (bottomLineStep - 8, topLineStep + 4, stepForY ((float) e.y));
    if (s != hoverStep) { hoverStep = s; repaint(); }
}

void StaffView::mouseExit (const juce::MouseEvent&)
{
    hoverStep = -1000;
    repaint();
}

void StaffView::mouseDown (const juce::MouseEvent& e)
{
    if (! clickable || ! onPlace)
        return;
    const int written = juce::jlimit (bottomLineStep - 8, topLineStep + 4, stepForY ((float) e.y));
    Note n { written % 7, accidental, written / 7 - 1 };   // sounding: an octave below written
    onPlace (n);
}
