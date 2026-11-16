#pragma once
#include <juce_audio_devices/juce_audio_devices.h>
#include "core/InputAnalyzer.h"
#include "core/GuitarVoice.h"
#include "core/Metronome.h"
#include <array>

// The audio side of the app: listens to the guitar (pitch and note detection on its own thread),
// and plays example notes and the metronome. One AudioDeviceManager for the whole app.
class AudioEngine : private juce::AudioIODeviceCallback,
                    private juce::Thread,
                    private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    // Opens the audio device: the saved setup, or the Audient iD14 (ASIO first) at 48 kHz.
    void startAudio();
    juce::AudioDeviceManager& getDeviceManager() { return deviceManager; }
    juce::String describeDevice() const;

    // input
    PitchFrame getPitch() const { return analyzer.getLatest(); }
    bool popNote (NoteEvent& e);                // notes played since the last call
    float takeInputPeak() { return analyzer.takeInputPeak(); }
    void setInputChannel (int channel);         // -1 = all active inputs mixed, otherwise 0-based active input
    int getInputChannel() const { return inputChannel.load(); }

    // output
    void playNote (int midi, double delaySeconds = 0.0, double durationSeconds = 1.5, float velocity = 0.8f);
    void playSequence (const std::vector<int>& midis, double gapSeconds = 0.75, double durationSeconds = 1.2);
    void stopNotes();

    std::atomic<bool> metronomeOn { false };
    std::atomic<float> metronomeBpm { 80.0f };
    std::atomic<int> metronomeBeats { 4 };
    std::atomic<float> metronomeVolume { 7.0f };   // 0..10
    int getMetronomePulse() const { return metronome.getPulse(); }

    std::atomic<float> outputVolume { 0.8f };

private:
    void audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                           int numSamples, const juce::AudioIODeviceCallbackContext&) override;
    void audioDeviceAboutToStart (juce::AudioIODevice* device) override;
    void audioDeviceStopped() override;
    void run() override;
    void changeListenerCallback (juce::ChangeBroadcaster*) override;
    void saveDeviceState();
    static juce::File settingsFile();

    juce::AudioDeviceManager deviceManager;
    InputAnalyzer analyzer;
    std::atomic<int> inputChannel { -1 };
    std::vector<float> monoIn, mixOut;

    struct NoteCommand { int midi; float velocity; int delaySamples; int durationSamples; };
    juce::AbstractFifo commandFifo { 128 };
    std::array<NoteCommand, 128> commands {};
    std::array<NoteCommand, 64> pending {};
    int numPending = 0;
    std::atomic<bool> stopRequested { false };

    juce::AbstractFifo noteFifo { 64 };
    std::array<NoteEvent, 64> notes {};

    std::array<GuitarVoice, 8> voices;
    int nextVoice = 0;
    Metronome metronome;
    double sampleRate = 48000.0;
    std::atomic<bool> prepared { false };
    bool started = false;
};
