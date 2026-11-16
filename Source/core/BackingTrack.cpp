#include "BackingTrack.h"
#include "Theory.h"

namespace
{
    constexpr double twoPi = 6.283185307179586;

    BluesChord ch (int offset, const char* quality) { return { offset, quality }; }
    std::vector<BluesChord> one (int offset, const char* quality = "7") { return { ch (offset, quality) }; }
    std::vector<BluesChord> two (BluesChord a, BluesChord b) { return { a, b }; }

    double hzFor (int midi) { return 440.0 * std::pow (2.0, (midi - 69) / 12.0); }
    float decayFor (double seconds, double sr) { return (float) std::exp (-1.0 / (seconds * sr)); }
}

// ---------------------------------------------------------------- forms

const std::vector<BluesForm>& bluesForms()
{
    static const std::vector<BluesForm> forms {
        { "standard", "12-bar blues", false,
          { one (0), one (0), one (0), one (0), one (5), one (5), one (0), one (0), one (7), one (5), one (0), one (7) } },
        { "quickChange", "Quick change (IV in bar 2)", false,
          { one (0), one (5), one (0), one (0), one (5), one (5), one (0), one (0), one (7), one (5), one (0), one (7) } },
        { "minor", "Minor blues", true,
          { one (0, "m7"), one (0, "m7"), one (0, "m7"), one (0, "m7"), one (5, "m7"), one (5, "m7"), one (0, "m7"), one (0, "m7"),
            one (8), one (7), one (0, "m7"), one (7) } },
        { "jazz", "Jazz blues", false,
          { one (0), one (5), one (0), two (ch (7, "m7"), ch (0, "7")), one (5), one (6, "dim7"), one (0), one (9),
            one (2, "m7"), one (7), two (ch (0, "7"), ch (9, "7")), two (ch (2, "m7"), ch (7, "7")) } } };
    return forms;
}

const BluesForm& bluesForm (const juce::String& id)
{
    for (auto& f : bluesForms())
        if (f.id == id)
            return f;
    return bluesForms().front();
}

std::vector<int> bluesChordTones (const BluesChord& c)
{
    if (c.quality == "m7")   return { 0, 3, 7, 10 };
    if (c.quality == "dim7") return { 0, 3, 6, 9 };
    return { 0, 4, 7, 10 };
}

theory::Note bluesChordRoot (int keyPc, const BluesChord& c)
{
    const int pc = theory::mod12 (keyPc + c.offset);
    const bool flatKey = keyPc == 1 || keyPc == 3 || keyPc == 5 || keyPc == 8 || keyPc == 10;
    const bool flats = c.offset != 6 && (flatKey || c.offset == 3 || c.offset == 8 || c.offset == 10);
    return theory::noteFromMidi (48 + pc, flats);
}

juce::String bluesChordName (int keyPc, const BluesChord& c)
{
    return bluesChordRoot (keyPc, c).name() + c.quality;
}

juce::String bluesRoman (const BluesChord& c, bool)
{
    static const char* numerals[12] = { "I", "bII", "II", "bIII", "III", "IV", "#IV", "V", "bVI", "VI", "bVII", "VII" };
    return juce::String (numerals[theory::mod12 (c.offset)]) + c.quality;
}

const juce::StringArray& feelNames()
{
    static const juce::StringArray names { "Shuffle", "Straight (rock)", "Slow blues (12/8)", "Funk" };
    return names;
}

// ---------------------------------------------------------------- voices

void BackingTrack::Bass::start (int midi, int samples, float velocity)
{
    inc = twoPi * hzFor (midi) / sr;
    remaining = samples;
    target = velocity;
    env = 1.0f;
}

float BackingTrack::Bass::next()
{
    // quick attack, a little decay, then a short release when the note ends
    if (remaining > 0)
    {
        --remaining;
        level += 0.02f * (target - level);
        env = 0.6f + (env - 0.6f) * 0.99993f;   // from the pluck down to a steady 60%
    }
    else
        level *= 0.9985f;
    if (level < 1.0e-5f)
        return 0.0f;
    phase += inc;
    if (phase > twoPi) phase -= twoPi;
    const double x = std::sin (phase) + 0.45 * std::sin (2 * phase) + 0.2 * std::sin (3 * phase);
    return (float) std::tanh (1.3 * x) * level * env;
}

void BackingTrack::Organ::start (int note, int samples)
{
    midi = note;
    inc = twoPi * hzFor (note) / sr;
    remaining = samples;
    target = 1.0f;
    on = true;
}

float BackingTrack::Organ::next()
{
    if (! on)
        return 0.0f;
    if (remaining > 0 && --remaining == 0)
        target = 0.0f;
    env += (target > env ? 0.0025f : 0.0004f) * (target - env) * 4.0f;
    if (target == 0.0f && env < 1.0e-4f)
    {
        on = false;
        midi = -1;
        return 0.0f;
    }
    static const double amps[4] = { 1.0, 0.6, 0.35, 0.2 };
    double x = 0.0;
    for (int k = 0; k < 4; ++k)
    {
        phase[k] += inc * (k + 1);
        if (phase[k] > twoPi) phase[k] -= twoPi;
        x += amps[k] * std::sin (phase[k]);
    }
    lfo += twoPi * 5.8 / sr;
    if (lfo > twoPi) lfo -= twoPi;
    return (float) (x * (1.0 + 0.15 * std::sin (lfo))) * env;
}

float BackingTrack::Drum::next (uint32_t& s)
{
    if (env < 1.0e-4f)
        return 0.0f;
    s = s * 1664525u + 1013904223u;
    const float noise = (float) ((s >> 9) & 0x7fff) / 16384.0f - 1.0f;
    ++age;
    float out = 0.0f;
    switch (kind)
    {
        case kick:
        {
            const double f = 45.0 + 80.0 * std::exp (-age / (0.03 * sr));
            phase += twoPi * f / sr;
            env *= decayFor (0.28, sr);
            out = (float) std::sin (phase) * env * 1.1f + (age < 40 ? noise * 0.15f : 0.0f);
            break;
        }
        case snare:
        {
            phase += twoPi * 185.0 / sr;
            tone *= decayFor (0.07, sr);
            env *= decayFor (0.15, sr);
            const float hp = noise - lastNoise;
            lastNoise = noise;
            out = (float) std::sin (phase) * tone * 0.6f + hp * env * 0.45f;
            break;
        }
        default:
        {
            // hats and cymbal: noise through two first-difference filters (bright)
            const float d1 = noise - hp1; hp1 = noise;
            const float d2 = d1 - hp2;    hp2 = d1;
            env *= decayFor (kind == hat ? 0.035 : kind == openHat ? 0.25 : 1.3, sr);
            out = d2 * env * (kind == crash ? 0.25f : 0.3f);
            break;
        }
    }
    return out * velocity;
}

// ---------------------------------------------------------------- track

void BackingTrack::prepare (double sampleRate)
{
    sr = sampleRate;
    bass.sr = sr;
    for (auto& g : guitars) g.prepare (sr);
    for (auto& o : organs) { o.sr = sr; o.on = false; o.env = 0; }
    const Drum::Kind kinds[5] = { Drum::kick, Drum::snare, Drum::hat, Drum::openHat, Drum::crash };
    for (int i = 0; i < 5; ++i) { drums[(size_t) i].kind = kinds[i]; drums[(size_t) i].sr = sr; drums[(size_t) i].env = 0; }
    playing.store (false);
}

void BackingTrack::setSettings (const BackingSettings& s)
{
    const juce::SpinLock::ScopedLockType l (lock);
    pendingSettings = s;
    settingsChanged.store (true);
}

BackingSettings BackingTrack::getSettings() const
{
    const juce::SpinLock::ScopedLockType l (lock);
    return pendingSettings;
}

void BackingTrack::start() { startRequested.store (true); }
void BackingTrack::stop() { stopRequested.store (true); }

int BackingTrack::samplesPerTick() const { return juce::jmax (1, (int) (sr * 60.0 / (settings.bpm * ticksPerBeat))); }

const BluesChord& BackingTrack::chordAt (int barIndex, int beatInBar) const
{
    const auto& form = bluesForms()[(size_t) juce::jlimit (0, (int) bluesForms().size() - 1, settings.form)];
    const auto& b = form.bars[(size_t) juce::jlimit (0, 11, barIndex)];
    return b.size() > 1 && beatInBar >= 2 ? b[1] : b[0];
}

int BackingTrack::runIndex (int barIndex) const
{
    const auto& form = bluesForms()[(size_t) juce::jlimit (0, (int) bluesForms().size() - 1, settings.form)];
    int k = 0;
    for (int b = barIndex - 1; b >= 0; --b)
    {
        const auto& prev = form.bars[(size_t) b];
        const auto& cur = form.bars[(size_t) barIndex];
        if (prev.size() != 1 || cur.size() != 1 || prev[0].offset != cur[0].offset || prev[0].quality != cur[0].quality)
            break;
        ++k;
    }
    return k;
}

void BackingTrack::playGuitar (int midi, int lengthTicks, float velocity)
{
    guitars[(size_t) nextGuitar].start (midi, velocity, lengthTicks * samplesPerTick());
    nextGuitar = (nextGuitar + 1) % (int) guitars.size();
}

void BackingTrack::playOrgan (const std::vector<int>& notes, int lengthTicks)
{
    const int samples = lengthTicks * samplesPerTick();
    // keep notes that carry on, release the others, start the new ones
    for (auto& o : organs)
        if (o.on && std::find (notes.begin(), notes.end(), o.midi) == notes.end())
            o.release();
    for (int n : notes)
    {
        bool held = false;
        for (auto& o : organs)
            if (o.on && o.midi == n && o.target > 0) { o.remaining = samples; held = true; }
        if (held)
            continue;
        for (auto& o : organs)
            if (! o.on || o.target == 0.0f)
            {
                o.start (n, samples);
                break;
            }
    }
}

void BackingTrack::onTick (long long tick)
{
    const int barTicks = ticksPerBeat * beatsPerBar;
    long long formTick = tick - (settings.countIn ? barTicks : 0);

    if (formTick < 0)
    {
        // count-in: four clicks on the hi-hat
        if (tick % ticksPerBeat == 0)
            drums[2].trigger (tick == 0 ? 0.9f : 0.7f);
        bar.store (-1);
        beat.store ((int) (tick / ticksPerBeat));
        return;
    }

    const int barAbs = (int) (formTick / barTicks);
    const int local = (int) (formTick % barTicks);
    const int barI = barAbs % 12;
    const int b = local / ticksPerBeat, sub = local % ticksPerBeat;

    if (local == 0)
    {
        if (settings.stopAfterBars > 0 && barAbs >= settings.stopAfterBars)
        {
            stopRequested.store (true);
            return;
        }
        bar.store (barI);
        chorus.store (barAbs / 12);
    }
    if (sub == 0)
        beat.store (b);

    const auto feel = (Feel) settings.feel;
    const auto& chord = chordAt (barI, b);
    const auto tones = bluesChordTones (chord);
    const bool twoChords = bluesForms()[(size_t) juce::jlimit (0, (int) bluesForms().size() - 1, settings.form)].bars[(size_t) barI].size() > 1;
    const int key = settings.keyPc;

    // ---- drums
    if (settings.drums)
    {
        auto hit = [&] (int d, float v) { drums[(size_t) d].trigger (v); if (d == 1) drums[1].tone = 1.0f; };
        const bool fillBeat = barI == 11 && b == 3;
        if (barAbs > 0 && barI == 0 && local == 0)
            hit (4, 0.8f);
        switch (feel)
        {
            case Feel::shuffle:
                if (sub == 0) hit (2, b % 2 == 0 ? 0.55f : 0.45f);
                if (sub == 8) hit (2, 0.3f);
                if (sub == 0 && (b == 0 || b == 2)) hit (0, 0.9f);
                if (sub == 8 && b == 1) hit (0, 0.4f);
                if (fillBeat) { if (sub % 4 == 0) hit (1, 0.4f + 0.12f * (float) (sub / 4)); }
                else if (sub == 0 && (b == 1 || b == 3)) hit (1, 0.75f);
                break;
            case Feel::straight:
                if (sub == 0) hit (2, 0.5f);
                if (sub == 6) hit (2, 0.3f);
                if ((sub == 0 && (b == 0 || b == 2)) || (sub == 6 && b == 2)) hit (0, 0.85f);
                if (fillBeat) { if (sub % 3 == 0) hit (1, 0.4f + 0.1f * (float) (sub / 3)); }
                else if (sub == 0 && (b == 1 || b == 3)) hit (1, 0.8f);
                break;
            case Feel::slow:
                if (sub == 0) hit (3, 0.35f);
                if (sub == 4 || sub == 8) hit (2, 0.3f);
                if ((sub == 0 && (b == 0 || b == 2)) || (sub == 8 && b == 1)) hit (0, sub == 0 ? 0.85f : 0.45f);
                if (fillBeat) { if (sub % 4 == 0) hit (1, 0.45f + 0.12f * (float) (sub / 4)); }
                else if (sub == 0 && (b == 1 || b == 3)) hit (1, 0.8f);
                break;
            case Feel::funk:
                if (sub % 3 == 0) hit (2, sub % 6 == 0 ? 0.45f : 0.25f);
                if ((sub == 0 && b == 0) || (sub == 9 && b == 0) || (sub == 6 && b == 2) || (sub == 0 && b == 2)) hit (0, 0.8f);
                if (sub == 0 && (b == 1 || b == 3)) hit (1, 0.85f);
                if ((sub == 9 && b == 1) || (sub == 3 && b == 3)) hit (1, 0.15f);
                if (fillBeat && sub == 6) hit (3, 0.4f);
                break;
        }
    }

    // ---- bass, below the low E string: E1 to D#2
    if (settings.bass)
    {
        const int r = 28 + theory::mod12 (key + chord.offset - 4);
        const int third = tones[1], fifth = tones[2], seventh = tones[3];
        const int sixth = 9;
        auto lineTone = [&] (int beatInBar)
        {
            if (twoChords)
                return beatInBar % 2 == 0 ? 0 : fifth;
            const int up[4] = { 0, third, fifth, sixth }, down[4] = { seventh, sixth, fifth, third };
            return runIndex (barI) % 2 == 0 ? up[beatInBar] : down[beatInBar];
        };
        const int beatSamples = samplesPerTick() * ticksPerBeat;
        switch (feel)
        {
            case Feel::shuffle:
            case Feel::slow:
                if (sub == 0) bass.start (r + lineTone (b), beatSamples * 9 / 10, 0.9f);
                break;
            case Feel::straight:
                if (sub == 0 || sub == 6) bass.start (r + lineTone (b), beatSamples * 4 / 10, sub == 0 ? 0.9f : 0.75f);
                break;
            case Feel::funk:
            {
                struct Step { int tick, tone, len; };
                const Step riff[] = { { 0, 0, 4 }, { 9, 0, 2 }, { 12, 12, 3 }, { 21, seventh, 2 }, { 24, 0, 4 }, { 33, fifth, 2 }, { 36, seventh, 3 }, { 42, 12, 3 } };
                for (auto& st : riff)
                    if (st.tick == local)
                        bass.start (r + st.tone, st.len * samplesPerTick(), 0.9f);
                break;
            }
        }
    }

    // ---- rhythm guitar: the boogie shuffle on the low strings, triplet arpeggios, or funk stabs
    if (settings.guitar)
    {
        const int r = 40 + theory::mod12 (key + chord.offset - 4);
        switch (feel)
        {
            case Feel::shuffle:
            case Feel::straight:
            {
                const int off = feel == Feel::shuffle ? 8 : 6;
                if (sub != 0 && sub != off)
                    break;
                const int e = b * 2 + (sub == 0 ? 0 : 1);
                static const int firstBar[8] = { 7, 7, 9, 9, 7, 7, 9, 9 }, secondBar[8] = { 7, 7, 9, 9, 10, 10, 9, 9 };
                int upper = (! twoChords && runIndex (barI) % 2 == 1) ? secondBar[e] : firstBar[e % 4];
                if (chord.quality == "dim7") upper = e % 4 < 2 ? 3 : 6;
                const float v = sub == 0 ? 0.42f : 0.32f;
                playGuitar (r, 4, v);
                playGuitar (r + upper, 4, v * 0.9f);
                break;
            }
            case Feel::slow:
            {
                if (sub % 4 != 0)
                    break;
                const int arp[2][3] = { { 0, 7, 12 }, { 0, 9, 12 } };
                playGuitar (r + 12 + arp[b % 2][sub / 4] - (sub == 0 ? 12 : 0), 5, 0.28f);
                break;
            }
            case Feel::funk:
                if (local == 6 || local == 15 || local == 27 || local == 30 || local == 42)
                    for (int t : { tones[1], tones[3], 14 })
                        playGuitar (r + 12 + t, 2, 0.3f);
                break;
        }
    }

    // ---- organ: held chords (3rd, 7th, 9th), or funk stabs
    if (settings.keys)
    {
        const int r = 48 + theory::mod12 (key + chord.offset);
        std::vector<int> voicing = chord.quality == "dim7" ? std::vector<int> { r + 3, r + 6, r + 9, r + 12 }
                                                            : std::vector<int> { r + tones[1], r + tones[3], r + 14, r + 19 };
        for (auto& v : voicing) if (v > 72) v -= 12;
        if (feel == Feel::funk)
        {
            if (local == 18 || local == 42)
                playOrgan (voicing, 3);
        }
        else if (local == 0 || (twoChords && local == 2 * ticksPerBeat))
            playOrgan (voicing, twoChords ? 2 * ticksPerBeat : barTicks);
    }
}

void BackingTrack::render (float* out, int n)
{
    if (settingsChanged.load())
    {
        const juce::SpinLock::ScopedTryLockType l (lock);
        if (l.isLocked())
        {
            settings = pendingSettings;
            settingsChanged.store (false);
        }
    }
    if (startRequested.exchange (false))
    {
        tickPos = 0.0;
        lastTick = -1;
        playing.store (true);
        bar.store (settings.countIn ? -1 : 0);
        chorus.store (0);
    }
    if (stopRequested.exchange (false))
    {
        playing.store (false);
        bar.store (-1);
        bass.remaining = 0;
        for (auto& g : guitars) g.release();
        for (auto& o : organs) o.release();
    }

    std::array<float, 32> buf {};
    const double ticksPerSample = settings.bpm * ticksPerBeat / (60.0 * sr);
    const float vol = settings.volume;
    for (int pos = 0; pos < n; pos += 32)
    {
        const int m = juce::jmin (32, n - pos);
        if (playing.load())
        {
            const long long now = (long long) tickPos;
            while (lastTick < now && playing.load())
                onTick (++lastTick);
            tickPos += ticksPerSample * m;
            if (stopRequested.load())
            {
                stopRequested.store (false);
                playing.store (false);
                bar.store (-1);
                for (auto& o : organs) o.release();
            }
        }
        std::fill (buf.begin(), buf.end(), 0.0f);
        for (auto& g : guitars)
            g.render (buf.data(), m);
        for (int i = 0; i < m; ++i)
        {
            float x = buf[(size_t) i] * 0.55f;
            x += bass.next() * 0.42f;
            for (auto& o : organs) x += o.next() * 0.045f;
            for (auto& d : drums) x += d.next (seed) * 0.8f;
            out[pos + i] += x * vol * 0.6f;
        }
    }
}
