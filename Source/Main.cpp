#include "ui/MainComponent.h"

class GuitarCoachApp : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override { return false; }

    void initialise (const juce::String& args) override
    {
        progress = std::make_unique<Progress> (Progress::defaultFile());
        engine = std::make_unique<AudioEngine>();
        engine->startAudio();
        writeAudioLog ("started");
        window = std::make_unique<MainWindow> (*engine, *progress);

        // GuitarCoach --selftest: listen to the input for 5 seconds, log what was heard, quit
        if (args.contains ("--selftest"))
            selfTest = std::make_unique<SelfTest> (*this);
    }

    // %APPDATA%\GuitarCoach\audio-log.txt: which device is open, for troubleshooting
    void writeAudioLog (const juce::String& event, const juce::String& extra = {})
    {
        auto file = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory).getChildFile ("GuitarCoach").getChildFile ("audio-log.txt");
        file.getParentDirectory().createDirectory();
        juce::String s;
        s << juce::Time::getCurrentTime().toString (true, true) << "  " << event << "\n  device: " << engine->describeDevice() << "\n";
        if (auto* d = engine->getDeviceManager().getCurrentAudioDevice())
            s << "  active inputs: " << d->getActiveInputChannels().toString (2) << "  outputs: " << d->getActiveOutputChannels().toString (2)
              << "  input latency: " << d->getInputLatencyInSamples() << "  output latency: " << d->getOutputLatencyInSamples() << " samples\n";
        s << extra;
        file.appendText (s);
    }

    void shutdown() override
    {
        selfTest = nullptr;
        window = nullptr;
        engine = nullptr;
        progress = nullptr;
    }

    void systemRequestedQuit() override { quit(); }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        MainWindow (AudioEngine& e, Progress& p)
            : DocumentWindow ("GuitarCoach", juce::Colour (0xff0f2219), DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (e, p), true);
            setResizable (true, true);
            setResizeLimits (1100, 720, 2400, 1600);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }
        void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
    };

    struct SelfTest : juce::Timer
    {
        explicit SelfTest (GuitarCoachApp& a) : app (a) { startTimer (50); }
        void timerCallback() override
        {
            peak = juce::jmax (peak, app.engine->takeInputPeak());
            const auto f = app.engine->getPitch();
            if (f.valid) { ++validFrames; lastHz = f.hz; }
            NoteEvent e;
            while (app.engine->popNote (e)) heard << theory::noteFromMidi (e.midi).name (true) << " ";
            if (++ticks >= 100)
            {
                stopTimer();
                juce::String s;
                s << "  input peak over 5 s: " << juce::Decibels::toString (juce::Decibels::gainToDecibels (peak)) << "\n"
                  << "  pitch frames: " << validFrames << (validFrames > 0 ? ", last " + juce::String (lastHz, 1) + " Hz" : juce::String()) << "\n"
                  << "  notes heard: " << (heard.isEmpty() ? juce::String ("none") : heard) << "\n";
                app.writeAudioLog ("selftest", s);
                app.systemRequestedQuit();
            }
        }
        GuitarCoachApp& app;
        float peak = 0.0f, lastHz = 0.0f;
        int ticks = 0, validFrames = 0;
        juce::String heard;
    };

    std::unique_ptr<SelfTest> selfTest;
    std::unique_ptr<Progress> progress;
    std::unique_ptr<AudioEngine> engine;
    std::unique_ptr<MainWindow> window;
};

START_JUCE_APPLICATION (GuitarCoachApp)
