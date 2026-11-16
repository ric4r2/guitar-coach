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
    for (int l = 0; l < 4; ++l)
    {
        theory.openLesson (l);
        save (main, folder, "02-lesson" + juce::String (l + 1));
    }

    // one of each exercise, before and after answering
    struct Shot { int lesson, exercise; const char* name; };
    const Shot shots[] = { { 0, 0, "nameStaff" }, { 0, 1, "writeStaff" }, { 0, 2, "playStaff" }, { 1, 0, "nameFret" }, { 1, 1, "findFret" },
                           { 2, 0, "steps" }, { 2, 1, "nameFretAcc" }, { 2, 3, "writeStaffAcc" }, { 3, 0, "intervalSemis" }, { 3, 1, "intervalName" },
                           { 3, 2, "playInterval" }, { 3, 4, "earInterval" } };
    for (auto& s : shots)
    {
        theory.openExercise (s.lesson, s.exercise, 7);
        engine.stopNotes();
        save (main, folder, juce::String ("03-ex-") + s.name);
        if (s.exercise == 0 || juce::String (s.name) == "intervalName" || juce::String (s.name) == "earInterval")
        {
            theory.getExerciseView().simulateChoice (juce::String (s.name) != "steps");
            save (main, folder, juce::String ("03-ex-") + s.name + "-answered");
        }
    }
    theory.openExercise (0, 0, 7);
    theory.getExerciseView().simulateFinish (9);
    save (main, folder, "04-exercise-finished");

    main.showPage (MainComponent::improv);    save (main, folder, "05-improv");
    main.showPage (MainComponent::practice);  save (main, folder, "06-practice");
    main.getPracticePage().getExplorer().startDrill (3);
    save (main, folder, "06-practice-drill");
    main.showPage (MainComponent::audio);     save (main, folder, "07-audio");

    progressFile.deleteFile();
    return 0;
}
