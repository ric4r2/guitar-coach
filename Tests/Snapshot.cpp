// GuitarCoachSnapshot <output folder>: renders every screen to PNG, without opening an audio device.
#include "ui/MainComponent.h"

static void save (juce::Component& c, const juce::File& folder, const juce::String& name)
{
    const auto image = c.createComponentSnapshot (c.getLocalBounds(), true, 1.0f);
    auto file = folder.getChildFile (name + ".png");
    file.deleteFile();
    juce::FileOutputStream out (file);
    juce::PNGImageFormat().writeImageToStream (image, out);
    std::printf ("wrote %s\n", file.getFullPathName().toRawUTF8());
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI init;
    const juce::File folder (argc >= 2 ? juce::String (argv[1]) : juce::File::getCurrentWorkingDirectory().getFullPathName());
    folder.createDirectory();

    auto progressFile = folder.getChildFile ("snapshot-progress.json");
    progressFile.deleteFile();
    Progress progress (progressFile);
    progress.recordExercise ("L1-staff/0", 9, 10);
    progress.recordExercise ("L1-staff/1", 6, 10);

    AudioEngine engine;   // not started: no device is opened
    MainComponent main (engine, progress);
    main.setSize (1280, 820);

    main.showPage (MainComponent::home);      save (main, folder, "01-home");
    main.showPage (MainComponent::theory);
    auto& theory = main.getTheoryPage();
    for (int l = 0; l < 24; ++l)
    {
        theory.openLesson (l);
        save (main, folder, "02-lesson" + juce::String (l + 1));
    }

    // one of each exercise, before and after answering
    struct Shot { int lesson, exercise; const char* name; };
    const Shot shots[] = { { 0, 0, "nameStaff" }, { 0, 1, "writeStaff" }, { 0, 2, "playStaff" }, { 1, 0, "nameFret" }, { 1, 1, "findFret" },
                           { 2, 0, "steps" }, { 2, 1, "nameFretAcc" }, { 2, 3, "writeStaffAcc" }, { 3, 0, "intervalSemis" }, { 3, 1, "intervalName" },
                           { 3, 2, "playInterval" }, { 3, 4, "earInterval" },
                           { 4, 0, "spellScale" }, { 4, 1, "scaleDegree" }, { 4, 2, "writeScale" }, { 4, 4, "playScale" },
                           { 5, 0, "nameKeySig" }, { 5, 1, "countKeySig" }, { 5, 2, "circle" },
                           { 6, 3, "earScale" }, { 6, 4, "nameKeySigMinor" },
                           { 7, 0, "spellTriad" }, { 7, 1, "triadQuality" }, { 7, 4, "earTriad" }, { 7, 5, "playTriad" },
                           { 8, 0, "inversionName" }, { 8, 1, "triadShape" }, { 9, 0, "intervalShape" },
                           { 10, 1, "seventhQuality" }, { 11, 1, "diatonicChord" }, { 12, 1, "chordFunction" }, { 13, 1, "progressionName" },
                           { 13, 2, "earProgression" }, { 14, 0, "rhythmValue" }, { 14, 2, "tapRhythm" }, { 15, 0, "earCell" }, { 15, 2, "tapRhythm16" },
                           { 16, 0, "modeName" }, { 17, 0, "arpDegree" }, { 18, 1, "guideTone" }, { 19, 0, "secondaryDominant" }, { 19, 2, "borrowedChord" },
                           { 20, 0, "tensionNote" }, { 20, 1, "tritoneSub" }, { 21, 0, "octaveShape" }, { 21, 1, "findOctave" }, { 22, 0, "playCell" },
                           { 22, 1, "earMelody" }, { 23, 2, "readinessHarmony" } };
    for (auto& s : shots)
    {
        theory.openExercise (s.lesson, s.exercise, 7);
        engine.stopNotes();
        save (main, folder, juce::String ("03-ex-") + s.name);
        const juce::String name (s.name);
        if (name == "spellScale" || name == "writeScale" || name == "spellTriad" || name == "playScale")
        {
            theory.getExerciseView().simulateSequence (name == "spellTriad" ? 1 : 3);
            save (main, folder, "03-ex-" + name + "-partway");
            theory.getExerciseView().simulateSequence (10);
            save (main, folder, "03-ex-" + name + "-answered");
        }
        else if (name == "earMelody" || name == "playCell")
        {
            theory.getExerciseView().simulateSequence (2);
            save (main, folder, "03-ex-" + name + "-partway");
        }
        else if (name.startsWith ("tapRhythm"))
        {
            theory.getExerciseView().simulateTaps (name == "tapRhythm" ? 0.04 : 0.3);
            save (main, folder, "03-ex-" + name + "-tapped");
        }
        else if (name == "modeName" || name == "arpDegree" || name == "guideTone" || name == "secondaryDominant" || name == "borrowedChord" || name == "tensionNote"
                 || name == "tritoneSub" || name == "octaveShape" || name == "readinessHarmony")
        {
            theory.getExerciseView().simulateChoice (true);
            save (main, folder, "03-ex-" + name + "-answered");
        }
        else if ((s.exercise == 0 && s.lesson < 4) || name == "seventhQuality" || name == "diatonicChord" || name == "chordFunction" || name == "progressionName"
                 || name == "earProgression" || name == "rhythmValue" || name == "earCell" || name == "intervalName" || name == "earInterval" || name == "nameKeySig" || name == "circle"
                 || name == "earScale" || name == "triadQuality" || name == "earTriad" || name == "inversionName" || name == "triadShape" || name == "intervalShape")
        {
            theory.getExerciseView().simulateChoice (name != "steps" && name != "circle");
            save (main, folder, "03-ex-" + name + "-answered");
        }
    }
    theory.openExercise (0, 0, 7);
    theory.getExerciseView().simulateFinish (9);
    save (main, folder, "04-exercise-finished");

    main.showPage (MainComponent::improv);    save (main, folder, "05-improv");
    auto& improv = main.getImprovPage();
    for (int l = 0; l < 9; ++l)
    {
        improv.getLessons().openLesson (l);
        save (main, folder, "05-blues-lesson" + juce::String (l + 1));
    }
    struct BShot { int lesson, exercise; const char* name; };
    const BShot bshots[] = { { 0, 0, "bluesChart" }, { 0, 1, "bluesNumbers" }, { 0, 2, "spell7" }, { 0, 3, "earBluesChange" }, { 1, 1, "earFeel" },
                             { 2, 1, "boxDegree" }, { 2, 2, "playPent" }, { 5, 0, "bendTarget" }, { 5, 1, "playBend" }, { 7, 0, "chordTone" } };
    for (auto& s : bshots)
    {
        improv.getLessons().openExercise (s.lesson, s.exercise, 11);
        engine.stopNotes();
        engine.getBacking().stop();
        save (main, folder, juce::String ("05-bex-") + s.name);
        const juce::String name (s.name);
        if (name != "playPent" && name != "playBend" && name != "spell7")
        {
            improv.getLessons().getExerciseView().simulateChoice (true);
            save (main, folder, "05-bex-" + name + "-answered");
        }
    }
    improv.getLessons().openLesson (0);
    improv.showTab (ImprovPage::backingTab);
    save (main, folder, "05-backing");
    improv.getBacking().showBar (4);
    save (main, folder, "05-backing-bar5");
    improv.showTab (ImprovPage::licksTab);
    for (int l : { 0, 4, 8, 12 })
    {
        improv.getLicks().selectLick (l);
        save (main, folder, "05-lick" + juce::String (l + 1));
    }
    improv.getLicks().selectLick (1);
    improv.getLicks().startTurn();
    save (main, folder, "05-lick-turn");
    improv.getLicks().simulateTurn (0);
    save (main, folder, "05-lick-scored");
    main.showPage (MainComponent::practice);  save (main, folder, "06-practice");
    main.getPracticePage().getExplorer().startDrill (3);
    save (main, folder, "06-practice-drill");
    auto& practice = main.getPracticePage();
    practice.showTab (PracticePage::scalesTab);
    practice.getScales().select (7, "major", 1, 1);
    save (main, folder, "06-practice-scales");
    practice.getScales().startRun();
    save (main, folder, "06-practice-scales-running");
    practice.getScales().simulateRun (999, 0, 80.0);
    save (main, folder, "06-practice-scales-clean");
    practice.getScales().select (9, "minorPentatonic", 0, 2);
    save (main, folder, "06-practice-pentatonic");
    practice.getScales().select (0, "major", 5, 0);
    save (main, folder, "06-practice-3nps");
    practice.showTab (PracticePage::techniqueTab);
    save (main, folder, "06-practice-technique");
    practice.getTechnique().simulateRun (999, 6, 100.0);
    save (main, folder, "06-practice-technique-sloppy");
    main.showPage (MainComponent::audio);     save (main, folder, "07-audio");

    progressFile.deleteFile();
    return 0;
}
