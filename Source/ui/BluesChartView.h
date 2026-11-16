#pragma once
#include "Look.h"
#include "core/BackingTrack.h"

// A 12-bar chart: chord names and numbers, three rows of four bars, with the bar being played lit up.
class BluesChartView : public juce::Component
{
public:
    void setForm (int index) { form = juce::jlimit (0, (int) bluesForms().size() - 1, index); repaint(); }
    void setKey (int pc) { key = theory::mod12 (pc); repaint(); }
    void setCurrentBar (int b) { if (b != current) { current = b; repaint(); } }

    void paint (juce::Graphics& g) override
    {
        const auto& f = bluesForms()[(size_t) form];
        auto r = getLocalBounds().toFloat();
        const float gap = 6.0f;
        const float cellW = (r.getWidth() - gap * 3) / 4.0f, cellH = (r.getHeight() - gap * 2) / 3.0f;
        for (int bar = 0; bar < 12; ++bar)
        {
            auto cell = juce::Rectangle<float> (r.getX() + (float) (bar % 4) * (cellW + gap), r.getY() + (float) (bar / 4) * (cellH + gap), cellW, cellH);
            const auto& chords = f.bars[(size_t) bar];
            const bool lit = bar == current;
            g.setColour (lit ? Palette::gold.withAlpha (0.85f) : Palette::panelLight);
            g.fillRoundedRectangle (cell, 6.0f);
            g.setColour (lit ? Palette::ink : Palette::dim);
            g.setFont (Fonts::ui (juce::jmin (13.0f, cellH * 0.2f)));
            g.drawText (juce::String (bar + 1), cell.reduced (7, 4), juce::Justification::topLeft);

            const int n = (int) chords.size();
            for (int i = 0; i < n; ++i)
            {
                auto part = cell.withWidth (cell.getWidth() / (float) n).translated (cell.getWidth() / (float) n * (float) i, 0);
                g.setColour (lit ? Palette::ink : Palette::text);
                g.setFont (Fonts::ui (juce::jmin (26.0f, cellH * 0.34f) * (n > 1 ? 0.8f : 1.0f), true));
                g.drawText (bluesChordName (key, chords[(size_t) i]), part.translated (0, -cellH * 0.06f), juce::Justification::centred);
                g.setColour (lit ? Palette::ink.withAlpha (0.8f) : Palette::gold);
                g.setFont (Fonts::ui (juce::jmin (15.0f, cellH * 0.2f), true));
                g.drawText (bluesRoman (chords[(size_t) i], f.minor), part.reduced (6, 4), juce::Justification::centredBottom);
            }
        }
    }

private:
    int form = 0, key = 9, current = -1;
};
