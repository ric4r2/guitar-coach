#pragma once
#include "GuitarVoice.h"
#include "Theory.h"
#include <juce_core/juce_core.h>
#include <array>
#include <atomic>

// Blues backing tracks made on the fly: drums, bass, rhythm guitar and organ, in any key, form, feel and tempo.
// Settings come from the UI thread; render() runs on the audio thread.

struct BluesChord
{
    int offset = 0;              // semitones above the key
    juce::String quality;        // "7", "m7", "dim7"
};

struct BluesForm
{
    juce::String id, name;
    bool minor = false;
    std::vector<std::vector<BluesChord>> bars;   // 12 bars, one or two chords each (two = two beats each)
};

const std::vector<BluesForm>& bluesForms();      // standard, quickChange, minor, jazz
const BluesForm& bluesForm (const juce::String& id);
juce::String bluesChordName (int keyPc, const BluesChord&);   // "A7", "Bm7", "D#dim7"
theory::Note bluesChordRoot (int keyPc, const BluesChord&);   // spelled as in the chord name, octave 3
juce::String bluesRoman (const BluesChord&, bool minorKey);   // "I7", "IV7", "i7", "bVI7", "#IVdim7"
std::vector<int> bluesChordTones (const BluesChord&);         // semitones from the chord root: 0 4 7 10

enum class Feel { shuffle, straight, slow, funk };
const juce::StringArray& feelNames();            // "Shuffle", "Straight (rock)", "Slow blues (12/8)", "Funk"

struct BackingSettings
{
    int keyPc = 9;               // A
    int form = 0;                // index into bluesForms()
    int feel = 0;                // Feel
    double bpm = 90.0;
    bool drums = true, bass = true, guitar = true, keys = true;
    float volume = 0.8f;
    bool countIn = true;
    int stopAfterBars = 0;       // 0 = loop until stopped
};

class BackingTrack
{
public:
    static constexpr int ticksPerBeat = 12;      // divides into triplets and sixteenths
    static constexpr int beatsPerBar = 4;

    void prepare (double sampleRate);
    void setSettings (const BackingSettings&);   // any thread
    BackingSettings getSettings() const;
    void start();
    void stop();
    bool isPlaying() const { return playing.load(); }

    // where the track is: bar of the form (0..11, -1 during the count-in), beat in the bar, chorus number
    int getBar() const { return bar.load(); }
    int getBeat() const { return beat.load(); }
    int getChorus() const { return chorus.load(); }

    // adds the track into out (mono). Audio thread.
    void render (float* out, int n);

private:
    // ---- voices
    struct Bass
    {
        double sr = 48000, phase = 0, inc = 0;
        float level = 0, target = 0, env = 0;
        int remaining = 0;
        void start (int midi, int samples, float velocity);
        float next();
    };
    struct Organ
    {
        double sr = 48000, phase[4] {}, inc = 0, lfo = 0;
        float env = 0, target = 0;
        bool on = false;
        int midi = -1, remaining = 0;
        void start (int note, int samples);
        void release() { target = 0; }
        float next();
    };
    struct Drum
    {
        enum Kind { kick, snare, hat, openHat, crash };
        Kind kind = kick;
        double sr = 48000, phase = 0;
        float env = 0, tone = 0, velocity = 0, hp1 = 0, hp2 = 0, lastNoise = 0;
        int age = 0;
        void trigger (float vel) { velocity = vel; env = 1; age = 0; phase = 0; }
        float next (uint32_t& seed);
    };

    double sr = 48000.0;
    juce::SpinLock lock;
    BackingSettings pendingSettings, settings;
    std::atomic<bool> settingsChanged { false }, startRequested { false }, stopRequested { false }, playing { false };
    std::atomic<int> bar { -1 }, beat { 0 }, chorus { 0 };

    double tickPos = 0.0;         // ticks since the start (including the count-in)
    long long lastTick = -1;
    int barsPlayed = 0;

    Bass bass;
    std::array<GuitarVoice, 6> guitars;
    int nextGuitar = 0;
    std::array<Organ, 4> organs;
    std::array<Drum, 5> drums;
    uint32_t seed = 22222u;

    void onTick (long long tick);
    void playGuitar (int midi, int lengthTicks, float velocity);
    void playOrgan (const std::vector<int>& notes, int lengthTicks);
    int samplesPerTick() const;
    const BluesChord& chordAt (int barIndex, int beatInBar) const;
    int runIndex (int barIndex) const;     // how many bars in a row before this one had the same single chord
};
