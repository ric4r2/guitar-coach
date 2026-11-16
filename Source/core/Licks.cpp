#include "Licks.h"
#include "BinaryData.h"

using namespace theory;

std::vector<Lick> parseLicks (const juce::String& json)
{
    std::vector<Lick> out;
    const auto root = juce::JSON::parse (json);
    if (auto* arr = root["licks"].getArray())
        for (auto& l : *arr)
        {
            Lick lick;
            lick.id = l["id"].toString();
            lick.name = l["name"].toString();
            lick.description = l["description"].toString();
            lick.scale = l.getProperty ("scale", "minorPentatonic").toString();
            lick.feel = l.getProperty ("feel", "shuffle").toString();
            lick.level = (int) l.getProperty ("level", 1);
            lick.beats = (double) l.getProperty ("beats", 4.0);
            if (auto* notes = l["notes"].getArray())
                for (auto& n : *notes)
                {
                    // [string, fret, beat, length, "tech", bend]
                    if (auto* a = n.getArray(); a != nullptr && a->size() >= 4)
                    {
                        LickNote ln;
                        ln.string = (int) (*a)[0];
                        ln.fret = (int) (*a)[1];
                        ln.beat = (double) (*a)[2];
                        ln.length = (double) (*a)[3];
                        if (a->size() >= 5) ln.tech = (*a)[4].toString();
                        if (a->size() >= 6) ln.bend = (int) (*a)[5];
                        lick.notes.push_back (ln);
                    }
                }
            out.push_back (std::move (lick));
        }
    return out;
}

std::vector<Lick> loadLicks()
{
    return parseLicks (juce::String::fromUTF8 (BinaryData::licks_json, BinaryData::licks_jsonSize));
}

Lick transposeLick (const Lick& lick, int keyPc)
{
    int lo = 99, hi = 0;
    for (auto& n : lick.notes) { lo = juce::jmin (lo, n.fret); hi = juce::jmax (hi, n.fret); }
    const int d = mod12 (keyPc - 9);   // written in A
    int best = 0, bestCost = 1000;
    for (int shift : { d, d - 12 })
    {
        if (lo + shift < 0 || hi + shift > 17)
            continue;
        const int cost = std::abs (shift);
        if (cost < bestCost) { bestCost = cost; best = shift; }
    }
    Lick out = lick;
    for (auto& n : out.notes)
        n.fret += best;
    return out;
}

double swingBeat (double beat, bool swing)
{
    if (! swing)
        return beat;
    const double whole = std::floor (beat + 1.0e-6);
    const double frac = beat - whole;
    return std::abs (frac - 0.5) < 1.0e-3 ? whole + 2.0 / 3.0 : beat;
}

void LickFollower::reset (const Lick& lick)
{
    notes = lick.notes;
    hitTimes.clear();
    pos = 0;
    wrong = 0;
}

LickFollower::Result LickFollower::feed (int midi, double time)
{
    if (finished())
        return Result::finished;
    const auto& want = notes[(size_t) pos];
    const int target = want.targetMidi();
    if (midi == target)
    {
        hitTimes.push_back (time);
        ++pos;
        return finished() ? Result::finished : Result::hit;
    }

    // on the way up to a bend: the fretted note and anything between
    if (want.tech == "b" && midi >= want.fretMidi() && midi < target)
        return Result::ignored;
    // releasing a bend: the notes between the bent pitch and the fret
    if (want.tech == "r" && midi > target && midi <= want.fretMidi() + want.bend)
        return Result::ignored;
    if (pos > 0)
    {
        const int prev = notes[(size_t) pos - 1].targetMidi();
        // a slide passes through the frets in between
        if (want.tech == "s" && midi > juce::jmin (prev, target) && midi < juce::jmax (prev, target))
            return Result::ignored;
        // the note just played, heard again: the same pluck
        if (midi == prev && ! hitTimes.empty() && time - hitTimes.back() < 0.2)
            return Result::ignored;
        // a bent note sliding back down after its target, before the next note
        if (notes[(size_t) pos - 1].tech == "b" && midi >= notes[(size_t) pos - 1].fretMidi() && midi < prev)
            return Result::ignored;
    }
    ++wrong;
    return Result::wrong;
}

double rhythmScore (const std::vector<double>& written, const std::vector<double>& times)
{
    const size_t n = juce::jmin (written.size(), times.size());
    if (n < 3)
        return n > 0 ? 1.0 : 0.0;
    // least-squares line: time = a + b * beat
    double sw = 0, st = 0, sww = 0, swt = 0;
    for (size_t i = 0; i < n; ++i)
    {
        sw += written[i]; st += times[i];
        sww += written[i] * written[i]; swt += written[i] * times[i];
    }
    const double denom = (double) n * sww - sw * sw;
    if (std::abs (denom) < 1.0e-9)
        return 0.0;
    const double b = ((double) n * swt - sw * st) / denom;
    const double a = (st - b * sw) / (double) n;
    if (b <= 0.0)
        return 0.0;
    const double eighth = b * 0.5;
    double err = 0.0;
    for (size_t i = 0; i < n; ++i)
        err += juce::jmin (1.0, std::abs (times[i] - (a + b * written[i])) / eighth);
    return juce::jlimit (0.0, 1.0, 1.0 - err / (double) n);
}
