# FL-Studio-Testliste – SPACE//FX (Phase 4)

Nicht von mir geprüft (kein Windows/FL). Bitte der Reihe nach abhaken, Abweichungen mit Schritt-Nummer melden. Zum Testen am besten einen Mixer-Slot mit einem rhythmischen Signal (Drum-Loop, Hi-Hats) und einem Pad/Synth nutzen.

## Laden und Grundlagen
- [ ] Plugin als Effekt laden, Editor öffnet (Delay links, Reverb rechts, Kopfbereich mit Presets). Schließen/öffnen, Plugin entfernen: kein Absturz.
- [ ] Mit Signal: Pegel-Anzeigen IN/OUT bewegen sich. Ausgang ist hörbar, kein Knacken, kein Übersteuern.
- [ ] Projekt speichern, FL neu starten, laden: alle Einstellungen sind wieder da.
- [ ] Mono-Spur (Mixer-Slot mit Mono-Quelle) funktioniert, keine Fehlermeldung.

## Delay
- [ ] Jeden der 9 Typen durchschalten (Digital, Ping Pong, Stereo, Multitap, Tape, Reverse, Pitch, Modulated, Filtered): jeweils hörbar anders, kein Knacken beim Umschalten.
- [ ] *Tempo sync* an: Echo-Abstand passt zum FL-Tempo (Note 1/8: zwei Echos pro Schlag). Tempo in FL ändern: Delay folgt, Anzeige oben zeigt BPM und ms. Punktiert/Triole prüfen.
- [ ] *Tempo sync* aus: *Time* wirkt in ms.
- [ ] Feedback hoch: Echos wiederholen länger, schaukeln sich nicht unkontrolliert auf.
- [ ] Pitch-Typ: *Pitch* ±12 – Echos steigen/fallen in der Tonhöhe. Modulated/Filtered: *Modulation* wirkt.
- [ ] Width, Pan, L/R offset, Low cut, High cut, Ducking (Delay wird leiser, solange Eingang laut ist) wirken hörbar.
- [ ] *Infinite feedback* an: Echo bleibt stehen, Pegel bleibt begrenzt (kein Aufschaukeln). Aus: Echos klingen wieder ab.

## Reverb
- [ ] Alle 7 Modi (Room, Hall, Plate, Shimmer, Reverse, Modulated, Infinite) durchschalten: hörbar unterschiedlich.
- [ ] Decay, Pre-delay, Size, Damping, Diffusion, Width, Modulation, Low/High cut, Mix, Ducking wirken. Shimmer-Regler nur im Modus *Shimmer*.
- [ ] Modus *Infinite*: Hall klingt nicht ab, Pegel bleibt begrenzt.

## Routing, Freeze, Presets
- [ ] Alle 5 Routings umschalten (auch während Signal läuft): kein lauter Klick (kurzes Ausblenden ist bekannt).
- [ ] *FREEZE DELAY* / *FREEZE REVERB*: Inhalt bleibt stehen, auch bei Ausschalten des Eingangs; Pegel bleibt begrenzt; beim Lösen klingt es normal weiter. Mehrere Minuten halten: kein Anstieg.
- [ ] Preset-Kategorie wählen, Presets mit `<` `>` durchblättern (45 Stück): jedes lädt ohne Knacken und klingt in etwa nach seinem Namen. Auffälliges (zu laut, zu leise, kaputt) mit Preset-Namen notieren.
- [ ] *Randomize* mehrfach: musikalisch brauchbare Einstellungen, nie extrem laut, Freeze/Infinite bleiben aus. *Reset* setzt auf Standard.
- [ ] Eigenes Preset speichern, ändern, wieder laden, löschen.

## Automation und Last
- [ ] Parameter in FL automatisieren (z. B. Delay-Mix, Reverb-Mix, Feedback): sauber, ohne Knacken. Time-Automation: Tonhöhengleiten (bekannt).
- [ ] CPU-Last in FL mit mehreren Instanzen (Pitch-Delay + Shimmer-Reverb) beobachten und notieren.
- [ ] Tail: Transport stoppen, Hall/Echos klingen aus und werden nicht abgeschnitten.
- [ ] Sample-Rate 44,1 / 48 / 96 kHz in FL: Verhalten gleich, keine Aussetzer.
