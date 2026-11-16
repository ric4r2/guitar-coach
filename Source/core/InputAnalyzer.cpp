#include "InputAnalyzer.h"
#include <cmath>

void InputAnalyzer::Biquad::lowpass (double sr, double fc, double q)
{
    const double w = 2.0 * 3.14159265358979 * fc / sr;
    const double alpha = std::sin (w) / (2.0 * q);
    const double c = std::cos (w);
    const double a0 = 1.0 + alpha;
    b0 = (float) ((1.0 - c) / 2.0 / a0);
    b1 = (float) ((1.0 - c) / a0);
    b2 = b0;
    a1 = (float) (-2.0 * c / a0);
    a2 = (float) ((1.0 - alpha) / a0);
    z1 = z2 = 0.0f;
}

void InputAnalyzer::prepare (double sr)
{
    decimation = std::max (1, (int) std::lround (sr / 24000.0));
    analysisRate = sr / decimation;
    lp1.lowpass (sr, 4500.0, 0.54);
    lp2.lowpass (sr, 4500.0, 1.31);
    decimCounter = 0;
    std::fill (ring.begin(), ring.end(), 0.0f);
    written.store (0);
    lastAnalysed = 0;
    candidate = emitted = -1;
    candidateCount = quietCount = 0;
    slowLevel = 0.0f;
    framesSinceOnset = 1000;
    retrigger = false;
    latestValid.store (false);
}

void InputAnalyzer::push (const float* x, int n)
{
    long long w = written.load (std::memory_order_relaxed);
    float peak = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        peak = std::max (peak, std::abs (x[i]));
        const float y = lp2.process (lp1.process (x[i]));
        if (++decimCounter >= decimation)
        {
            decimCounter = 0;
            ring[(size_t) (w % ringSize)] = y;
            ++w;
        }
    }
    written.store (w, std::memory_order_release);
    if (peak > inputLevel.load (std::memory_order_relaxed))
        inputLevel.store (peak, std::memory_order_relaxed);
}

bool InputAnalyzer::analyze (NoteEvent& event, bool& gotEvent)
{
    gotEvent = false;
    const long long w = written.load (std::memory_order_acquire);
    if (w < PitchDetector::frameSize || w - lastAnalysed < hop)
        return false;
    lastAnalysed = w;

    const long long start = w - PitchDetector::frameSize;
    for (int i = 0; i < PitchDetector::frameSize; ++i)
        frame[(size_t) i] = ring[(size_t) ((start + i) % ringSize)];

    // level of the newest hop, against a slow average, to spot a fresh pluck of the same note
    double e = 0.0;
    for (int i = PitchDetector::frameSize - hop; i < PitchDetector::frameSize; ++i)
        e += (double) frame[(size_t) i] * frame[(size_t) i];
    const float level = (float) std::sqrt (e / hop);
    // an attack: much louder than the recent average, and not in the first 130 ms of the previous attack
    if (++framesSinceOnset > 12 && level > 0.01f && level > 2.0f * slowLevel)
    {
        retrigger = true;
        framesSinceOnset = 0;
        candidateCount = 0;   // the new pluck must hold steady on its own before it counts
    }
    // report attacks for tapping, from the high-frequency energy of the newest hop: a pick or a strum is a burst of
    // brightness even when the string before is still ringing as loud. 75 ms apart at least, so sixteenths come through.
    double hfE = 0.0;
    for (int i = PitchDetector::frameSize - hop; i < PitchDetector::frameSize; ++i)
    {
        const double d = (double) frame[(size_t) i] - frame[(size_t) i - 1];
        hfE += d * d;
    }
    const float hf = (float) std::sqrt (hfE / hop);
    // against the quietest of the last four hops: an attack can spread over two hops
    float recentMin = 1.0e9f;
    for (auto v : recentHf) recentMin = juce::jmin (recentMin, v);
    const bool attack = hf > 0.004f && hf > 2.0f * recentMin;
    for (size_t k = recentHf.size() - 1; k > 0; --k) recentHf[k] = recentHf[k - 1];
    recentHf[0] = hf;
    if (++framesSinceReport > 7 && attack)
    {
        framesSinceReport = 0;
        const auto scope = onsetFifo.write (1);
        const double t = (double) (w - hop) / analysisRate;
        if (scope.blockSize1 > 0) onsetTimes[(size_t) scope.startIndex1] = t;
        else if (scope.blockSize2 > 0) onsetTimes[(size_t) scope.startIndex2] = t;
    }
    slowLevel = 0.85f * slowLevel + 0.15f * level;

    const auto r = detector.detect (frame.data(), analysisRate);
    // 70 Hz floor: below drop-D (73 Hz) is mains hum (50/60 Hz), not a guitar note.
    // The level gate keeps hum and interface noise from reading as a pitch when nothing is played.
    double frameEnergy = 0.0;
    for (int i = 0; i < PitchDetector::windowSize; ++i)
        frameEnergy += (double) frame[(size_t) i] * frame[(size_t) i];
    const float frameLevel = (float) std::sqrt (frameEnergy / PitchDetector::windowSize);
    const bool good = r.valid && r.confidence > 0.8f && r.frequency > 70.0f && r.frequency < 1400.0f && frameLevel > minLevel;

    latestLevel.store (level);
    if (good)
    {
        const float m = (float) midiFromFrequency (r.frequency);
        latestHz.store (r.frequency);
        latestMidi.store (m);
        latestConf.store (r.confidence);
        latestValid.store (true);

        quietCount = 0;
        const int note = (int) std::lround (m);
        if (note == candidate)
            ++candidateCount;
        else
        {
            candidate = note;
            candidateCount = 1;
        }

        if (candidateCount >= stableFrames && (candidate != emitted || retrigger))
        {
            emitted = candidate;
            retrigger = false;
            event.midi = candidate;
            event.cents = (m - (float) candidate) * 100.0f;
            event.hz = r.frequency;
            event.time = (double) w / analysisRate;
            gotEvent = true;
        }
    }
    else
    {
        latestValid.store (false);
        candidate = -1;
        candidateCount = 0;
        if (++quietCount >= silenceFrames)
            emitted = -1;
    }
    return true;
}

bool InputAnalyzer::popOnset (double& seconds)
{
    const auto scope = onsetFifo.read (1);
    if (scope.blockSize1 > 0) { seconds = onsetTimes[(size_t) scope.startIndex1]; return true; }
    if (scope.blockSize2 > 0) { seconds = onsetTimes[(size_t) scope.startIndex2]; return true; }
    return false;
}

PitchFrame InputAnalyzer::getLatest() const
{
    PitchFrame f;
    f.valid = latestValid.load();
    f.hz = latestHz.load();
    f.midi = latestMidi.load();
    f.confidence = latestConf.load();
    f.level = latestLevel.load();
    return f;
}
