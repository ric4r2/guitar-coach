#pragma once
#include <cmath>

// What you hear of your own guitar: dry, or a gentle clean-amp colour (low cut, a touch of warmth and
// soft saturation, and a speaker-like top end) so a DI guitar does not sound thin and spiky in headphones.
class MonitorTone
{
public:
    void prepare (double sampleRate)
    {
        hp.highpass (sampleRate, 80.0, 0.707);
        low.peak (sampleRate, 140.0, 0.8, 2.5);
        lp1.lowpass (sampleRate, 5200.0, 0.75);
        lp2.lowpass (sampleRate, 7500.0, 0.6);
        hp.reset(); low.reset(); lp1.reset(); lp2.reset();
    }

    // one sample; warm = clean-amp colour, otherwise dry
    float process (float x, bool warm)
    {
        if (! warm)
            return x;
        float y = low.process (hp.process (x));
        y = std::tanh (2.2f * y) / 2.2f * 1.25f;   // soft above about -12 dBFS, clean below
        return lp2.process (lp1.process (y));
    }

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
        void reset() { z1 = z2 = 0; }
        float process (float x)
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return (float) y;
        }
        void set (double c0, double c1, double c2, double d0, double d1, double d2)
        {
            b0 = c0 / d0; b1 = c1 / d0; b2 = c2 / d0; a1 = d1 / d0; a2 = d2 / d0;
        }
        void lowpass (double sr, double f, double q)
        {
            const double w = 2.0 * 3.14159265358979 * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 - cs) / 2, 1 - cs, (1 - cs) / 2, 1 + al, -2 * cs, 1 - al);
        }
        void highpass (double sr, double f, double q)
        {
            const double w = 2.0 * 3.14159265358979 * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
            set ((1 + cs) / 2, -(1 + cs), (1 + cs) / 2, 1 + al, -2 * cs, 1 - al);
        }
        void peak (double sr, double f, double q, double gainDb)
        {
            const double A = std::pow (10.0, gainDb / 40.0), w = 2.0 * 3.14159265358979 * f / sr, cs = std::cos (w), al = std::sin (w) / (2.0 * q);
            set (1 + al * A, -2 * cs, 1 - al * A, 1 + al / A, -2 * cs, 1 - al / A);
        }
    };
    Biquad hp, low, lp1, lp2;
};
