#include "Fingerings.h"
#include <algorithm>
#include <map>

using namespace theory;

namespace
{
    std::vector<int> scalePcs (int rootPc, const ScaleType& t)
    {
        std::vector<int> pcs;
        for (auto& i : t.intervals)
            pcs.push_back (mod12 (rootPc + i.semitones));
        return pcs;
    }

    int degreeOf (const std::vector<int>& pcs, int pc)
    {
        for (int i = 0; i < (int) pcs.size(); ++i)
            if (pcs[(size_t) i] == mod12 (pc))
                return i;
        return -1;
    }

    // every scale note in frets lo..hi; a pitch that fits on two strings (G and B) goes on the higher string
    Fingering window (const std::vector<int>& pcs, int lo, int hi)
    {
        if (lo < 0) { lo = 0; hi = 4; }
        std::map<int, FretNote> byMidi;
        for (int s = 6; s >= 1; --s)
            for (int f = lo; f <= hi; ++f)
            {
                const int midi = fretMidi (s, f);
                const int d = degreeOf (pcs, midi);
                if (d < 0)
                    continue;
                auto it = byMidi.find (midi);
                if (it == byMidi.end() || s < it->second.string)
                    byMidi[midi] = { s, f, midi, d };
            }
        Fingering fg;
        fg.lowFret = 99;
        for (auto& [m, n] : byMidi)
        {
            fg.notes.push_back (n);
            fg.lowFret = std::min (fg.lowFret, n.fret);
            fg.highFret = std::max (fg.highFret, n.fret);
        }
        return fg;
    }

    juce::String fretRange (const Fingering& f) { return " (frets " + juce::String (f.lowFret) + "-" + juce::String (f.highFret) + ")"; }
}

std::vector<Fingering> positionFingerings (int rootPc, const ScaleType& t)
{
    const auto pcs = scalePcs (rootPc, t);
    std::vector<Fingering> out;

    if (t.size() == 7)
    {
        // the windows of the relative major's CAGED shapes, filled with this scale's notes
        const int majorTonic = mod12 (rootPc + t.parent);
        struct Shape { int offset; const char* name; };
        for (auto sh : { Shape { 0, "E shape" }, Shape { 2, "D shape" }, Shape { 4, "C shape" }, Shape { 7, "A shape" }, Shape { 9, "G shape" } })
        {
            const int f = mod12 (majorTonic + sh.offset - 4);   // that degree on the 6th string
            auto fg = window (pcs, f - 1, f + 3);
            fg.name = juce::String (sh.name) + fretRange (fg);
            out.push_back (fg);
        }
        std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.lowFret < b.lowFret; });
        return out;
    }

    // pentatonic and blues: a box starting on each pentatonic degree on the 6th string
    int box = 1;
    for (int d = 0; d < t.size(); ++d)
    {
        const auto& iv = t.intervals[(size_t) d];
        if (iv.quality == "d" || iv.quality == "A")   // the blue note does not start a box
            continue;
        const int f = mod12 (pcs[(size_t) d] - 4);
        auto fg = window (pcs, f - 1, f + 3);
        fg.name = "Box " + juce::String (box++) + fretRange (fg);
        out.push_back (fg);
    }
    return out;
}

std::vector<Fingering> threeNpsFingerings (int rootPc, const ScaleType& t)
{
    std::vector<Fingering> out;
    if (t.size() != 7)
        return out;
    const auto pcs = scalePcs (rootPc, t);
    for (int d = 0; d < 7; ++d)
      for (int octave = 0; octave < 2; ++octave)
    {
        // near the nut a pattern can run below the open string; then it is played an octave up
        Fingering fg;
        fg.lowFret = 99;
        int midi = fretMidi (6, mod12 (pcs[(size_t) d] - 4) + 12 * octave);
        for (int i = 0; i < 18; ++i)
        {
            while (degreeOf (pcs, midi) < 0)
                ++midi;
            const int s = 6 - i / 3;
            const FretNote n { s, midi - openMidi[s], midi, degreeOf (pcs, midi) };
            fg.notes.push_back (n);
            fg.lowFret = std::min (fg.lowFret, n.fret);
            fg.highFret = std::max (fg.highFret, n.fret);
            ++midi;
        }
        if (fg.lowFret < 0)
            continue;
        fg.name = "3 per string from " + degreeLabel (t.intervals[(size_t) d]) + fretRange (fg);
        out.push_back (fg);
        break;
    }
    std::sort (out.begin(), out.end(), [] (auto& a, auto& b) { return a.lowFret < b.lowFret; });
    return out;
}

std::array<juce::String, 12> scaleSpelling (const Note& root, const ScaleType& t)
{
    std::array<juce::String, 12> names;
    for (auto& n : scaleNotes (root, t, false))
        names[(size_t) n.pc()] = n.name();
    return names;
}

const juce::StringArray& runPatternNames()
{
    static const juce::StringArray names { "Up", "Up and down", "In 3s, up and down", "In 4s, up and down", "In 3rds, up and down" };
    return names;
}

std::vector<FretNote> scaleRun (const Fingering& fg, RunPattern p)
{
    const auto& n = fg.notes;
    const int count = (int) n.size();
    std::vector<FretNote> out;
    auto groups = [&] (int size)
    {
        // ascending groups, then descending groups from the note below the top (so no note is played twice in a row)
        for (int i = 0; i + size - 1 < count; ++i)
            for (int k = 0; k < size; ++k)
                out.push_back (n[(size_t) (i + k)]);
        for (int i = count - 2; i - size + 1 >= 0; --i)
            for (int k = 0; k < size; ++k)
                out.push_back (n[(size_t) (i - k)]);
    };

    switch (p)
    {
        case RunPattern::up:
            out = n;
            break;
        case RunPattern::upDown:
            out = n;
            for (int i = count - 2; i >= 0; --i)
                out.push_back (n[(size_t) i]);
            break;
        case RunPattern::threes: groups (3); break;
        case RunPattern::fours:  groups (4); break;
        case RunPattern::thirds:
            // pairs a 3rd apart: 1-3, 2-4, ... then down
            for (int i = 0; i + 2 < count; ++i) { out.push_back (n[(size_t) i]); out.push_back (n[(size_t) i + 2]); }
            for (int i = count - 2; i - 2 >= 0; --i) { out.push_back (n[(size_t) i]); out.push_back (n[(size_t) i - 2]); }
            break;
    }
    return out;
}

const juce::StringArray& techDrillNames()
{
    static const juce::StringArray names { "Chromatic 1-2-3-4", "Spider 1-3-2-4", "String skipping 1-2-3-4" };
    return names;
}

std::vector<FretNote> techniqueDrill (TechDrill d, int p)
{
    p = juce::jlimit (0, 15, p);
    const std::vector<int> strings = d == TechDrill::stringSkip ? std::vector<int> { 6, 4, 5, 3, 4, 2, 3, 1 }
                                                                : std::vector<int> { 6, 5, 4, 3, 2, 1 };
    const std::vector<int> fingers = d == TechDrill::spider ? std::vector<int> { 1, 3, 2, 4 } : std::vector<int> { 1, 2, 3, 4 };

    std::vector<FretNote> out;
    auto add = [&] (int s, int finger) { const int f = p + finger - 1; out.push_back ({ s, f, fretMidi (s, f), finger }); };
    for (int s : strings)
        for (int finger : fingers)
            add (s, finger);
    // and back: strings in reverse, fingers in reverse, without playing the top note twice
    const auto top = out.size();
    for (auto it = strings.rbegin(); it != strings.rend(); ++it)
        for (auto f = fingers.rbegin(); f != fingers.rend(); ++f)
            add (*it, *f);
    out.erase (out.begin() + (long) top);
    return out;
}

RunScore scoreRun (const std::vector<double>& times, int mistakes, double bpm, int notesPerBeat)
{
    RunScore r;
    r.notes = (int) times.size();
    r.mistakes = mistakes;
    r.accuracy = r.notes + mistakes > 0 ? (double) r.notes / (r.notes + mistakes) : 0.0;
    if (r.notes < 3)
        return r;

    std::vector<double> gaps;
    for (size_t i = 1; i < times.size(); ++i)
        gaps.push_back (times[i] - times[i - 1]);
    auto sorted = gaps;
    std::sort (sorted.begin(), sorted.end());
    const double median = sorted[sorted.size() / 2];
    const int npb = juce::jmax (1, notesPerBeat);
    r.averageBpm = median > 0 ? 60.0 / (median * npb) : 0.0;

    const double expected = bpm > 0 ? 60.0 / (bpm * npb) : median;
    double err = 0.0;
    for (auto g : gaps)
        err += std::min (1.0, std::abs (g - expected) / expected);
    r.evenness = juce::jlimit (0.0, 1.0, 1.0 - err / (double) gaps.size());

    const bool onTempo = bpm <= 0 || std::abs (r.averageBpm - bpm) <= bpm * 0.1;
    r.clean = r.accuracy >= 0.95 && r.evenness >= 0.75 && onTempo;
    return r;
}
