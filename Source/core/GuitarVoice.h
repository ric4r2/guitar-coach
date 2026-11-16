#pragma once
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

// Plucked-string voice (Karplus-Strong), used to play example notes and ear-training questions.
class GuitarVoice
{
public:
    void prepare (double sampleRate)
    {
        sr = sampleRate;
        line.assign ((size_t) (sr / 30.0) + 4, 0.0f);
        active = false;
    }

    void start (int midi, float velocity, int durationSamples)
    {
        const double hz = 440.0 * std::pow (2.0, (midi - 69) / 12.0);
        // the averaging filter delays by half a sample, so the line is half a sample shorter
        delay = std::clamp (sr / hz - 0.5, 2.0, (double) line.size() - 3.0);
        const int len = (int) std::ceil (delay) + 1;

        // the pluck: noise smoothed a little so it is not too harsh, louder for higher velocity
        float prev = 0.0f;
        const float smooth = 0.55f;
        for (int i = 0; i < (int) line.size(); ++i)
        {
            float v = 0.0f;
            if (i < len)
            {
                seed = seed * 1664525u + 1013904223u;
                const float noise = (float) ((seed >> 9) & 0x7fff) / 16384.0f - 1.0f;
                v = prev + smooth * (noise - prev);
                prev = v;
            }
            line[(size_t) i] = v * velocity;
        }
        // remove any DC so the note does not thump
        float mean = 0.0f;
        for (int i = 0; i < len; ++i) mean += line[(size_t) i];
        mean /= (float) len;
        for (int i = 0; i < len; ++i) line[(size_t) i] -= mean;

        writePos = len % (int) line.size();   // the pluck sits just behind the write position, so it is read first
        last = 0.0f;
        // longer sustain for low notes, like a real string
        const double seconds = std::clamp (3.5 - (midi - 40) * 0.05, 0.8, 3.5);
        loss = (float) std::pow (0.001, 1.0 / (seconds * hz));
        remaining = durationSamples;
        targetDelay = delay;
        bendWait = -1;
        releasing = false;
        level = 1.0f;
        active = true;
        midiNote = midi;
    }

    void release() { releasing = true; }

    // Bend the string: after waitSamples, glide by `semitones` (negative = release back) over glideSamples.
    void bend (double semitones, int waitSamples, int glideSamples)
    {
        targetDelay = std::clamp (delay / std::pow (2.0, semitones / 12.0), 2.0, (double) line.size() - 3.0);
        bendWait = std::max (0, waitSamples);
        bendStep = (targetDelay - delay) / std::max (1, glideSamples);
    }
    bool isActive() const { return active; }
    int getNote() const { return midiNote; }

    // Adds the voice into out (mono).
    void render (float* out, int n)
    {
        if (! active)
            return;
        const int size = (int) line.size();
        for (int i = 0; i < n; ++i)
        {
            if (bendWait > 0)
                --bendWait;
            else if (bendWait == 0)
            {
                delay += bendStep;
                if ((bendStep > 0 && delay >= targetDelay) || (bendStep <= 0 && delay <= targetDelay))
                {
                    delay = targetDelay;
                    bendWait = -1;
                }
            }
            double readPos = writePos - delay;
            while (readPos < 0) readPos += size;
            const int i0 = (int) readPos;
            const float frac = (float) (readPos - i0);
            const float a = line[(size_t) (i0 % size)], b = line[(size_t) ((i0 + 1) % size)];
            const float x = a + frac * (b - a);

            const float y = loss * 0.5f * (x + last);
            last = x;
            line[(size_t) writePos] = y;
            writePos = (writePos + 1) % size;

            out[i] += x * level;

            if (remaining > 0 && --remaining == 0)
                releasing = true;
            if (releasing)
            {
                level *= 0.9993f;
                if (level < 0.0005f)
                {
                    active = false;
                    return;
                }
            }
        }
    }

private:
    double sr = 48000.0, delay = 100.0, targetDelay = 100.0, bendStep = 0.0;
    int bendWait = -1;
    std::vector<float> line;
    int writePos = 0, remaining = 0, midiNote = -1;
    float last = 0.0f, loss = 0.996f, level = 1.0f;
    bool active = false, releasing = false;
    uint32_t seed = 12345u;
};
