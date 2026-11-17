#include "AudioEngine.h"

AudioEngine::AudioEngine() : juce::Thread ("GuitarCoach pitch") {}

AudioEngine::~AudioEngine()
{
    stopThread (1000);
    deviceManager.removeChangeListener (this);
    deviceManager.removeAudioCallback (this);
    deviceManager.closeAudioDevice();
}

juce::File AudioEngine::settingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("GuitarCoach").getChildFile ("audio-device.xml");
}

void AudioEngine::startAudio()
{
    if (started)
        return;
    started = true;

    std::unique_ptr<juce::XmlElement> saved;
    if (settingsFile().existsAsFile())
        saved = juce::XmlDocument::parse (settingsFile());

    juce::String error;
    if (saved != nullptr)
        error = deviceManager.initialise (2, 2, saved.get(), true);

    if (saved == nullptr || error.isNotEmpty() || deviceManager.getCurrentAudioDevice() == nullptr)
    {
        // first run: look for the Audient, preferring its ASIO driver, then Windows Audio
        deviceManager.initialise (2, 2, nullptr, false);
        bool found = false;
        for (auto typeName : { "ASIO", "Windows Audio", "Windows Audio (Exclusive Mode)" })
        {
            for (auto* type : deviceManager.getAvailableDeviceTypes())
            {
                if (type->getTypeName() != typeName)
                    continue;
                type->scanForDevices();
                for (auto& name : type->getDeviceNames (true))
                {
                    if (! (name.containsIgnoreCase ("Audient") || name.containsIgnoreCase ("iD14")))
                        continue;
                    deviceManager.setCurrentAudioDeviceType (typeName, true);
                    auto setup = deviceManager.getAudioDeviceSetup();
                    setup.inputDeviceName = name;
                    setup.outputDeviceName = name;
                    if (juce::String (typeName) == "Windows Audio")
                    {
                        // WASAPI lists inputs and outputs separately: pick the iD14's first analogue pair
                        for (auto& out : type->getDeviceNames (false))
                            if (out.containsIgnoreCase ("iD14") && out.containsIgnoreCase ("1/2"))
                                setup.outputDeviceName = out;
                        if (! name.containsIgnoreCase ("1/2"))
                            continue;
                    }
                    setup.sampleRate = 48000.0;
                    setup.bufferSize = juce::String (typeName) == "ASIO" ? 128 : 480;
                    setup.useDefaultInputChannels = false;
                    setup.inputChannels.clear();
                    setup.inputChannels.setRange (0, 2, true);
                    setup.useDefaultOutputChannels = true;
                    if (deviceManager.setAudioDeviceSetup (setup, true).isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr)
                    {
                        found = true;
                        break;
                    }
                }
                break;
            }
            if (found)
                break;
        }
        if (! found)
            deviceManager.initialise (2, 2, nullptr, true);
    }

    deviceManager.addAudioCallback (this);
    deviceManager.addChangeListener (this);
    startThread (juce::Thread::Priority::high);
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster*) { saveDeviceState(); }

void AudioEngine::saveDeviceState()
{
    if (auto xml = deviceManager.createStateXml())
    {
        settingsFile().getParentDirectory().createDirectory();
        xml->writeTo (settingsFile());
    }
}

juce::String AudioEngine::describeDevice() const
{
    auto* d = deviceManager.getCurrentAudioDevice();
    if (d == nullptr)
        return "No audio device";
    return d->getName() + " (" + d->getTypeName() + juce::String::fromUTF8 (") · ") + juce::String ((int) d->getCurrentSampleRate()) + juce::String::fromUTF8 (" Hz · ")
           + juce::String (d->getCurrentBufferSizeSamples()) + " samples";
}

void AudioEngine::setInputChannel (int channel) { inputChannel.store (channel); }

void AudioEngine::audioDeviceAboutToStart (juce::AudioIODevice* device)
{
    prepared.store (false);
    sampleRate = device->getCurrentSampleRate();
    const int block = juce::jmax (device->getCurrentBufferSizeSamples(), 4096);
    monoIn.assign ((size_t) block, 0.0f);
    mixOut.assign ((size_t) block, 0.0f);
    analyzer.prepare (sampleRate);
    for (auto& v : voices)
        v.prepare (sampleRate);
    metronome.prepare (sampleRate);
    backing.prepare (sampleRate);
    monitorTone.prepare (sampleRate);
    monitorGain = 0.0f;
    numPending = 0;
    prepared.store (true);
}

void AudioEngine::audioDeviceStopped() { prepared.store (false); }

void AudioEngine::audioDeviceIOCallbackWithContext (const float* const* in, int numIn, float* const* out, int numOut,
                                                    int numSamples, const juce::AudioIODeviceCallbackContext&)
{
    for (int c = 0; c < numOut; ++c)
        if (out[c] != nullptr)
            juce::FloatVectorOperations::clear (out[c], numSamples);

    if (! prepared.load() || numSamples > (int) monoIn.size())
        return;

    // input: the chosen channel, or all of them mixed
    const int chosen = inputChannel.load();
    juce::FloatVectorOperations::clear (monoIn.data(), numSamples);
    for (int c = 0; c < numIn; ++c)
        if (in[c] != nullptr && (chosen < 0 || chosen == c))
            juce::FloatVectorOperations::add (monoIn.data(), in[c], numSamples);
    analyzer.push (monoIn.data(), numSamples);

    // output: example notes and metronome
    if (stopRequested.exchange (false))
    {
        numPending = 0;
        for (auto& v : voices)
            v.release();
    }

    {
        const auto scope = commandFifo.read (commandFifo.getNumReady());
        auto take = [&] (int start, int size)
        {
            for (int i = start; i < start + size; ++i)
                if (numPending < (int) pending.size())
                    pending[(size_t) numPending++] = commands[(size_t) i];
        };
        take (scope.startIndex1, scope.blockSize1);
        take (scope.startIndex2, scope.blockSize2);
    }

    float* mix = mixOut.data();
    juce::FloatVectorOperations::clear (mix, numSamples);

    // start notes whose time has come; a note due mid-block starts at the next block (a few ms)
    for (int i = 0; i < numPending;)
    {
        auto& cmd = pending[(size_t) i];
        if (cmd.delaySamples < numSamples)
        {
            voices[(size_t) nextVoice].start (cmd.midi, cmd.velocity, cmd.durationSamples);
            if (cmd.bend != 0.0f)
                voices[(size_t) nextVoice].bend (cmd.bend, cmd.bendAfterSamples, (int) (0.12 * sampleRate));
            nextVoice = (nextVoice + 1) % (int) voices.size();
            pending[(size_t) i] = pending[(size_t) --numPending];
        }
        else
        {
            cmd.delaySamples -= numSamples;
            ++i;
        }
    }
    for (auto& v : voices)
        v.render (mix, numSamples);
    backing.render (mix, numSamples);

    const float vol = outputVolume.load() * 0.45f;
    juce::FloatVectorOperations::multiply (mix, vol, numSamples);

    // your guitar, on top of everything else, fading in and out smoothly when switched
    {
        const float target = monitorOn.load() ? monitorVolume.load() * monitorVolume.load() * 2.0f : 0.0f;
        const bool warm = monitorWarm.load();
        if (target > 0.0f || monitorGain > 1.0e-4f)
            for (int i = 0; i < numSamples; ++i)
            {
                monitorGain += 0.002f * (target - monitorGain);
                mix[i] += monitorTone.process (monoIn[(size_t) i], warm) * monitorGain;
            }
    }

    if (numOut >= 1 && out[0] != nullptr)
    {
        float* l = out[0];
        // with a single output, the metronome's right channel goes to the (already used) input scratch buffer
        float* r = numOut >= 2 && out[1] != nullptr ? out[1] : monoIn.data();
        juce::FloatVectorOperations::copy (l, mix, numSamples);
        juce::FloatVectorOperations::copy (r, mix, numSamples);
        metronome.process (l, r, numSamples, metronomeOn.load(), metronomeBpm.load(),
                           metronomeBeats.load(), 1, metronomeVolume.load());
    }
}

void AudioEngine::run()
{
    while (! threadShouldExit())
    {
        bool any = false;
        if (prepared.load())
        {
            NoteEvent e;
            bool got = false;
            while (analyzer.analyze (e, got))
            {
                any = true;
                if (got)
                {
                    const auto scope = noteFifo.write (1);
                    if (scope.blockSize1 > 0) notes[(size_t) scope.startIndex1] = e;
                    else if (scope.blockSize2 > 0) notes[(size_t) scope.startIndex2] = e;
                }
            }
        }
        if (! any)
            wait (3);
    }
}

bool AudioEngine::popNote (NoteEvent& e)
{
    const auto scope = noteFifo.read (1);
    if (scope.blockSize1 > 0) { e = notes[(size_t) scope.startIndex1]; return true; }
    if (scope.blockSize2 > 0) { e = notes[(size_t) scope.startIndex2]; return true; }
    return false;
}

void AudioEngine::playNote (int midi, double delaySeconds, double durationSeconds, float velocity)
{
    const auto scope = commandFifo.write (1);
    NoteCommand cmd { midi, velocity, (int) (delaySeconds * sampleRate), juce::jmax (1, (int) (durationSeconds * sampleRate)) };
    if (scope.blockSize1 > 0) commands[(size_t) scope.startIndex1] = cmd;
    else if (scope.blockSize2 > 0) commands[(size_t) scope.startIndex2] = cmd;
}

void AudioEngine::playBentNote (int midi, double delaySeconds, double durationSeconds, float bendSemitones, double bendAfterSeconds, float velocity)
{
    const auto scope = commandFifo.write (1);
    NoteCommand cmd { midi, velocity, (int) (delaySeconds * sampleRate), juce::jmax (1, (int) (durationSeconds * sampleRate)),
                      bendSemitones, (int) (bendAfterSeconds * sampleRate) };
    if (scope.blockSize1 > 0) commands[(size_t) scope.startIndex1] = cmd;
    else if (scope.blockSize2 > 0) commands[(size_t) scope.startIndex2] = cmd;
}

void AudioEngine::playSequence (const std::vector<int>& midis, double gap, double duration)
{
    for (size_t i = 0; i < midis.size(); ++i)
        playNote (midis[i], gap * (double) i, duration);
}

void AudioEngine::stopNotes() { stopRequested.store (true); }
