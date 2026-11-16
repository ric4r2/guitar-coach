#include "Planner.h"
#include "Fingerings.h"

using namespace theory;

juce::var PlanItem::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("kind", (int) kind);
    o->setProperty ("title", title);
    o->setProperty ("detail", detail);
    o->setProperty ("minutes", minutes);
    o->setProperty ("lesson", lessonId);
    o->setProperty ("exercise", exercise);
    o->setProperty ("drill", drill);
    o->setProperty ("scale", scaleId);
    o->setProperty ("root", rootPc);
    o->setProperty ("lick", lickId);
    return juce::var (o);
}

PlanItem PlanItem::fromVar (const juce::var& v)
{
    PlanItem p;
    p.kind = (Kind) juce::jlimit (0, 6, (int) v["kind"]);
    p.title = v["title"].toString();
    p.detail = v["detail"].toString();
    p.minutes = (int) v["minutes"];
    p.lessonId = v["lesson"].toString();
    p.exercise = (int) v["exercise"];
    p.drill = (int) v["drill"];
    p.scaleId = v["scale"].toString();
    p.rootPc = (int) v["root"];
    p.lickId = v["lick"].toString();
    return p;
}

const Lesson* findLesson (const PlanContext& c, const juce::String& id, bool* isBlues, int* index)
{
    for (int pass = 0; pass < 2; ++pass)
    {
        const auto& list = pass == 0 ? c.theory : c.blues;
        for (int i = 0; i < (int) list.size(); ++i)
            if (list[(size_t) i].id == id)
            {
                if (isBlues) *isBlues = pass == 1;
                if (index) *index = i;
                return &list[(size_t) i];
            }
    }
    return nullptr;
}

namespace
{
    // the first lesson with an exercise not passed yet, and that exercise
    bool nextUnpassed (const std::vector<Lesson>& lessons, const Progress& p, int& lesson, int& exercise)
    {
        for (int i = 0; i < (int) lessons.size(); ++i)
            for (int x = 0; x < (int) lessons[(size_t) i].exercises.size(); ++x)
                if (! p.passed (lessons[(size_t) i].exerciseKey (x)))
                {
                    lesson = i;
                    exercise = x;
                    return true;
                }
        return false;
    }

    int dayNumber (const Progress& p) { return Progress::daysBetween ("2026-01-01", p.today()); }

    juce::String percent (double v) { return juce::String (juce::roundToInt (v * 100)) + "%"; }
}

std::vector<PlanItem> makePlan (const PlanContext& c, const Progress& p, int minutes)
{
    std::vector<PlanItem> plan;
    const bool long_ = minutes >= 60, short_ = minutes <= 30;
    const int day = dayNumber (p);

    // warm-up: a technique drill, a different one each day
    {
        PlanItem w;
        w.kind = PlanItem::Kind::warmup;
        w.drill = ((day % 3) + 3) % 3;
        w.minutes = long_ ? 10 : 5;
        w.title = "Warm-up: " + techDrillNames()[w.drill];
        const double best = p.bestTempo ("tech/" + juce::String (w.drill + 1) + "/2");
        w.detail = best > 0 ? "Best clean tempo " + juce::String (juce::roundToInt (best)) + " BPM in eighths. Start 10 below it; the tempo ladder takes you up."
                            : "Start slowly in eighth notes, around 60 BPM, with the tempo ladder on.";
        plan.push_back (w);
    }

    // the next new exercise in Music Theory
    int lessonIndex = 0, exIndex = 0;
    const bool anyNew = nextUnpassed (c.theory, p, lessonIndex, exIndex);
    if (anyNew)
    {
        const auto& l = c.theory[(size_t) lessonIndex];
        PlanItem n;
        n.kind = PlanItem::Kind::lesson;
        n.lessonId = l.id;
        n.exercise = exIndex;
        n.minutes = long_ ? 15 : 10;
        n.title = "New: " + l.title;
        n.detail = l.exercises[(size_t) exIndex].title + (exIndex == 0 ? ". Read the lesson first." : ". 8 out of 10 to pass.");
        plan.push_back (n);
    }
    const int reached = anyNew ? lessonIndex : (int) c.theory.size() - 1;

    // scales: the last scale met in the lessons so far
    if (! short_)
    {
        PlanItem s;
        s.kind = PlanItem::Kind::scales;
        s.scaleId = "major";
        s.rootPc = 7;
        for (int i = 0; i <= reached && i < (int) c.theory.size(); ++i)
            for (auto& ex : c.theory[(size_t) i].exercises)
                if (ex.type == "spellScale" || ex.type == "playScale")
                {
                    s.scaleId = ex.params.getProperty ("scale", "major").toString();
                    if (auto* roots = ex.params["roots"].getArray(); roots != nullptr && ! roots->isEmpty())
                        s.rootPc = parseNote (roots->getReference (day % roots->size()).toString()).value_or (Note { 4, 0, 3 }).pc();
                }
        s.minutes = long_ ? 10 : 5;
        s.title = "Scales: " + pcName (s.rootPc, s.rootPc == 10 || s.rootPc == 3 || s.rootPc == 8 || s.rootPc == 1) + " " + scaleType (s.scaleId).name.toLowerCase();
        s.detail = "Practice > Scales: a different position from last time, up and down with the metronome.";
        plan.push_back (s);
    }

    // reviews that are due, most overdue first
    const int maxReviews = short_ ? 1 : long_ ? 3 : 2;
    int reviews = 0;
    for (auto& key : p.dueKeys())
    {
        if (reviews >= maxReviews)
            break;
        const auto id = key.upToLastOccurrenceOf ("/", false, false);
        const int x = key.fromLastOccurrenceOf ("/", false, false).getIntValue();
        if (auto* l = findLesson (c, id); l != nullptr && x >= 0 && x < (int) l->exercises.size())
        {
            PlanItem r;
            r.kind = PlanItem::Kind::review;
            r.lessonId = id;
            r.exercise = x;
            r.minutes = 4;
            r.title = "Review: " + l->exercises[(size_t) x].title;
            r.detail = l->title + juce::String::fromUTF8 (" \xc2\xb7 best ") + percent (p.bestScore (key)) + ", due since " + p.dueDate (key) + ".";
            plan.push_back (r);
            ++reviews;
        }
    }

    // ear training: when there are few reviews, the ear exercise not done for the longest time, in lessons reached
    if (reviews < maxReviews)
    {
        juce::String bestId;
        int bestEx = -1;
        juce::String oldest = "9999";
        for (int i = 0; i <= reached && i < (int) c.theory.size(); ++i)
        {
            const auto& l = c.theory[(size_t) i];
            for (int x = 0; x < (int) l.exercises.size(); ++x)
                if (l.exercises[(size_t) x].type.startsWith ("ear"))
                {
                    const auto last = p.lastDate (l.exerciseKey (x));
                    const auto when = last.isEmpty() ? juce::String ("0000") : last;
                    if (when < oldest) { oldest = when; bestId = l.id; bestEx = x; }
                }
        }
        if (bestEx >= 0)
        {
            const auto* l = findLesson (c, bestId);
            PlanItem e;
            e.kind = PlanItem::Kind::ear;
            e.lessonId = bestId;
            e.exercise = bestEx;
            e.minutes = 5;
            e.title = "Ear: " + l->exercises[(size_t) bestEx].title;
            e.detail = l->title + ". Sing the answer before you click.";
            plan.push_back (e);
        }
    }

    // blues track: the next blues lesson, and a lick to learn and use (a short session alternates them)
    int bl = 0, bx = 0;
    const bool bluesNew = nextUnpassed (c.blues, p, bl, bx);
    const bool wantBlues = bluesNew && (! short_ || day % 2 == 0);
    if (wantBlues)
    {
        const auto& l = c.blues[(size_t) bl];
        PlanItem b;
        b.kind = PlanItem::Kind::blues;
        b.lessonId = l.id;
        b.exercise = bx;
        b.minutes = 10;
        b.title = "Blues: " + l.title;
        b.detail = l.exercises[(size_t) bx].title + ".";
        plan.push_back (b);
    }
    if (! c.licks.empty() && (! short_ || ! wantBlues))
    {
        // the first lick not learned yet, or the one played least recently
        const Lick* pick = nullptr;
        for (auto& lk : c.licks)
            if (p.bestScore ("lick/" + lk.id) < Progress::passMark) { pick = &lk; break; }
        if (pick == nullptr)
        {
            juce::String oldest = "9999";
            for (auto& lk : c.licks)
                if (p.lastDate ("lick/" + lk.id) < oldest) { oldest = p.lastDate ("lick/" + lk.id); pick = &lk; }
        }
        PlanItem m;
        m.kind = PlanItem::Kind::improv;
        m.lickId = pick->id;
        m.minutes = long_ ? 15 : 10;
        m.title = "Improvise: " + pick->name;
        m.detail = "Learn it in Licks, then play it over a backing track and change one thing each time: rhythm, last note, key.";
        plan.push_back (m);
    }
    return plan;
}

std::vector<PlanItem> todaysPlan (const PlanContext& c, Progress& p, int minutes)
{
    const auto stored = p.getPlan (p.today());
    if (stored.isObject() && (int) stored["minutes"] == minutes)
        if (auto* items = stored["items"].getArray())
        {
            std::vector<PlanItem> plan;
            for (auto& v : *items)
                plan.push_back (PlanItem::fromVar (v));
            return plan;
        }
    const auto plan = makePlan (c, p, minutes);
    auto* o = new juce::DynamicObject();
    o->setProperty ("minutes", minutes);
    juce::Array<juce::var> items;
    for (auto& item : plan)
        items.add (item.toVar());
    o->setProperty ("items", items);
    p.setPlan (p.today(), juce::var (o));
    return plan;
}

bool isDone (const PlanItem& item, const Progress& p)
{
    const auto today = p.today();
    switch (item.kind)
    {
        case PlanItem::Kind::warmup: return p.day (today).techRuns > 0;
        case PlanItem::Kind::scales: return p.day (today).scaleRuns > 0;
        case PlanItem::Kind::improv: return p.lastDate ("lick/" + item.lickId) == today;
        default:                     return p.lastDate (item.lessonId + "/" + juce::String (item.exercise)) == today;
    }
}
