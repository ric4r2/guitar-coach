#pragma once
#include "Theory.h"

// Blues licks: short phrases from Content/licks.json, written in A and moved to any key.
// Each note has a string, fret, start beat and length; techniques: h hammer-on, p pull-off, s slide,
// b bend (bend = semitones up), r release (bend = how far it was bent; it ends on the fretted note), v vibrato.

struct LickNote
{
    int string = 1, fret = 5;
    double beat = 0.0, length = 0.5;
    int bend = 0;
    juce::String tech;

    int fretMidi() const { return theory::fretMidi (string, fret); }
    int targetMidi() const { return tech == "r" ? fretMidi() : fretMidi() + bend; }   // the pitch that is heard last
};

struct Lick
{
    juce::String id, name, description, scale;   // scale: the scale id it comes from
    juce::String feel;                           // "shuffle", "straight", "slow"
    int level = 1;
    double beats = 4.0;
    std::vector<LickNote> notes;
};

std::vector<Lick> loadLicks();
std::vector<Lick> parseLicks (const juce::String& json);

// The lick moved from A to another key, kept between the nut and fret 17.
Lick transposeLick (const Lick&, int keyPc);

// Shuffle: an eighth note on the "and" is played two thirds of the way through the beat.
double swingBeat (double beat, bool swing);

// Follows a player through a lick, note by note. Notes passed on the way are forgiven: the fretted note
// before a bend reaches its target, and the notes slid through.
class LickFollower
{
public:
    enum class Result { hit, ignored, wrong, finished };

    void reset (const Lick&);
    Result feed (int midi, double time);
    int position() const { return pos; }
    int mistakes() const { return wrong; }
    bool finished() const { return pos >= (int) notes.size(); }
    const std::vector<double>& times() const { return hitTimes; }

private:
    std::vector<LickNote> notes;
    std::vector<double> hitTimes;
    int pos = 0, wrong = 0;
};

// How close the timing was to the written rhythm (1 = exact), whatever the tempo:
// the played onsets are fitted to the written ones by a straight line and the errors measured in eighth notes.
double rhythmScore (const std::vector<double>& writtenBeats, const std::vector<double>& times);
