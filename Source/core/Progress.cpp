#include "Progress.h"

Progress::Progress (juce::File f) : file (std::move (f))
{
    if (file.existsAsFile())
        data = juce::JSON::parse (file);
    if (! data.isObject())
        data = juce::var (new juce::DynamicObject());
}

juce::File Progress::defaultFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("GuitarCoach").getChildFile ("progress.json");
}

juce::DynamicObject* Progress::section (const char* name)
{
    auto* root = data.getDynamicObject();
    if (! root->getProperty (name).isObject())
        root->setProperty (name, juce::var (new juce::DynamicObject()));
    return root->getProperty (name).getDynamicObject();
}

const juce::DynamicObject* Progress::section (const char* name) const
{
    return data.getDynamicObject()->getProperty (name).getDynamicObject();
}

static juce::String today() { return juce::Time::getCurrentTime().formatted ("%Y-%m-%d"); }

void Progress::recordExercise (const juce::String& key, int correct, int total)
{
    if (total <= 0)
        return;
    auto* ex = section ("exercises");
    auto entry = ex->getProperty (key);
    if (! entry.isObject())
        entry = juce::var (new juce::DynamicObject());
    auto* e = entry.getDynamicObject();
    const double score = (double) correct / total;
    e->setProperty ("best", juce::jmax (score, (double) e->getProperty ("best")));
    e->setProperty ("last", score);
    e->setProperty ("attempts", (int) e->getProperty ("attempts") + 1);
    e->setProperty ("date", today());
    ex->setProperty (key, entry);
    markPracticeToday();
}

double Progress::bestScore (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        if (ex->hasProperty (key))
            return (double) ex->getProperty (key)["best"];
    return -1.0;
}

int Progress::attempts (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        return (int) ex->getProperty (key)["attempts"];
    return 0;
}

int Progress::totalExercisesDone() const
{
    int n = 0;
    if (auto* ex = section ("exercises"))
        for (auto& p : ex->getProperties())
            n += (int) p.value["attempts"];
    return n;
}

void Progress::recordDrill (const juce::String& key, double value)
{
    auto* d = section ("drills");
    const double old = d->hasProperty (key) ? (double) d->getProperty (key) : -1.0;
    if (old < 0 || value < old)
        d->setProperty (key, value);
    markPracticeToday();
}

double Progress::bestDrill (const juce::String& key) const
{
    if (auto* d = section ("drills"))
        if (d->hasProperty (key))
            return (double) d->getProperty (key);
    return -1.0;
}

void Progress::recordTempo (const juce::String& key, double bpm)
{
    auto* d = section ("tempos");
    if (bpm > (double) d->getProperty (key))
        d->setProperty (key, bpm);
    markPracticeToday();
}

double Progress::bestTempo (const juce::String& key) const
{
    if (auto* d = section ("tempos"))
        return (double) d->getProperty (key);
    return 0.0;
}

void Progress::markPracticeToday()
{
    auto* days = section ("days");
    days->setProperty (today(), true);
    save();
}

int Progress::streakDays() const
{
    auto* days = section ("days");
    if (days == nullptr)
        return 0;
    int streak = 0;
    auto t = juce::Time::getCurrentTime();
    // today counts if practised; otherwise the streak can still be alive from yesterday
    if (! days->hasProperty (t.formatted ("%Y-%m-%d")))
        t -= juce::RelativeTime::days (1);
    while (days->hasProperty (t.formatted ("%Y-%m-%d")))
    {
        ++streak;
        t -= juce::RelativeTime::days (1);
    }
    return streak;
}

void Progress::save() const
{
    if (file == juce::File())
        return;
    file.getParentDirectory().createDirectory();
    file.replaceWithText (juce::JSON::toString (data));
}
