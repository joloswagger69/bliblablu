# Changelog

## 0.5.0 – Phase 5: NEO//WAVE nach Spezifikation
- Neu: **Noise-Engine** (sechste Klang-Engine, für Oszillator A und B), **SVF-Filter** mit stufenlosem LP/BP/HP/Notch-Morph (auch per Matrix modulierbar), **EQ** (3 Bänder) und **Bitcrusher** als eigenständige, einzeln schaltbare Effekte.
- Presets: 138 (vorher 108) in den 12 Kategorien der Spezifikation (Bass, Acid, Rave, Jungle, DnB, Techno, Leads, Pads, Plucks, Atmospheres, Experimental, FX); 30 neue Presets, alte neu einsortiert, Pegel neu kalibriert.
- Tests: `neowave_presets`, `neowave_catalog`, `neowave_features`, `neowave_gui` (alle in `ctest`).
- Fix: Bundle-IDs ohne Leerzeichen (macOS-Warnung), Anzeige der neuen Regler mit Einheiten.
- Bekannt: noch nie unter Windows gebaut, nicht in FL Studio getestet, Klang nie gehört.

## 0.4.0 – Phase 4: SPACE//FX
- SPACE//FX ist jetzt ein vollwertiger Delay+Reverb-Effekt (Stereo, auch Mono-Layout).
- 9 Delay-Typen (Digital, Ping Pong, Stereo, Multitap, Tape, Reverse, Pitch, Modulated, Filtered), Tempo-Sync mit Host-Tempo (18 Notenwerte inkl. punktiert/Triole), Feedback, Width, Pan, Low/High Cut, Modulation, L/R-Offset, Ducking, Infinite Feedback, Freeze.
- 7 Reverb-Modi (Room, Hall, Plate, Shimmer, Reverse, Modulated, Infinite) auf Basis eines echten 8-Leitungs-FDN (Hadamard-Mischung, Diffusion, Early Reflections, Dämpfung, modulierte Leitungen), mit Decay, Pre-Delay, Size, Damping, Diffusion, Width, Modulation, Cuts, Ducking, Freeze.
- 5 Routings: Delay > Reverb, Reverb > Delay, Parallel, Delay only, Reverb only (8-ms-Überblendung beim Umschalten).
- 45 Werkspresets in 10 Kategorien, musikalischer Randomizer (begrenzte Bereiche, Freeze/Infinite bleiben aus, Ausgangspegel unverändert), Benutzerpresets.
- Freeze/Infinite sind durch eine Soft-Begrenzung in der Rückkopplung im Pegel beschränkt (getestet über 40–60 s).
- Tests: `SpaceTest` (DSP-Messungen), `SpaceProcessorTest` (Parameter, Tempo-Sync, Presets, Randomizer, State, Mono, Blockgrößen, Fuzz), `SpaceShot` (GUI-Rendering + Button-Checks, nur Linux/xvfb).
- Bekannt: noch nie unter Windows gebaut, nicht in FL Studio getestet, Klang und Presets nie gehört (siehe `plugins/SpaceFX/README.md`).

## 0.3.0 – Phase 3: MELODY//STUDIO
- MELODY//STUDIO ist jetzt ein vollwertiger Musik-Ideengenerator mit MIDI-Export und einfachem Vorhör-Synth (kein Klangerzeuger für die Produktion).
- Chords (7 Moods, Komplexität, Erweiterungen bis 13, Voicings, Umkehrungen, Voice Leading, Sus/dim/aug, 4 Rhythmen), Melody (5 Charaktere, Dichte, Rhythmik, Wiederholung, Variation, Sprungweite), Arp, Bass (7 Stile), Acid//Flow (5 Charaktere, Slide, Accent, Tie, Wahrscheinlichkeit, 16/32 Schritte).
- Make a Track: 10 Genres, schreibt Tempo, Swing und alle Teil-Einstellungen und erzeugt alles aus einem Seed.
- Jeder Teil lässt sich einzeln neu erzeugen (Generate / New idea / Variation / Clear), Undo, Tonart/Skala/Länge/Taktart werden für alle Teile gemeinsam gehalten.
- Export: einzelne Teile oder alles als .mid (getrennt oder eine Datei mit einer Spur je Teil). Echtzeit-MIDI-Ausgabe an andere Plugins ist **nicht** enthalten.
- Tests: `MelodyTest` (Musiklogik), `MelodyProcessorTest` (Erzeugung, Undo, MIDI-Round-Trip, State, Audio, Fuzz), `MelodyShot` (GUI-Rendering + Button-Checks, nur Linux/xvfb).
- Bekannt: noch nie unter Windows gebaut, nicht in FL Studio getestet, Klang nie gehört (siehe `plugins/MelodyStudio/README.md`).

## 0.2.0 – Phase 2: RAVE//STATION
- RAVE//STATION ist jetzt ein vollwertiges Instrument mit eigener Sample-Engine und internem Sequencer (kein Zugriff auf Playlist-Audio-Clips von FL).
- 16 Pads (WAV/AIFF/FLAC/OGG, Drag & Drop), Pad-Einstellungen, Layer, Kits speichern/laden, mitgelieferter synthetischer Default-Kit.
- Step-Sequencer (16 Muster, 4–64 Schritte, 8 Taktarten, Swing, Humanize, Velocity/Wahrscheinlichkeit/Ratchet/Roll/Ghost/Pitch pro Schritt, Copy/Paste, Undo).
- Break-Slicer (Transienten-Erkennung, manuelle Marker, Arrangement mit Reverse/Pitch/Stretch/Stutter, Slices -> Pads, Slice-Pattern).
- Breakbeat-Generator (8 Genres, Parameter, Seed) und EVOLVE (4 Varianten, Original bleibt unverändert, Variante übernehmen).
- Sample-Morph pro Pad (Reverse, Pitch, Stretch, Filter, Drive, Bitcrush, Stutter, Glitch, Layer, Resample).
- Export: MIDI, Pattern-WAV, Stems, Slices, einzelne Pads.
- Tests: `RaveTest` (Engine), `RaveProcessorTest` (State/Presets/Export/Morph/Generator), `RaveShot` (GUI-Rendering + Button-Checks, nur Linux/xvfb).
- Bekannt: noch nie unter Windows gebaut und nicht in FL Studio getestet (siehe `plugins/RaveStation/README.md`).

## 0.1.0 – Phase 1: technisches Fundament
- Monorepo mit gemeinsamer Core-Bibliothek (`core/`), vier Plugin-Ordnern und eigenem Testwerkzeug.
- Neu: RAVE-STATION, MELODY-STUDIO, SPACE-FX als **Fundament-Builds** (nur Testfunktionen, noch keine Plugin-Features).
- NEO//WAVE als viertes Plugin eingehängt (bestehender Funktionsumfang, unverändert).
- Eindeutige Plugin-Codes und Namen, per CMake erzwungen.
- Windows-x64-CI (MSVC), Linux-Referenz-Build, Release-Job für Tags `v*`.
