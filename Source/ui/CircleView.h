#pragma once
#include "Look.h"
#include "core/Theory.h"
#include <functional>

// The circle of fifths: major keys outside, relative minors inside, key signatures on the rim.
// Click a key to select it (and hear it, through onSelect).
class CircleView : public juce::Component
{
public:
    std::function<void (int fifths, bool minor)> onSelect;
    void setSelected (int fifths) { selected = fifths; repaint(); }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        const float size = juce::jmin (r.getWidth(), r.getHeight());
        const auto c = r.getCentre();
        const float outer = size * 0.5f - 2.0f, mid = outer * 0.70f, inner = outer * 0.42f;

        g.setColour (Palette::panelLight);
        g.fillEllipse (c.x - outer, c.y - outer, outer * 2, outer * 2);
        g.setColour (Palette::panel);
        g.fillEllipse (c.x - mid, c.y - mid, mid * 2, mid * 2);
        g.setColour (Palette::bg);
        g.fillEllipse (c.x - inner, c.y - inner, inner * 2, inner * 2);

        for (int i = 0; i < 12; ++i)
        {
            const int fifths = i <= 6 ? i : i - 12;   // C at the top, sharps clockwise, flats counter-clockwise
            const float a = juce::MathConstants<float>::twoPi * (float) i / 12.0f;
            auto at = [&] (float radius) { return juce::Point<float> (c.x + std::sin (a) * radius, c.y - std::cos (a) * radius); };

            if (fifths == selected)
            {
                juce::Path wedge;
                wedge.addPieSegment (c.x - outer, c.y - outer, outer * 2, outer * 2, a - 0.26f, a + 0.26f, inner / outer);
                g.setColour (Palette::gold.withAlpha (0.35f));
                g.fillPath (wedge);
            }

            // spokes between keys
            const float b = a + juce::MathConstants<float>::twoPi / 24.0f;
            g.setColour (Palette::line);
            g.drawLine (c.x + std::sin (b) * inner, c.y - std::cos (b) * inner, c.x + std::sin (b) * outer, c.y - std::cos (b) * outer, 1.0f);

            auto major = theory::keyName (fifths, false).upToFirstOccurrenceOf (" ", false, false);
            if (i == 6)
                major = theory::keyName (6, false).upToFirstOccurrenceOf (" ", false, false) + "/" + theory::keyName (-6, false).upToFirstOccurrenceOf (" ", false, false);
            const auto minor = theory::keyName (fifths, true).upToFirstOccurrenceOf (" ", false, false) + "m";

            g.setColour (Palette::text);
            g.setFont (Fonts::ui (juce::jmax (12.0f, size * 0.075f), true));
            const auto pm = at ((outer + mid) * 0.5f + size * 0.02f);
            g.drawText (major, juce::Rectangle<float> (pm.x - size * 0.13f, pm.y - size * 0.06f, size * 0.26f, size * 0.07f), juce::Justification::centred);
            g.setColour (Palette::dim);
            g.setFont (Fonts::ui (juce::jmax (10.0f, size * 0.045f)));
            g.drawText (i == 6 ? theory::fifthsLabel (6) + "/" + theory::fifthsLabel (-6) : theory::fifthsLabel (fifths),
                        juce::Rectangle<float> (pm.x - size * 0.1f, pm.y + size * 0.012f, size * 0.2f, size * 0.05f), juce::Justification::centred);

            g.setColour (Palette::gold);
            g.setFont (Fonts::ui (juce::jmax (10.0f, size * 0.05f), true));
            const auto pn = at ((mid + inner) * 0.5f);
            g.drawText (minor, juce::Rectangle<float> (pn.x - size * 0.09f, pn.y - size * 0.03f, size * 0.18f, size * 0.06f), juce::Justification::centred);
        }

        g.setColour (Palette::dim);
        g.setFont (Fonts::ui (juce::jmax (10.0f, size * 0.042f)));
        g.drawFittedText (juce::String::fromUTF8 ("\xe2\x86\xbb 5th up\n\xe2\x86\xba 4th up"), juce::Rectangle<float> (c.x - inner, c.y - inner * 0.5f, inner * 2, inner).toNearestInt(),
                          juce::Justification::centred, 2);
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        const auto c = getLocalBounds().toFloat().getCentre();
        const float size = juce::jmin ((float) getWidth(), (float) getHeight());
        const float outer = size * 0.5f - 2.0f, mid = outer * 0.70f, inner = outer * 0.42f;
        const auto d = e.position - c;
        const float dist = d.getDistanceFromOrigin();
        if (dist < inner || dist > outer)
            return;
        float a = std::atan2 (d.x, -d.y);
        if (a < 0) a += juce::MathConstants<float>::twoPi;
        const int i = juce::roundToInt (a / juce::MathConstants<float>::twoPi * 12.0f) % 12;
        const int fifths = i <= 6 ? i : i - 12;
        setSelected (fifths);
        if (onSelect)
            onSelect (fifths, dist < mid);
    }

private:
    int selected = 100;
};
