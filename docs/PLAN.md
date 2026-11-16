# GuitarCoach: plan

A desktop program for daily guitar practice and music theory, built for Joe Garcia: 30–60 focused
minutes a day for 12–18 months, ending ready to study Gleason's *Guitar Improv & Comping*.
Background research: [RESEARCH.md](RESEARCH.md).

## 1. Curriculum

Five strands run side by side. Every lesson has a short explanation, a diagram (staff, fretboard or
both) and an exercise that is scored. Most exercises can be answered by clicking **or by playing the
guitar** (pitch detection).

| Level (≈ months) | Reading & rhythm | Fretboard | Intervals & ear | Scales, chords, harmony | Improvisation (blues track) |
|---|---|---|---|---|---|
| **1. Foundations** (1–3) | Staff, treble clef, guitar sounds an octave lower, notes in open position; whole to eighth notes, rests, 4/4, counting | Natural notes on strings 6 and 5, then all strings; octave shapes | Half/whole steps; semitone count 0–12; hear 2nds, 3rds, 5ths, octave | Chromatic scale, major scale formula W-W-H-W-W-W-H, keys C G F; major and minor triads | 12-bar blues form in E and A; shuffle vs straight; E minor pentatonic open position; first licks |
| **2. Position player** (4–6) | Reading in 2nd and 5th position (Leavitt-style); dotted notes, ties, syncopation; key signatures | Sharps/flats everywhere; any note in under 2 s; CAGED / five octave shapes | All intervals by name, semitones and tones; interval shapes on the neck; sing and play back | All 12 major keys, circle of fifths; natural minor; diminished/augmented triads; triad inversions on string sets | Minor pentatonic in 5 shapes; blues scale; bends, slides, vibrato; call-and-response; licks by scale degree |
| **3. Harmony** (7–10) | Sixteenths, triplets, swing; Melodic Rhythms-style studies; rhythmic words (three- and four-pulse) | Major scale 5 shapes + 3-notes-per-string; arpeggios across the neck | Chord quality by ear (maj, min, dim, aug, 7ths); bass movement I-IV-V | Seventh chords; diatonic chords and Roman numerals in every key; II-V-I; harmonic function | Major pentatonic and mixing major/minor; Mixolydian over dominant chords; target chord tones on the changes; quick-change and minor blues |
| **4. Pre-Gleason** (11–18) | Reading in every position; chord charts; sight-reading drills | Modes as positions of the parent scale; full-fretboard chord tones | Progression dictation; melodic dictation | Modes, secondary dominants, modal interchange, tensions, substitute dominants; harmonic and melodic minor | Thinking in numbers; octave shapes and forward/back-slash fingerings; triad and scale-tone arpeggios; cells and fragment patterns; jazz blues; readiness quiz for Gleason Part 1–2 |

**Daily session (default 45 min, adjustable 30–60):** 5 min warm-up/technique with metronome →
10 min theory lesson → 10 min fretboard/ear drill → 15 min blues/improv → 5 min review of items due
(spaced repetition). Built on the book's practice method: short phrases, start where you make 5–10%
mistakes, raise tempo only when clean, rest, repeat in cycles.

## 2. Features

**Home:** today's session, streak, what is due for review, level progress.

**Music Theory section**
- Lesson browser by level and strand; progress saved per lesson.
- Exercises: name the note on the staff (click or play it); write notation (click the staff to place
  notes for a given scale, chord or interval); interval quiz (name, semitones, tones from the root);
  build scales and chords (pick notes, app checks them); triads and inversions; diatonic harmony and
  Roman numerals; ear training (intervals, chord qualities, progressions, melodic dictation) with
  playback.

**Improvisation section**
- Blues track: lessons (form, feels, scales, phrasing), lick library (shown as notation + tab +
  fretboard, played back at any tempo, learned with pitch detection), scale maps per key.
- Backing tracks generated in the app: 12-bar (quick-change, minor, jazz blues), shuffle /
  straight / slow / funk feels, any key, 50–200 BPM, loop, mute parts (drums, bass, keys/rhythm guitar).
- Background reading for Gleason: numbers, rhythmic words, octave shapes, cells, modes, arpeggios.

**Practice section**
- Scales and chord progressions with live pitch detection: the app shows the next note, hears you,
  marks right/wrong and timing.
- Technique, speed and accuracy drills with metronome: chromatic/spider, alternate picking,
  string skipping, legato, scale sequences (3s, 4s), tempo ladder that steps up after clean passes,
  accuracy and timing scores stored per drill.
- Fretboard mastery: note names everywhere (toggle letters / numbers / intervals), all fingerings of
  each scale (5 CAGED shapes, 3-notes-per-string), interval shapes with distance in semitones and
  tones, "find all the Gs" drills scored by pitch detection.
- Built-in tuner (reused from GuitarDrive).

**Audio:** input from the iD14 (ASIO when the SDK is present, otherwise WASAPI exclusive/shared),
48 kHz, low buffer. Pitch detection (YIN, from GuitarDrive), later chord detection by chroma.
Output: plucked-string guitar voice for examples, synthesized drums/bass/keys for backing tracks,
metronome clicks. Input monitoring off by default (the user can use the iD14's direct monitor).

## 3. Tech choice

**Recommendation: JUCE 8 (C++) standalone app, built with CMake and VS2022, same toolchain as
GuitarDrive.**

| Option | Pros | Cons |
|---|---|---|
| **JUCE 8 / C++** (chosen) | Already installed and working; lowest latency (ASIO/WASAPI) for pitch detection and in-time playing; reuse GuitarDrive's YIN tuner, metronome and vintage look; offline tests and PNG snapshots already proven | UI and notation must be drawn by hand (more code) |
| Electron / web (Web Audio) | Fast UI work, notation libraries (VexFlow) | Higher and less predictable latency on Windows, no ASIO; extra runtime; new toolchain and downloads |
| Python (PyQt + sounddevice) | Quick prototyping | Packaging and real-time audio are fragile; slower UI |

Design notes:
- Theory engine (notes, intervals, scales, chords, keys, Roman numerals) as plain C++ with unit
  tests, independent of the UI.
- Lessons and licks as data (JSON files embedded in the app), so new content needs no code.
- Notation drawn with JUCE paths (staff, clefs, note heads, stems, beams, accidentals, tab staff);
  no external font download.
- Progress stored as JSON under `%APPDATA%\GuitarCoach`.
- Look: GuitarDrive's green vintage style, adapted for reading (larger text, high contrast staff).
- Testing: unit tests for the theory engine, offline tests that feed synthetic guitar tones into the
  pitch detector and exercise scorer, and a snapshot tool that renders every screen to PNG.

## 4. Milestones

| # | Milestone | Done when |
|---|---|---|
| **M1** | **Foundation**: app shell with Home / Theory / Improvisation / Practice tabs, audio engine (iD14 input, pitch detection, guitar-voice and metronome output, audio settings), theory engine, notation and fretboard views, progress saving. Content: Level 1 theory lessons 1–4 (notes on staff, notes on the neck, half and whole steps, intervals in semitones and tones) with exercises — staff note quiz (click or play), interval quiz and ear training, fretboard note finder with pitch detection; metronome and tuner in Practice. | Builds on this machine; unit + offline audio tests pass; snapshots of every screen checked; the program opens, sees the iD14 and detects played notes. |
| **M2** | **Scales and chords**: major/minor scales, keys, circle of fifths, triads and inversions; scale practice with pitch detection; all fingerings (CAGED + 3NPS); interval shapes; technique/speed drills with tempo ladder. | Done 2026-11-16: Level 2 lessons 5-10 (major scale, key signatures and circle of fifths, minor scales and relative keys, triads, inversions on string sets, interval shapes) with spelling, writing, playing and ear exercises; Practice > Scales (7 scale types, 5 positions + 7 three-per-string fingerings, 5 run patterns) and Practice > Technique (chromatic, spider, string skipping), scored note by note with accuracy, timing evenness and a tempo ladder; unit tests and snapshots pass; self-test on the iD14. |
| M3 | Blues track: backing-track generator (drums, bass, rhythm), 12-bar forms and feels, blues lessons, lick library with notation/tab and play-along scoring. | |
| M4 | Harmony and rhythm: seventh chords, diatonic harmony and Roman numerals, progressions, chord-quality and progression ear training, rhythm reading with tap/strum onset detection, rhythmic words. | |
| M5 | Pre-Gleason: modes, arpeggios, II-V-I, secondary dominants, modal interchange, numbers and octave shapes, cells, readiness quiz. | |
| M6 | Daily session planner with spaced repetition, statistics, polish. | |

## 5. Project and repository
- Folder: `C:\Users\ricar\Downloads\RICHIE\Code\Plugins\Guitar\GuitarCoach`, separate from GuitarDrive.
- Its own git repo; commits authored only by Ricardo Cruz. A private GitHub repo only if you ask.
- JUCE comes from the copy GuitarDrive already downloaded (no new download). ASIO is optional and
  uses the same SDK path as GuitarDrive if present.
