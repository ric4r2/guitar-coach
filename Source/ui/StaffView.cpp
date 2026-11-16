#include "StaffView.h"

using theory::Note;

static constexpr int bottomLineStep = 2 + 7 * 4;   // written E4
static constexpr int topLineStep = bottomLineStep + 8;   // written F5

void StaffView::setNotes (std::vector<Note> sounding, std::vector<juce::Colour> c, bool showLabels)
{
    columns.clear();
    for (size_t i = 0; i < sounding.size(); ++i)
        columns.push_back ({ { sounding[i] }, i < c.size() ? c[i] : Palette::ink, sounding[i].name() });
    labels = showLabels;
    stepLabels.clear();
    repaint();
}

void StaffView::setChords (std::vector<std::vector<Note>> chords, juce::StringArray l, std::vector<juce::Colour> c)
{
    columns.clear();
    for (size_t i = 0; i < chords.size(); ++i)
        columns.push_back ({ chords[i], i < c.size() ? c[i] : Palette::ink, l[(int) i] });
    labels = ! l.isEmpty();
    stepLabels.clear();
    repaint();
}

float StaffView::spacing() const { return juce::jlimit (8.0f, 16.0f, getHeight() / 14.0f); }
float StaffView::bottomLineY() const { return getHeight() * 0.5f + 2.0f * spacing() + (labels ? -spacing() * 0.6f : 0.0f); }
float StaffView::yForStep (int step) const { return bottomLineY() - (float) (step - bottomLineStep) * spacing() * 0.5f; }
int StaffView::stepForY (float y) const { return bottomLineStep + juce::roundToInt ((bottomLineY() - y) / (spacing() * 0.5f)); }
float StaffView::firstNoteX() const { return spacing() * (7.0f + (keyFifths != 0 ? 1.1f * (float) std::abs (keyFifths) + 0.8f : 0.0f)); }

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

void StaffView::drawAccidental (juce::Graphics& g, float cx, int step, int acc, juce::Colour c) const
{
    const float sp = spacing();
    const float y = yForStep (step);
    g.setColour (c);
    g.setFont (Fonts::music (sp * 2.6f));
    g.drawText (theory::accidentalText (acc), juce::Rectangle<float> (cx - sp * 0.9f, y - sp * 1.6f, sp * 1.8f, sp * 2.6f), juce::Justification::centred, false);
}

void StaffView::drawKeySignature (juce::Graphics& g) const
{
    // written positions in the treble clef
    static const int sharpSteps[7] = { 38, 35, 39, 36, 33, 37, 34 };   // F5 C5 G5 D5 A4 E5 B4
    static const int flatSteps[7]  = { 34, 37, 33, 36, 32, 35, 31 };   // B4 E5 A4 D5 G4 C5 F4
    const float sp = spacing();
    for (int i = 0; i < std::abs (keyFifths); ++i)
        drawAccidental (g, sp * (4.9f + 1.1f * (float) i) + sp * 0.9f, keyFifths > 0 ? sharpSteps[i] : flatSteps[i], keyFifths > 0 ? 1 : -1, Palette::ink);
}

void StaffView::drawHead (juce::Graphics& g, float x, int step, juce::Colour c, bool ghost) const
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
    juce::Path head;
    head.addEllipse (-sp * 0.72f, -sp * 0.5f, sp * 1.44f, sp * 1.0f);
    juce::Path hole;
    hole.addEllipse (-sp * 0.42f, -sp * 0.28f, sp * 0.84f, sp * 0.56f);
    hole.applyTransform (juce::AffineTransform::rotation (-0.6f));
    head.addPath (hole);
    head.setUsingNonZeroWinding (false);
    g.setColour (ghost ? c.withAlpha (0.45f) : c);
    g.fillPath (head, juce::AffineTransform::translation (x, y));
}

void StaffView::drawColumn (juce::Graphics& g, float x, const Column& col) const
{
    const float sp = spacing();
    // accidentals, top note first: move one further left when it would collide with the one above
    std::vector<std::pair<int, int>> placed;   // written step, slot
    auto notes = col.notes;
    std::sort (notes.begin(), notes.end(), [] (auto& a, auto& b) { return a.diatonic() > b.diatonic(); });
    for (auto& n : notes)
    {
        const int step = n.diatonic() + 7;
        drawHead (g, x, step, col.colour, false);
        if (n.acc == 0)
            continue;
        int slot = 0;
        for (auto& [s, sl] : placed)
            if (sl == slot && std::abs (s - step) < 6)
                ++slot;
        placed.push_back ({ step, slot });
        drawAccidental (g, x - sp * (2.0f + 1.4f * (float) slot), step, n.acc, col.colour);
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
    drawKeySignature (g);

    const int n = (int) columns.size();
    for (int i = 0; i < n; ++i)
    {
        const auto& col = columns[(size_t) i];
        const float x = noteX (i, n);
        drawColumn (g, x, col);
        if (labels && col.label.isNotEmpty())
        {
            g.setColour (Palette::ink.withAlpha (0.8f));
            g.setFont (Fonts::ui (sp * 1.25f, true));
            g.drawText (col.label, juce::Rectangle<float> (x - sp * 3, r.getBottom() - sp * 1.9f, sp * 6, sp * 1.6f), juce::Justification::centred);
        }
    }

    // W / H between the notes, along the top
    g.setFont (Fonts::ui (sp * 1.1f, true));
    for (int i = 0; i + 1 < n && i < stepLabels.size(); ++i)
    {
        const float x = (noteX (i, n) + noteX (i + 1, n)) * 0.5f;
        g.setColour (stepLabels[i] == "H" ? Palette::red.darker (0.2f) : Palette::ink.withAlpha (0.7f));
        g.drawText (stepLabels[i], juce::Rectangle<float> (x - sp * 1.5f, sp * 0.3f, sp * 3.0f, sp * 1.4f), juce::Justification::centred);
    }

    if (clickable && hoverStep > -1000)
    {
        const float x = noteX (n, n + 1);
        drawHead (g, x, hoverStep, Palette::blue, true);
        if (accidental != 0)
            drawAccidental (g, x - sp * 2.0f, hoverStep, accidental, Palette::blue.withAlpha (0.6f));
    }
}

void StaffView::mouseMove (const juce::MouseEvent& e)
{
    if (! clickable)
        return;
    const int s = juce::jlimit (bottomLineStep - 8, topLineStep + 6, stepForY ((float) e.y));
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
    const int written = juce::jlimit (bottomLineStep - 8, topLineStep + 6, stepForY ((float) e.y));
    Note n { written % 7, accidental, written / 7 - 1 };   // sounding: an octave below written
    onPlace (n);
}
