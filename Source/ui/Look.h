#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

// GuitarDrive's racing-green and cream look, with larger, high-contrast text for reading.
namespace Palette
{
    const juce::Colour bg          { 0xff0f2219 };
    const juce::Colour panel       { 0xff173226 };
    const juce::Colour panelLight  { 0xff22432f };
    const juce::Colour line        { 0xff2f5a42 };
    const juce::Colour text        { 0xfff3f4f6 };
    const juce::Colour dim         { 0xffa9b7ae };
    const juce::Colour cream       { 0xfff3ead0 };   // paper for staff and fretboard
    const juce::Colour ink         { 0xff1d1f23 };
    const juce::Colour gold        { 0xffd8b25a };
    const juce::Colour green       { 0xff86c25a };
    const juce::Colour red         { 0xffd9553b };
    const juce::Colour blue        { 0xff5aa7d8 };
    const juce::Colour rosewood    { 0xff3b2418 };
}

namespace Fonts
{
    inline juce::Font ui (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions ("Segoe UI", size, bold ? juce::Font::bold : juce::Font::plain));
    }
    inline juce::Font music (float size)   // has the clef, sharp, flat and natural signs
    {
        return juce::Font (juce::FontOptions ("Segoe UI Symbol", size, juce::Font::plain));
    }
}

class CoachLook : public juce::LookAndFeel_V4
{
public:
    CoachLook()
    {
        using namespace Palette;
        setColourScheme ({ bg, panel, panelLight, line, text, gold, ink, gold, text });
        setColour (juce::ResizableWindow::backgroundColourId, bg);
        setColour (juce::TextButton::buttonColourId, panelLight);
        setColour (juce::TextButton::buttonOnColourId, gold);
        setColour (juce::TextButton::textColourOffId, text);
        setColour (juce::TextButton::textColourOnId, ink);
        setColour (juce::ComboBox::backgroundColourId, panelLight);
        setColour (juce::ComboBox::outlineColourId, line);
        setColour (juce::ComboBox::textColourId, text);
        setColour (juce::PopupMenu::backgroundColourId, panel);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, gold);
        setColour (juce::PopupMenu::highlightedTextColourId, ink);
        setColour (juce::Label::textColourId, text);
        setColour (juce::Slider::thumbColourId, gold);
        setColour (juce::Slider::trackColourId, gold.withAlpha (0.6f));
        setColour (juce::Slider::backgroundColourId, panelLight);
        setColour (juce::Slider::textBoxTextColourId, text);
        setColour (juce::Slider::textBoxOutlineColourId, line);
        setColour (juce::ListBox::backgroundColourId, panel);
        setColour (juce::ScrollBar::thumbColourId, line);
        setColour (juce::ToggleButton::textColourId, text);
        setColour (juce::ToggleButton::tickColourId, gold);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int h) override { return Fonts::ui (juce::jmin (17.0f, h * 0.5f)); }
    juce::Font getLabelFont (juce::Label& l) override { return Fonts::ui (l.getFont().getHeight()); }
    juce::Font getComboBoxFont (juce::ComboBox&) override { return Fonts::ui (15.0f); }
    juce::Font getPopupMenuFont() override { return Fonts::ui (15.0f); }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour& base, bool over, bool down) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.0f);
        auto c = b.getToggleState() ? findColour (juce::TextButton::buttonOnColourId) : base;
        if (! b.isEnabled()) c = c.withAlpha (0.4f);
        if (over) c = c.brighter (0.12f);
        if (down) c = c.darker (0.15f);
        g.setColour (c);
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (Palette::line.brighter (0.2f));
        g.drawRoundedRectangle (r, 6.0f, 1.0f);
    }
};

// A heading label drawn in gold.
inline void drawHeading (juce::Graphics& g, const juce::String& text, juce::Rectangle<int> r, float size = 24.0f)
{
    g.setColour (Palette::gold);
    g.setFont (Fonts::ui (size, true));
    g.drawText (text, r, juce::Justification::centredLeft);
}

// A rounded panel background.
inline void drawPanel (juce::Graphics& g, juce::Rectangle<float> r)
{
    g.setColour (Palette::panel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (Palette::line);
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);
}

// Lesson text: "## heading", "- bullet", or a paragraph. Lays itself out to its width.
class RichText : public juce::Component
{
public:
    void setLines (const juce::StringArray& l) { lines = l; layout(); repaint(); }

    int heightForWidth (int w)
    {
        if (w != lastWidth) { lastWidth = w; build (w); }
        return totalHeight;
    }

    void resized() override { layout(); }
    void paint (juce::Graphics& g) override
    {
        for (auto& b : blocks)
            b.layout.draw (g, b.area);
    }

private:
    struct Block { juce::TextLayout layout; juce::Rectangle<float> area; };
    juce::StringArray lines;
    std::vector<Block> blocks;
    int totalHeight = 0, lastWidth = -1;

    void layout() { lastWidth = getWidth(); build (getWidth()); }

    void build (int width)
    {
        blocks.clear();
        float y = 0.0f;
        for (auto& raw : lines)
        {
            juce::AttributedString s;
            float indent = 0.0f, before = 4.0f;
            if (raw.startsWith ("## "))
            {
                s.append (raw.substring (3), Fonts::ui (20.0f, true), Palette::gold);
                before = y > 0 ? 14.0f : 0.0f;
            }
            else if (raw.startsWith ("- "))
            {
                s.append (juce::String (juce::CharPointer_UTF8 ("\xe2\x80\xa2  ")) + raw.substring (2), Fonts::ui (17.0f), Palette::text);
                indent = 14.0f;
                before = 2.0f;
            }
            else
            {
                s.append (raw, Fonts::ui (17.0f), Palette::text);
                before = 6.0f;
            }
            s.setWordWrap (juce::AttributedString::byWord);
            s.setLineSpacing (3.0f);
            Block b;
            b.layout.createLayout (s, (float) juce::jmax (50, width) - indent);
            y += before;
            b.area = { indent, y, (float) width - indent, b.layout.getHeight() };
            y += b.layout.getHeight();
            blocks.push_back (std::move (b));
        }
        totalHeight = (int) std::ceil (y) + 8;
    }
};
