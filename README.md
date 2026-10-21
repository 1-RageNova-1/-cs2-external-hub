# RageNova

CS2 External Hub — ESP + Triggerbot

**Repo:** https://github.com/1-RageNova-1/-cs2-external-hub

Based on the modernized [IMXNOOBX/cs2-external-esp](https://github.com/IMXNOOBX/cs2-external-esp) codebase, cleaned and rebranded.

## Features
- ESP (box, skeleton, health, armor, name, tracers, flags)
- **Hide teammates** toggle (`Show Teammates` in ESP tab — uncheck to hide)
- Triggerbot (keybind, delay, team check)
- Radar, bomb info, spectators, streamproof
- Automatic offset scanning

## Build
1. `git clone --recursive https://github.com/1-RageNova-1/-cs2-external-hub`
2. Open `cs2-external-esp.sln` in Visual Studio 2022+
3. Build **x64 | Release**
4. Run as Administrator while CS2 is open (not fullscreen exclusive)

## Notes
- Game Debug overlay removed (was covering chat / clutter)
- Console / Dev menu stripped from release UI
- Triggerbot kept intact
