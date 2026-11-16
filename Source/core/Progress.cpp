#include "Progress.h"
#include <algorithm>

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

// ---------------------------------------------------------------- dates

static juce::Time parseDate (const juce::String& d)
{
    return juce::Time (d.substring (0, 4).getIntValue(), d.substring (5, 7).getIntValue() - 1, d.substring (8, 10).getIntValue(), 12, 0);
}

juce::String Progress::today() const
{
    return todayOverride.isNotEmpty() ? todayOverride : juce::Time::getCurrentTime().formatted ("%Y-%m-%d");
}

juce::String Progress::addDays (const juce::String& date, int days)
{
    return (parseDate (date) + juce::RelativeTime::days (days)).formatted ("%Y-%m-%d");
}

int Progress::daysBetween (const juce::String& from, const juce::String& to)
{
    return juce::roundToInt ((parseDate (to) - parseDate (from)).inDays());
}

juce::DynamicObject* Progress::dayEntry (const juce::String& date)
{
    auto* h = section ("history");
    if (! h->getProperty (date).isObject())
        h->setProperty (date, juce::var (new juce::DynamicObject()));
    return h->getProperty (date).getDynamicObject();
}

static void addTo (juce::DynamicObject* o, const char* name, double amount)
{
    o->setProperty (name, (double) o->getProperty (name) + amount);
}

// ---------------------------------------------------------------- exercises and reviews

int Progress::intervalForBox (int b)
{
    static const int days[6] = { 1, 3, 7, 14, 30, 60 };
    return days[juce::jlimit (0, 5, b)];
}

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
    const bool wasPassed = e->hasProperty ("best") && (double) e->getProperty ("best") >= passMark;
    e->setProperty ("best", juce::jmax (score, (double) e->getProperty ("best")));
    e->setProperty ("last", score);
    e->setProperty ("attempts", (int) e->getProperty ("attempts") + 1);

    // spaced repetition: only once it has been passed; one step up per day at most
    const auto now = today();
    if (score >= passMark)
    {
        int b = e->hasProperty ("box") ? (int) e->getProperty ("box") : -1;
        if (! wasPassed || e->getProperty ("date").toString() != now)
            b = juce::jmin (5, b + 1);
        b = juce::jmax (0, b);
        e->setProperty ("box", b);
        e->setProperty ("due", addDays (now, intervalForBox (b)));
    }
    else if (wasPassed)
    {
        e->setProperty ("box", 0);
        e->setProperty ("due", addDays (now, 1));
    }
    e->setProperty ("date", now);
    ex->setProperty (key, entry);

    auto* d = dayEntry (now);
    addTo (d, "exercises", 1);
    addTo (d, "correct", correct);
    addTo (d, "questions", total);
    markPracticeToday();
}

double Progress::bestScore (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        if (ex->hasProperty (key))
            return (double) ex->getProperty (key)["best"];
    return -1.0;
}

double Progress::lastScore (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        if (ex->hasProperty (key))
            return (double) ex->getProperty (key)["last"];
    return -1.0;
}

int Progress::attempts (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        return (int) ex->getProperty (key)["attempts"];
    return 0;
}

juce::String Progress::lastDate (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        return ex->getProperty (key)["date"].toString();
    return {};
}

int Progress::box (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
        if (ex->getProperty (key).hasProperty ("box"))
            return (int) ex->getProperty (key)["box"];
    return -1;
}

juce::String Progress::dueDate (const juce::String& key) const
{
    if (auto* ex = section ("exercises"))
    {
        const auto e = ex->getProperty (key);
        if (e.hasProperty ("due"))
            return e["due"].toString();
        // passed before reviews existed: due the day after it was last done
        if ((double) e["best"] >= passMark && e["date"].toString().isNotEmpty())
            return addDays (e["date"].toString(), 1);
    }
    return {};
}

bool Progress::isDue (const juce::String& key) const
{
    const auto due = dueDate (key);
    return due.isNotEmpty() && due <= today();
}

juce::StringArray Progress::dueKeys() const
{
    std::vector<std::pair<juce::String, juce::String>> due;   // due date, key
    if (auto* ex = section ("exercises"))
        for (auto& p : ex->getProperties())
        {
            const auto d = dueDate (p.name.toString());
            if (d.isNotEmpty() && d <= today())
                due.push_back ({ d, p.name.toString() });
        }
    std::sort (due.begin(), due.end());
    juce::StringArray out;
    for (auto& [d, k] : due)
        out.add (k);
    return out;
}

int Progress::totalExercisesDone() const
{
    int n = 0;
    if (auto* ex = section ("exercises"))
        for (auto& p : ex->getProperties())
            n += (int) p.value["attempts"];
    return n;
}

// ---------------------------------------------------------------- drills, tempos, time

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

juce::NamedValueSet Progress::allTempos() const
{
    if (auto* d = section ("tempos"))
        return d->getProperties();
    return {};
}

void Progress::recordRun (const juce::String& kind)
{
    addTo (dayEntry (today()), kind == "tech" ? "techRuns" : "scaleRuns", 1);
    markPracticeToday();
}

void Progress::addPracticeSeconds (double seconds)
{
    if (seconds <= 0)
        return;
    addTo (dayEntry (today()), "seconds", seconds);
    unsavedSeconds += seconds;
    if (unsavedSeconds >= 30.0)
    {
        unsavedSeconds = 0.0;
        section ("days")->setProperty (today(), true);
        save();
    }
}

Progress::Day Progress::day (const juce::String& date) const
{
    Day d;
    d.date = date;
    if (auto* h = section ("history"))
    {
        const auto e = h->getProperty (date);
        d.exercises = (int) e["exercises"];
        d.correct = (int) e["correct"];
        d.questions = (int) e["questions"];
        d.scaleRuns = (int) e["scaleRuns"];
        d.techRuns = (int) e["techRuns"];
        d.seconds = (double) e["seconds"];
    }
    return d;
}

double Progress::totalPracticeSeconds() const
{
    double s = 0.0;
    if (auto* h = section ("history"))
        for (auto& p : h->getProperties())
            s += (double) p.value["seconds"];
    return s;
}

juce::var Progress::getPlan (const juce::String& date) const
{
    if (auto* p = section ("plan"))
        if (p->getProperty ("date").toString() == date)
            return p->getProperty ("items");
    return {};
}

void Progress::setPlan (const juce::String& date, const juce::var& plan)
{
    auto* p = section ("plan");
    p->setProperty ("date", date);
    p->setProperty ("items", plan);
    save();
}

juce::var Progress::setting (const juce::String& name, const juce::var& fallback) const
{
    if (auto* s = section ("settings"))
        if (s->hasProperty (name))
            return s->getProperty (name);
    return fallback;
}

void Progress::setSetting (const juce::String& name, const juce::var& value)
{
    section ("settings")->setProperty (name, value);
    save();
}

// ---------------------------------------------------------------- days

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
    auto d = today();
    // today counts if practised; otherwise the streak can still be alive from yesterday
    if (! days->hasProperty (d))
        d = addDays (d, -1);
    while (days->hasProperty (d))
    {
        ++streak;
        d = addDays (d, -1);
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
