# GuitarCoach

Desktop program for daily guitar practice and music theory: lessons with exercises answered by clicking,
writing on the staff or playing the guitar (pitch detection through the Audient iD14); scale practice in
every position and technique drills, checked note by note with a tempo ladder; a blues track with
backing tracks made in the app and a lick library you play along with; harmony and progressions; rhythm
reading you tap or strum; modes, arpeggios and a readiness quiz for Gleason's book; a fretboard explorer,
tuner and metronome; a daily session planner with spaced-repetition reviews, and a Progress page. See [docs/PLAN.md](docs/PLAN.md) for the curriculum and milestones and
[docs/RESEARCH.md](docs/RESEARCH.md) for the research behind it.

## Build (Windows, VS2022 Build Tools)

```
cmake -B build -G "Visual Studio 17 2022" -A x64 -DGUITARCOACH_BUILD_TESTS=ON
cmake --build build --config Release --target GuitarCoach GuitarCoachTests GuitarCoachSnapshot GuitarCoachPaths
```

- The app: `build/GuitarCoach_artefacts/Release/GuitarCoach.exe`
- Tests: `build/GuitarCoachTests_artefacts/Release/GuitarCoachTests.exe`
- Screens to PNG: `build/GuitarCoachSnapshot_artefacts/Release/GuitarCoachSnapshot.exe <folder>`
- Every user path (all exercises answered right, wrong then right and with Show answer; drills, licks, settings, window sizes): `build/GuitarCoachPaths_artefacts/Release/GuitarCoachPaths.exe <folder>`
- Progress lives in `%APPDATA%\GuitarCoach\progress.json`; a copy is kept each day in `backups\` (the last 30 days).

JUCE is taken from `../GuitarDrive/build/_deps/juce-src` when present (otherwise downloaded), and ASIO
is enabled when the Steinberg SDK is at `../_sdk/ASIO-SDK_2.3.4/ASIOSDK` (or pass `-DASIO_SDK_DIR=`).

Progress and audio settings are saved in `%APPDATA%\GuitarCoach`.

## Layout

- `Source/core` - theory engine (notes, intervals, scales, keys, triads), scale fingerings and drill patterns,
  pitch detection (YIN), note tracking, synth voice, backing-track generator, licks, lessons, progress
- `Source/audio` - audio device and real-time engine
- `Source/ui` - screens: Home, Music Theory, Improvisation, Practice, Audio
- `Content/lessons.json` - lesson text and exercise settings (Music Theory and blues tracks)
- `Content/licks.json` - the lick library, written in A
- `Tests` - offline tests and the snapshot tool
