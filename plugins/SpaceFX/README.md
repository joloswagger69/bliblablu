# SPACE//FX (Phase 4)

VST3-Effekt für FL Studio: **Delay und Reverb in einem Plugin**, mit 9 Delay-Typen, 7 Reverb-Modi, 5 Routings, Freeze, Infinite Feedback, Ducking, Tempo-Sync, Randomizer und 45 Werkspresets.

## Schnellstart
Plugin als Effekt in einen Mixer-Slot laden. Oben Kategorie und Preset wählen (`<` `>` blättern durch alle 45), oder *Randomize*. Links Delay, rechts Reverb. *Routing* bestimmt die Reihenfolge. *FREEZE DELAY / FREEZE REVERB* frieren den aktuellen Inhalt ein.

## Was es kann
- **Delay-Typen:** Digital (sauber), Ping Pong (wechselt zwischen L und R), Stereo (L/R-Offset), Multitap (4 Taps bei ¼, ½, ¾, 1 × Zeit), Tape (Wow/Flutter, Sättigung, Höhen-Dämpfung, Zeitänderung mit Tonhöhengleiten), Reverse (rückwärts gespielte Echos), Pitch (Tonhöhenverschiebung in der Rückkopplung, −24…+24 Halbtöne), Modulated (Zeit-LFO), Filtered (resonanter Bandpass, per LFO bewegt).
- **Tempo-Sync:** folgt dem Host-Tempo (FL); 18 Notenwerte (1/1 bis 1/32, jeweils normal, punktiert, Triole). Die Anzeige oben zeigt die aufgelöste Zeit in ms. Sync aus = freie Zeit 1–4000 ms.
- **Delay-Regler:** Feedback, Mix, Width (M/S), Pan, Low Cut, High Cut, Modulation, L/R-Offset, Ducking, Pitch (nur beim Typ *Pitch*), Infinite Feedback.
- **Reverb-Modi:** Room, Hall, Plate, Shimmer (Oktave aufwärts in der Rückkopplung), Reverse (Anschwellen), Modulated, Infinite. Alle laufen auf derselben Architektur: Vorverzögerung → 4 Allpass-Diffusoren → **8-Leitungs-Feedback-Delay-Network** mit Hadamard-Mischmatrix, Dämpfung je Leitung, modulierten Leitungslängen und (bei Room) frühen Reflexionen. Die Modi unterscheiden sich in Leitungslängen, Dämpfung, Modulation und Diffusion – nicht nur im Regler.
- **Reverb-Regler:** Decay (0,1–30 s), Pre-Delay, Size, Damping, Diffusion, Width, Modulation, Low/High Cut, Mix, Ducking, Shimmer (nur Modus *Shimmer*).
- **Routing:** Delay > Reverb, Reverb > Delay, Parallel, Delay only, Reverb only.
- **Freeze** (je Delay und Reverb): Eingang wird abgeschaltet, Inhalt bleibt stehen. **Infinite Feedback** (Delay) und Reverb-Modus *Infinite* halten ebenfalls. Alle Rückkopplungen laufen durch einen weichen Begrenzer, damit der Pegel beschränkt bleibt.
- **Presets:** 45 Werkspresets in 10 Kategorien (Clean Mixing, Ambient, Atmospheric, Dub, Techno, Jungle, DnB, Experimental, Huge Spaces, Creative FX), außerdem eigene Presets speichern/laden/löschen (Dateien unter Dokumente/NEOWAVE_AUDIO/SPACE-FX/Presets).
- **Randomize:** musikalische, begrenzte Bereiche (z. B. Feedback 15–70 %, Delay-Mix 15–45 %, Decay 0,8–9 s). Freeze und Infinite bleiben aus, der Ausgangspegel bleibt unverändert. Pro Zufallszahl reproduzierbar (im Test per Seed).

## Technische Eckdaten
- State-ID `nwa.spacefx`, State-Version 1, Plugin-Code `Spf1`. Stereo und Mono (Ein-/Ausgang gleich). Keine Latenz.
- DSP ist reines C++ ohne JUCE (`Source/dsp`), keine Speicherallokation im Audio-Thread, Parameteränderungen werden geglättet. Maximale Delay-Zeit 4 s (intern bis 6,5 s Puffer).
- Tail-Länge für den Host: aus Decay bzw. Delay-Wiederholungen berechnet, maximal 30 s.

## Getestet (automatisch, Linux, GCC 13)
- `space_dsp`: Echo-Zeitpunkt bei 44,1/48/96 kHz auf den Sample genau, Feedback-Verhältnis, Ping-Pong-Abwechslung, Multitap-Positionen, Pitch ±12 Halbtöne (Frequenzmessung), Reverse, Ducking, Cuts, **RT60 je Reverb-Modus gemessen** (≈ 1,8–1,87 s bei 2,0 s Ziel), Decay/Damping/Size monoton, Pre-Delay, Modi messbar verschieden (Hüllkurve bzw. Spektralschwerpunkt, Shimmer-Oktavenergie, Modulations-Seitenbänder), **Freeze und Infinite über 40–60 s beschränkt** (Spitze ≤ 1,0001), Stabilität aller Typen/Modi bei Extrem-Einstellungen, Routing ohne Klicks beim Umschalten, Rechenlast (Pitch-Delay + Shimmer parallel ≈ 2 % eines Kerns bei 48 kHz).
- `space_processor`: alle Parameter vorhanden, **Tempo-Sync mit Host-Tempo** (90/120/140 BPM, Echo auf ±4 Samples), freie Zeit, 45 Presets (≥ 40, 10 Kategorien mit je ≥ 4, alle Parameter gültig, paarweise messbar verschieden, endlich und begrenzt gerendert, kein Rest des vorherigen Presets), Randomizer (120 Seeds in Grenzen, Freeze/Infinite aus, reproduzierbar, Vielfalt), State-Round-Trip, kaputter State, Benutzerpresets, Mono, Blockgrößen 1…2048 und Samplerates 22–192 kHz, 800 Fuzz-Runden mit Zufallsparametern. Läuft auch mit 512 KB Stack.
- `space_gui`: Editor offscreen gerendert, Button-Checks (Preset vor/zurück, Randomize, beide Freeze, Tempo sync, Infinite, Reset).

## NICHT geprüft / bekannte Grenzen – bitte lesen
- **Klang nie gehört.** Delay- und Reverb-Qualität sind nur über messbare Eigenschaften abgesichert (Zeit, Nachhallzeit, Spektrum, Stabilität). Ob es gut klingt, ob die 45 Presets nach ihrem Namen klingen, kann ich nicht beurteilen. Werte der Presets sind per Messung auf „verschieden und sinnvoll beschränkt“ geprüft, nicht per Gehör abgemischt.
- **Kein Windows-Build, nicht in FL Studio getestet, nicht mit pluginval validiert.** Siehe `docs/FL_STUDIO_TESTLISTE_SPACE.md`.
- **GUI nur per Rendering und Button-Auslösung geprüft**, nicht mit echter Maus und nicht in FL. Die Eingabe-Dialoge zum Speichern/Löschen von Presets sind ungetestet.
- **Pitch-Delay und Shimmer** nutzen einen einfachen Zwei-Köpfe-Pitchshifter mit hörbaren Artefakten (Modulation/„Flattern“), besonders bei großen Intervallen und langen Rückkopplungen.
- **Reverse-Reverb** arbeitet phasen-/blockweise (Länge folgt dem Decay, begrenzt auf 0,3–4 s) und ist **nicht** auf Transienten synchronisiert; das Ende ist abrupt.
- **Delay-Zeit ändern** erzeugt Tonhöhengleiten (beim Tape-Typ bewusst langsamer, 0,25 s).
- **Routing wechseln** blendet 8 ms aus/ein und löscht dann die unbenutzte Engine (kurze Lücke, kein Klick – gemessen, nicht gehört).
- **Freeze / Infinite** halten durch weiche Begrenzung des Rückkopplungspegels; bei sehr hohem Eingangspegel kann der gehaltene Klang dadurch gesättigt klingen. Die Ausgänge der Effektanteile sind auf ±1 weich begrenzt.
- **Ducking** ist ein einfacher Hüllkurvenfolger (keine externe Sidechain).
- Nicht in dieser Phase: Sidechain-Eingang, Oversampling, Freeze-Aufnahme per Tempo-Raster, A/B-Vergleich.
