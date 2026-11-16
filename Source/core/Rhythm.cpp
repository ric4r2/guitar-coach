#include "Rhythm.h"

const std::vector<RhythmCell>& threePulseCells()
{
    static const std::vector<RhythmCell> cells {
        { "triplet",    "Triplet",    "1 trip let", 3, { 0, 1, 2 } },
        { "swing",      "Swing",      "1 - let",    3, { 0, 2 } },
        { "afroCuban",  "Afro-Cuban", "1 trip -",   3, { 0, 1 } },
        { "waltz",      "Waltz",      "- trip let", 3, { 1, 2 } } };
    return cells;
}

const std::vector<RhythmCell>& fourPulseCells()
{
    static const std::vector<RhythmCell> cells {
        { "allFour",  "All four (sixteenths)",     "1 e & a", 4, { 0, 1, 2, 3 } },
        { "eighths",  "Two eighths",               "1 - & -", 4, { 0, 2 } },
        { "oneAndA",  "Eighth and two sixteenths", "1 - & a", 4, { 0, 2, 3 } },
        { "oneEAnd",  "Two sixteenths and eighth", "1 e & -", 4, { 0, 1, 2 } },
        { "oneA",     "Dotted eighth, sixteenth",  "1 - - a", 4, { 0, 3 } } };
    return cells;
}

std::vector<RhythmEvent> cellEvents (const RhythmCell& c, double start)
{
    std::vector<RhythmEvent> out;
    const double pulse = 1.0 / c.pulses;
    for (size_t i = 0; i < c.hits.size(); ++i)
    {
        const int next = i + 1 < c.hits.size() ? c.hits[i + 1] : c.pulses;
        out.push_back ({ start + c.hits[i] * pulse, (next - c.hits[i]) * pulse });
    }
    return out;
}

std::vector<RhythmEvent> randomRhythm (int level, int beats, juce::Random& rng)
{
    for (;;)
    {
        std::vector<RhythmEvent> out;
        double b = 0.0;
        while (b < beats - 1.0e-6)
        {
            const bool first = b < 1.0e-6;
            const bool twoLeft = beats - b >= 2.0 - 1.0e-6;
            const int choices = level <= 1 ? 4 : level == 2 ? 7 : 12;
            const int pick = rng.nextInt (choices);
            auto add = [&] (double at, double len) { out.push_back ({ b + at, len }); };
            switch (pick)
            {
                case 0: add (0, 1); b += 1; break;                                            // quarter
                case 1: add (0, 0.5); add (0.5, 0.5); b += 1; break;                          // two eighths
                case 2: if (first) continue; b += 1; break;                                   // quarter rest
                case 3: if (! twoLeft) continue; add (0, 2); b += 2; break;                   // half
                case 4: if (! twoLeft) continue; add (0, 1.5); add (1.5, 0.5); b += 2; break; // dotted quarter, eighth
                case 5: if (! twoLeft) continue; add (0, 0.5); add (0.5, 1); add (1.5, 0.5); b += 2; break;   // syncopation
                case 6: if (first) continue; add (0.5, 0.5); b += 1; break;                   // eighth rest, eighth
                case 7: for (auto& e : cellEvents (fourPulseCells()[0], 0)) add (e.beat, e.length); b += 1; break;
                case 8: for (auto& e : cellEvents (fourPulseCells()[2], 0)) add (e.beat, e.length); b += 1; break;
                case 9: for (auto& e : cellEvents (fourPulseCells()[3], 0)) add (e.beat, e.length); b += 1; break;
                case 10: for (auto& e : cellEvents (fourPulseCells()[4], 0)) add (e.beat, e.length); b += 1; break;
                default: for (auto& e : cellEvents (threePulseCells()[0], 0)) add (e.beat, e.length); b += 1; break;
            }
        }
        // something to read: enough notes, and not just quarters
        bool varied = false;
        for (auto& e : out) varied = varied || std::abs (e.length - 1.0) > 1.0e-6;
        if (out.size() >= 4 && varied)
            return out;
    }
}

const std::vector<NoteValue>& noteValues()
{
    static const std::vector<NoteValue> v {
        { "whole note", 4.0 }, { "dotted half note", 3.0 }, { "half note", 2.0 }, { "dotted quarter note", 1.5 },
        { "quarter note", 1.0 }, { "dotted eighth note", 0.75 }, { "eighth note", 0.5 }, { "sixteenth note", 0.25 } };
    return v;
}

TapResult scoreTaps (const std::vector<RhythmEvent>& written, const std::vector<double>& taps, double bpm)
{
    TapResult r;
    const size_t n = juce::jmin (written.size(), taps.size());
    r.missing = (int) written.size() - (int) n;
    r.extra = (int) taps.size() - (int) n;
    if (n == 0 || bpm <= 0)
        return r;
    const double spb = 60.0 / bpm;
    double offset = 0.0;
    for (size_t i = 0; i < n; ++i)
        offset += taps[i] - written[i].beat * spb;
    offset /= (double) n;
    double err = 0.0;
    for (size_t i = 0; i < n; ++i)
    {
        const double dev = (taps[i] - offset - written[i].beat * spb) / spb;   // in beats
        r.errorsInBeats.push_back (dev);
        err += juce::jmin (1.0, std::abs (dev) / 0.5);
    }
    err += (double) (r.missing + r.extra);
    r.score = juce::jlimit (0.0, 1.0, 1.0 - err / (double) juce::jmax (written.size(), taps.size()));
    return r;
}
