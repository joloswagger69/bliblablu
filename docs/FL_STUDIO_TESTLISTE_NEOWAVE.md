# FL-Studio-Testliste – NEO//WAVE (Phase 5)

Nicht von mir geprüft (kein Windows/FL, kein Gehör). Bitte der Reihe nach abhaken, Abweichungen mit Schritt-Nummer melden.

## Laden und Grundlagen
- [ ] Plugin „NEOWAVE“ als Instrument laden, Editor öffnet (7 Seiten). Schließen/öffnen, entfernen: kein Absturz.
- [ ] MIDI-Noten spielen (Piano Roll oder Keyboard), Klang kommt, Notenende klingt sauber aus, kein Knacken.
- [ ] Projekt speichern, FL neu starten, laden: Preset/Einstellungen sind wieder da.
- [ ] Verschiedene Buffer Sizes (64/256/1024) und Sample-Rates (44,1/48/96 kHz): kein Aussetzer, gleiche Klangfarbe.

## Engines (Seite OSCILLATORS)
- [ ] Für Oszillator A und B jede der 6 Engines (Analog, Wavetable, FM, Phase Dist, Granular, **Noise**) wählen: jede klingt hörbar anders, Morph/Color wirken wie im Hinweistext.
- [ ] Noise-Engine: Color auf 1 = getonter Klang, folgt den Noten (Tonleiter spielen); Morph = weiß/pink/braun.
- [ ] Unison, Detune, Oct/Semi/Fine, Sync, Ring, Cross FM, Sub, Noise-Pegel.

## Filter / Modulation
- [ ] Alle 11 Filtertypen (LP12, LP24, HP, BP, Notch, Ladder, Acid, Formant, Comb, Diode, **SVF**) mit Cutoff/Resonanz/Drive; Filter-Hüllkurve, Key Track.
- [ ] SVF: Regler *SVF Mode* von links nach rechts: LP → BP → HP → Notch hörbar.
- [ ] Matrix: je eine Verbindung (LFO→Cutoff, ModEnv→Pitch, Velocity→Amp, Macro→…, **LFO→SVF Mode**). Aftertouch/Mod-Wheel am Keyboard, falls vorhanden.

## Effekte (Seite EFFECTS)
- [ ] Jeden Effekt einzeln ein- und ausschalten: Saturation/Distortion/Wavefolder, Chorus/Flanger/Phaser, Delay, Reverb, Compressor, **Bitcrusher**, **EQ**. Aus = Klang wie vorher.
- [ ] EQ: Low/Mid/High ±, Mid-Frequenz; Anzeige in dB/Hz korrekt.

## Presets (Seite PRESETS)
- [ ] 12 Kategorien sichtbar (Bass, Acid, Rave, Jungle, DnB, Techno, Leads, Pads, Plucks, Atmospheres, Experimental, FX). Jedes Preset anspielen, durchblättern mit `<` `>`.
- [ ] Auffällige Presets (zu laut, zu leise, kaputt, klingt nicht nach Namen) mit Namen notieren.
- [ ] Eigenes Preset speichern, laden, löschen; Favoriten (★), Suche.
- [ ] Automation: Parameter in FL automatisieren (Rechtsklick → Create automation clip), sauber ohne Knacken.
- [ ] CPU in FL bei vielen Stimmen/Unison 16 beobachten und notieren.
