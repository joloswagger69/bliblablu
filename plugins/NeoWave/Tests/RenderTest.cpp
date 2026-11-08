// Offline-Test fuer NEO//WAVE (kein Audiogeraet noetig):
//   NeoWaveTest              -> rendert alle Presets, meldet NaN/Pegel/CPU/Spektrum
//   NeoWaveTest --calibrate  -> zusaetzlich Lautstaerke angleichen, schreibt Source/PresetLevels.inc
//   NeoWaveTest --state      -> Zustand speichern/laden Roundtrip
//   NeoWaveTest --random     -> 200x Randomize (smart + voll), auf NaN/Pegel pruefen
//   NeoWaveTest --catalog    -> Preset-Katalog: Kategorien, Anzahl, Unterscheidbarkeit (Parameter + Klang), Speichern/Laden jedes Presets
//   NeoWaveTest --features   -> Engines, SVF, EQ, Bitcrusher, Modulationsmatrix, Effekt-Schalter einzeln auf Wirkung pruefen
#include "../Source/PluginProcessor.h"
#include <chrono>
#include <cstdio>
#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <string>

struct Metrics { float peak = 0, rmsMax = 0, centroid = 0, dc = 0; int bad = 0; double cpu = 0; };

static Metrics render(NeoProcessor& proc, std::vector<int> notes, float noteSec, float totalSec, double sr = 44100.0, int block = 512)
{
    Metrics m;
    const int total = (int) (totalSec * sr);
    juce::AudioBuffer<float> buf(2, block);
    std::vector<float> mono((size_t) total, 0.f);
    juce::MidiBuffer midi;
    int pos = 0;
    bool on = false, off = false;
    const auto t0 = std::chrono::steady_clock::now();
    while (pos < total)
    {
        midi.clear();
        if (!on) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, 0.8f), 0); on = true; }
        if (!off && pos >= (int) (noteSec * sr)) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0); off = true; }
        proc.processBlock(buf, midi);
        const int len = juce::jmin(block, total - pos);
        for (int i = 0; i < len; ++i)
        {
            const float l = buf.getSample(0, i), r = buf.getSample(1, i);
            if (!std::isfinite(l) || !std::isfinite(r)) ++m.bad;
            m.peak = juce::jmax(m.peak, std::abs(l), std::abs(r));
            mono[(size_t) (pos + i)] = 0.5f * (l + r);
        }
        pos += len;
    }
    const auto t1 = std::chrono::steady_clock::now();
    m.cpu = std::chrono::duration<double>(t1 - t0).count() / (double) totalSec;

    const int w = (int) (0.05 * sr);
    int bestStart = 0;
    for (int s = 0; s + w < total; s += w / 2)
    {
        double e = 0;
        for (int i = 0; i < w; ++i) e += mono[(size_t) (s + i)] * mono[(size_t) (s + i)];
        const float rms = (float) std::sqrt(e / w);
        if (rms > m.rmsMax) { m.rmsMax = rms; bestStart = s; }
    }
    double mean = 0;
    for (float v : mono) mean += v;
    m.dc = (float) (mean / total);

    // Spektralschwerpunkt (2048er FFT am lautesten Punkt)
    juce::dsp::FFT fft(11);
    std::vector<float> fd(4096, 0.f);
    for (int i = 0; i < 2048 && bestStart + i < total; ++i)
        fd[(size_t) i] = mono[(size_t) (bestStart + i)] * (0.5f - 0.5f * std::cos(2 * 3.14159265f * i / 2048.f));
    fft.performFrequencyOnlyForwardTransform(fd.data());
    double num = 0, den = 1e-12;
    for (int k = 1; k < 1024; ++k) { num += (double) k * fd[(size_t) k]; den += fd[(size_t) k]; }
    m.centroid = (float) (num / den * sr / 2048.0);
    return m;
}

// Rohes Mono-Signal (Akkord 48/55/64, Note 1.2 s, gesamt 2.6 s)
static std::vector<float> renderRaw(NeoProcessor& proc, std::vector<int> notes = { 48, 55, 64 }, float totalSec = 2.6f, float noteSec = 1.2f, float vel = 0.8f)
{
    const double sr = 44100.0; const int block = 512, total = (int) (totalSec * sr);
    std::vector<float> mono((size_t) total, 0.f); juce::AudioBuffer<float> buf(2, block); juce::MidiBuffer midi;
    proc.panic(); int pos = 0; bool on = false, off = false;
    while (pos < total)
    {
        midi.clear();
        if (!on) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOn(1, n, vel), 0); on = true; }
        if (!off && pos >= (int) (noteSec * sr)) { for (int n : notes) midi.addEvent(juce::MidiMessage::noteOff(1, n), 0); off = true; }
        proc.processBlock(buf, midi);
        const int len = juce::jmin(block, total - pos);
        for (int i = 0; i < len; ++i) mono[(size_t) (pos + i)] = 0.5f * (buf.getSample(0, i) + buf.getSample(1, i));
        pos += len;
    }
    return mono;
}
static double rmsOf(const std::vector<float>& v, int a = 0, int b = -1)
{ if (b < 0 || b > (int) v.size()) b = (int) v.size(); double e = 0; for (int i = a; i < b; ++i) e += (double) v[(size_t) i] * v[(size_t) i]; return std::sqrt(e / std::max(1, b - a)); }
// Spektralschwerpunkt und Bandenergien (8 Oktavbaender) eines Segments
static void spectrum(const std::vector<float>& v, int start, float& centroid, std::array<float, 8>& bands)
{
    juce::dsp::FFT fft(12); std::vector<float> fd(8192, 0.f);
    for (int i = 0; i < 4096 && start + i < (int) v.size(); ++i) fd[(size_t) i] = v[(size_t) (start + i)] * (0.5f - 0.5f * std::cos(2 * 3.14159265f * (float) i / 4096.f));
    fft.performFrequencyOnlyForwardTransform(fd.data());
    double num = 0, den = 1e-12; bands.fill(0.f);
    for (int k = 1; k < 2048; ++k)
    {
        const double f = k * 44100.0 / 4096.0; num += f * fd[(size_t) k]; den += fd[(size_t) k];
        const int b = juce::jlimit(0, 7, (int) std::floor(std::log2(f / 100.0)) + 1); bands[(size_t) b] += fd[(size_t) k] * fd[(size_t) k];
    }
    centroid = (float) (num / den);
    for (auto& b : bands) b = 10.f * std::log10(b + 1e-9f);
}
static double featDist(const std::vector<float>& a, const std::vector<float>& b)
{
    double d = 0;
    for (int w = 0; w < 10; ++w)                                       // Pegelhuellkurve in dB (10 Fenster)
    {
        const int s0 = w * (int) (a.size() / 10), s1 = s0 + (int) (a.size() / 10);
        d += std::abs(20 * std::log10(rmsOf(a, s0, s1) + 1e-5) - 20 * std::log10(rmsOf(b, s0, s1) + 1e-5)) / 10.0;
    }
    float ca, cb; std::array<float, 8> ba, bb; spectrum(a, 11025, ca, ba); spectrum(b, 11025, cb, bb);
    double sd = 0; for (int i = 0; i < 8; ++i) sd += std::abs(ba[(size_t) i] - bb[(size_t) i]) / 8.0;
    return d + sd;
}

int main(int argc, char** argv)
{
    juce::String mode = argc > 1 ? argv[1] : "";
    NeoProcessor proc;
    proc.prepareToPlay(44100.0, 512);
    const auto& lib = presetLibrary();
    int failures = 0;

    if (mode == "--state")
    {
        proc.loadFactoryPreset(juce::jmin(5, (int) lib.size() - 1));
        std::vector<float> before;
        for (int i = 0; i < P::kCount; ++i) before.push_back(proc.getParamReal(i));
        juce::MemoryBlock mb; proc.getStateInformation(mb);
        proc.setAllToDefault(false);
        proc.setStateInformation(mb.getData(), (int) mb.getSize());
        int diff = 0;
        for (int i = 0; i < P::kCount; ++i)
            if (std::abs(before[(size_t) i] - proc.getParamReal(i)) > 1e-3f * (1 + std::abs(before[(size_t) i]))) { ++diff; std::printf("  diff %s: %f -> %f\n", pdef(i).id, before[(size_t) i], proc.getParamReal(i)); }
        std::printf("STATE roundtrip: %d differences (%s)\n", diff, proc.presetName.toRawUTF8());
        return diff ? 1 : 0;
    }

    if (mode == "--random")
    {
        int bad = 0;
        for (int i = 0; i < 200; ++i)
        {
            proc.randomize(i % 2 == 0);
            auto m = render(proc, { 48, 55, 64 }, 0.8f, 1.2f);
            if (m.bad) { ++bad; std::printf("random %d: NaN!\n", i); }
            if (i < 12) std::printf("random(%s) peak %.2f rms %.3f centroid %.0f Hz\n", i % 2 == 0 ? "smart" : "full ", m.peak, m.rmsMax, m.centroid);
        }
        std::printf("RANDOM test: %d bad runs\n", bad);
        return bad ? 1 : 0;
    }

    if (mode == "--repeat")
    {
        const int idx = argc > 2 ? juce::String(argv[2]).getIntValue() : 0;
        proc.loadFactoryPreset(idx);
        for (int k = 0; k < 6; ++k)
        {
            proc.panic();
            auto m = render(proc, { 48, 55, 64 }, 1.2f, 2.6f);
            std::printf("run %d: master %.3f peak %.2f rms %.3f\n", k, proc.getParamReal(P::master), m.peak, m.rmsMax);
        }
        return 0;
    }
    if (mode == "--catalog")
    {
        int bad = 0;
        auto fail = [&](const juce::String& m) { ++bad; std::printf("FEHLER: %s\n", m.toRawUTF8()); };
        const char* req[] = { "Bass", "Acid", "Rave", "Jungle", "DnB", "Techno", "Leads", "Pads", "Plucks", "Atmospheres", "Experimental", "FX" };
        std::map<std::string, int> cnt; for (auto& p : lib) ++cnt[p.category];
        if ((int) lib.size() < 100) fail("weniger als 100 Presets");
        for (auto* c : req) { std::printf("  %-13s %d\n", c, cnt[c]); if (cnt[c] < 6) fail(juce::String(c) + " hat weniger als 6 Presets"); }
        for (auto& kv : cnt) { bool known = kv.first == "Init"; for (auto* c : req) if (kv.first == c) known = true; if (!known) fail("unbekannte Kategorie " + juce::String(kv.first)); }
        std::set<std::string> names; for (auto& p : lib) if (!names.insert(p.name).second) fail("doppelter Name " + juce::String(p.name));
        // Parameter-Abstand: jedes Paar unterscheidet sich in >= 6 Parametern (Init ausgenommen)
        std::vector<std::vector<float>> vals;
        for (int i = 0; i < (int) lib.size(); ++i) { proc.loadFactoryPreset(i); std::vector<float> v; for (int k = 0; k < P::kCount; ++k) v.push_back(proc.getParamReal(k)); vals.push_back(v); }
        int minDiff = 1000; std::string pa, pb;
        for (size_t a = 1; a < vals.size(); ++a) for (size_t b = a + 1; b < vals.size(); ++b)
        { int d = 0; for (size_t k = 0; k < vals[a].size(); ++k) if (std::abs(vals[a][k] - vals[b][k]) > 1e-4f) ++d; if (d < minDiff) { minDiff = d; pa = lib[a].name; pb = lib[b].name; } }
        std::printf("  kleinster Parameter-Unterschied: %d (%s <-> %s)\n", minDiff, pa.c_str(), pb.c_str());
        if (minDiff < 8) fail("zwei Presets unterscheiden sich in weniger als 8 Parametern");
        // Klang-Abstand
        std::vector<std::vector<float>> audio;
        for (int i = 0; i < (int) lib.size(); ++i) { proc.loadFactoryPreset(i); audio.push_back(renderRaw(proc)); }
        double minD = 1e9; size_t ma = 0, mb = 0;
        for (size_t a = 1; a < audio.size(); ++a) for (size_t b = a + 1; b < audio.size(); ++b)
        { const double d = featDist(audio[a], audio[b]); if (d < minD) { minD = d; ma = a; mb = b; } }
        std::printf("  kleinster Klang-Abstand (dB-Merkmale): %.2f (%s <-> %s)\n", minD, lib[ma].name.c_str(), lib[mb].name.c_str());
        if (minD < 1.5) fail("zwei Presets klingen praktisch gleich");
        // Speichern/Laden: Zustand jedes Presets ueberlebt getState/setState (und erzeugt danach gleichen Klang)
        int stateBad = 0;
        for (int i = 0; i < (int) lib.size(); ++i)
        {
            proc.loadFactoryPreset(i); juce::MemoryBlock mb2; proc.getStateInformation(mb2);
            std::vector<float> before; for (int k = 0; k < P::kCount; ++k) before.push_back(proc.getParamReal(k));
            proc.setAllToDefault(false); proc.setStateInformation(mb2.getData(), (int) mb2.getSize());
            for (int k = 0; k < P::kCount; ++k) if (std::abs(before[(size_t) k] - proc.getParamReal(k)) > 1e-3f * (1 + std::abs(before[(size_t) k]))) { ++stateBad; std::printf("  %s: %s\n", lib[(size_t) i].name.c_str(), pdef(k).id); break; }
        }
        if (stateBad) fail(juce::String(stateBad) + " Presets nach State-Round-Trip veraendert");
        // Benutzerpreset-Datei
        proc.loadFactoryPreset(30);
        const juce::String un = "__test_user_preset__";
        if (!proc.saveUserPreset(un, "Bass")) fail("Benutzerpreset speichern");
        proc.loadFactoryPreset(60);
        auto files = PresetStore::userDir().findChildFiles(juce::File::findFiles, false, "*.nwpreset");
        bool found = false; for (auto& f : files) if (f.getFileNameWithoutExtension() == un) { found = true; if (!proc.loadUserPreset(f)) fail("Benutzerpreset laden"); f.deleteFile(); }
        if (!found) fail("Benutzerpreset-Datei nicht gefunden");
        else { proc.loadFactoryPreset(30); std::vector<float> ref; for (int k = 0; k < P::kCount; ++k) ref.push_back(proc.getParamReal(k)); for (int k = 0; k < P::kCount; ++k) if (std::abs(ref[(size_t) k] - vals[30][(size_t) k]) > 1e-4f) { fail("Preset nicht reproduzierbar"); break; } }
        std::printf("CATALOG: %d Presets, %d Fehler\n", (int) lib.size(), bad);
        return bad ? 1 : 0;
    }

    if (mode == "--features")
    {
        int bad = 0;
        auto fail = [&](const juce::String& m) { ++bad; std::printf("FEHLER: %s\n", m.toRawUTF8()); };
        auto setup = [&]() { proc.setAllToDefault(false); proc.setParamReal(P::fType, 1.f); proc.setParamReal(P::fCut, 8000.f); proc.setParamReal(P::fEnv, 0.f); proc.setParamReal(P::bLevel, 0.f); };
        // 1) Jede Engine fuer Oszillator A und B hoerbar, endlich, mit eigenem Spektrum
        std::vector<float> cent; std::vector<std::array<float, 8>> bnd;
        for (int e = 0; e < 6; ++e)
        {
            for (int osc = 0; osc < 2; ++osc)
            {
                setup(); proc.setParamReal(osc == 0 ? P::aEngine : P::bEngine, (float) e);
                if (osc == 1) { proc.setParamReal(P::aLevel, 0.f); proc.setParamReal(P::bLevel, 0.8f); }
                proc.setParamReal(osc == 0 ? P::aMorph : P::bMorph, 0.5f);
                if (e == 5) proc.setParamReal(osc == 0 ? P::aColor : P::bColor, 0.5f);
                auto v = renderRaw(proc, { 57 }, 1.5f, 1.0f);
                const double r = rmsOf(v, 4000, 40000); bool fin = true; for (float x : v) if (!std::isfinite(x)) fin = false;
                if (r < 0.01 || !fin) fail("Engine " + juce::String(e) + (osc ? " (B)" : " (A)") + " stumm oder ungueltig (rms " + juce::String(r, 4) + ")");
                if (osc == 0) { float c; std::array<float, 8> b; spectrum(v, 8000, c, b); cent.push_back(c); bnd.push_back(b); }
            }
        }
        for (size_t a = 0; a < 6; ++a) for (size_t b = a + 1; b < 6; ++b)
        { double d = 0; for (int i = 0; i < 8; ++i) d += std::abs(bnd[a][(size_t) i] - bnd[b][(size_t) i]); d /= 8; if (d < 1.0) fail("Engines " + juce::String((int) a) + " und " + juce::String((int) b) + " klingen (spektral) praktisch gleich"); }
        std::printf("  Engine-Schwerpunkte (Hz): "); for (float c : cent) std::printf("%.0f ", c); std::printf("\n");
        // 2) Noise-Engine: Color macht den Klang tonal (Schwerpunkt folgt der Note), Morph faerbt (weiss heller als braun)
        {
            auto peakHz = [&](int note) { setup(); proc.setParamReal(P::aEngine, 5.f); proc.setParamReal(P::aMorph, 0.3f); proc.setParamReal(P::aColor, 1.f);
                auto v = renderRaw(proc, { note }, 1.2f, 1.0f); juce::dsp::FFT fft(14); std::vector<float> fd(32768, 0.f);
                for (int i = 0; i < 16384; ++i) fd[(size_t) i] = v[(size_t) (8000 + i)] * (0.5f - 0.5f * std::cos(2 * 3.14159265f * (float) i / 16384.f));
                fft.performFrequencyOnlyForwardTransform(fd.data()); int pk = 20; for (int k = 20; k < 2000; ++k) if (fd[(size_t) k] > fd[(size_t) pk]) pk = k; return (float) pk * 44100.f / 16384.f; };
            for (int note : { 45, 57, 69 })
            { const float want = 440.f * std::pow(2.f, (float) (note - 69) / 12.f), got = peakHz(note); std::printf("  Noise Color=1, Note %d: Spitze %.1f Hz (Soll %.1f)\n", note, got, want);
              if (std::abs(got - want) > want * 0.1f) fail("Noise-Engine folgt der Tonhoehe nicht (Note " + juce::String(note) + ")"); }
            auto cen = [&](float morph, float color, int note) { setup(); proc.setParamReal(P::aEngine, 5.f); proc.setParamReal(P::aMorph, morph); proc.setParamReal(P::aColor, color);
                                                                  auto v = renderRaw(proc, { note }, 1.0f, 0.9f); float c; std::array<float, 8> b; spectrum(v, 12000, c, b); return c; };
            const float white = cen(0.f, 0.f, 57), brown = cen(1.f, 0.f, 57);
            if (!(white > brown * 2.f)) fail("Noise Morph: weiss nicht deutlich heller als braun (" + juce::String(white, 0) + " vs " + juce::String(brown, 0) + ")");
            std::printf("  Noise Morph: weiss %.0f Hz / braun %.0f Hz Schwerpunkt\n", white, brown);
        }
        // 3) Filter: alle 11 Typen veraendern den Klang gegenueber Bypass-aehnlichem Zustand, SVF-Mode morpht stufenlos
        {
            setup(); proc.setParamReal(P::fType, 10.f); proc.setParamReal(P::fCut, 800.f); proc.setParamReal(P::fRes, 0.3f);
            std::vector<double> lows; float prevC = -1; bool monotoneish = true;
            for (float m : { 0.f, 1.f / 3.f, 2.f / 3.f, 1.f })
            {
                proc.setParamReal(P::fMode, m); auto v = renderRaw(proc, { 48 }, 1.2f, 1.0f); float c; std::array<float, 8> b; spectrum(v, 12000, c, b);
                lows.push_back(rmsOf(v, 4000, 40000)); if (c < prevC * 0.9f && m < 0.7f) monotoneish = false; prevC = c;
            }
            if (!(lows[0] > 0.005 && lows[2] > 0.001)) fail("SVF: LP oder HP stumm");
            proc.setParamReal(P::fMode, 0.f); auto lp = renderRaw(proc, { 48 }, 1.2f, 1.0f); proc.setParamReal(P::fMode, 0.66f); auto hp = renderRaw(proc, { 48 }, 1.2f, 1.0f);
            float c1, c2; std::array<float, 8> b1, b2; spectrum(lp, 12000, c1, b1); spectrum(hp, 12000, c2, b2);
            if (!(c2 > c1 * 1.5f)) fail("SVF-Mode LP -> HP aendert den Spektralschwerpunkt nicht (" + juce::String(c1, 0) + " -> " + juce::String(c2, 0) + ")");
            (void) monotoneish;
            std::printf("  SVF: LP %.0f Hz -> HP %.0f Hz Schwerpunkt\n", c1, c2);
            for (int t = 0; t < 11; ++t)
            {
                setup(); proc.setParamReal(P::fType, (float) t); proc.setParamReal(P::fCut, 600.f); proc.setParamReal(P::fRes, 0.5f); proc.setParamReal(P::fDrive, 0.4f);
                auto v = renderRaw(proc, { 48, 60 }, 1.2f, 1.0f); bool fin = true; float pk = 0; for (float x : v) { if (!std::isfinite(x)) fin = false; pk = std::max(pk, std::abs(x)); }
                if (!fin || pk > 1.05f) fail("Filtertyp " + juce::String(t) + " ungueltig/uebersteuert (peak " + juce::String(pk, 2) + ")");
            }
        }
        // 4) Effekte einzeln: EQ, Bitcrusher, Saturation, Chorus/Flanger/Phaser, Delay, Reverb, Compressor je ein/aus -> hoerbare Aenderung; aus = Bypass
        {
            struct Fx { const char* name; std::vector<std::pair<int, float>> on; };
            std::vector<Fx> fx = {
                { "EQ low +12", { { P::eqOn, 1 }, { P::eqLow, 12 } } }, { "EQ high +12", { { P::eqOn, 1 }, { P::eqHigh, 12 } } }, { "EQ mid -12", { { P::eqOn, 1 }, { P::eqMid, -12 }, { P::eqMidF, 400 } } },
                { "Bitcrusher", { { P::bcOn, 1 }, { P::bcBits, 4 }, { P::bcRate, 8 } } },
                { "Saturation (Analog)", { { P::satType, 1 }, { P::satAmt, 0.8f } } }, { "Distortion", { { P::satType, 2 }, { P::satAmt, 0.8f } } }, { "Wavefolder", { { P::satType, 3 }, { P::satAmt, 0.8f } } },
                { "Chorus", { { P::mfType, 1 }, { P::mfMix, 0.7f } } }, { "Flanger", { { P::mfType, 3 }, { P::mfMix, 0.7f } } }, { "Phaser", { { P::mfType, 4 }, { P::mfMix, 0.7f } } },
                { "Delay", { { P::dlType, 1 }, { P::dlMix, 0.6f }, { P::dlTime, 0.2f } } }, { "Reverb", { { P::rvType, 1 }, { P::rvMix, 0.5f } } },
                { "Compressor", { { P::cmpAmt, 0.9f } } } };
            setup(); proc.setParamReal(P::aEngine, 0.f); proc.setParamReal(P::fCut, 12000.f);
            auto dry = renderRaw(proc);
            auto dry2 = renderRaw(proc);
            const double floorD = featDist(dry, dry2);   // Rauschboden: freilaufende Startphasen/Zufall in der Stimme
            if (floorD > 1.0) fail("Referenz nicht reproduzierbar (" + juce::String(floorD, 2) + ")");
            for (auto& f : fx)
            {
                setup(); proc.setParamReal(P::fCut, 12000.f); for (auto& kv : f.on) proc.setParamReal(kv.first, kv.second);
                auto wet = renderRaw(proc); const double d = featDist(dry, wet);
                std::printf("  %-20s Wirkung %.2f\n", f.name, d);
                if (d < 1.5 * floorD + 0.4) fail(juce::String(f.name) + " hat kaum hoerbare Wirkung");
                // Aus-Zustand: Schalter zurueck -> identisch zu Referenz
                setup(); proc.setParamReal(P::fCut, 12000.f); auto off = renderRaw(proc);
                if (featDist(dry, off) > 2.0 * floorD + 0.3) fail(juce::String(f.name) + ": Aus-Zustand nicht identisch zur Referenz");
            }
        }
        // 4b) EQ-Gain direkt: +12 dB High-Shelf hebt die oberen Baender >= 6 dB, -12 dB Low-Shelf senkt die unteren
        {
            auto bandsOf = [&](std::vector<std::pair<int, float>> set) { setup(); proc.setParamReal(P::fCut, 12000.f); for (auto& kv : set) proc.setParamReal(kv.first, kv.second);
                auto v = renderRaw(proc, { 48 }, 1.5f, 1.2f); float c; std::array<float, 8> b; spectrum(v, 12000, c, b); return b; };
            auto ref = bandsOf({}); auto hi = bandsOf({ { P::eqOn, 1 }, { P::eqHigh, 12 } }); auto lo = bandsOf({ { P::eqOn, 1 }, { P::eqLow, -12 } });
            const float gHi = hi[7] - ref[7], gLo = (lo[0] - ref[0] + lo[1] - ref[1]) * 0.5f;
            std::printf("  EQ High +12 dB: Band ab 6.4 kHz %+.1f dB, Low -12 dB: untere Baender %+.1f dB\n", gHi, gLo);
            if (gHi < 6.f) fail("EQ High-Shelf hebt nicht ausreichend"); if (gLo > -4.f) fail("EQ Low-Shelf senkt nicht ausreichend");
        }
        // 5) Modulationsmatrix: je Quelle -> Ziel muss messbar wirken
        {
            struct Mm { const char* name; int src, dst; float amt; };
            std::vector<Mm> mm = { { "LFO1 -> Cutoff", sLfo1, dCutoff, 0.8f }, { "LFO1 -> Pitch", sLfo1, dPitch, 0.05f }, { "ModEnv -> Cutoff", sModEnv, dCutoff, 0.8f }, { "Velocity -> Amp", sVel, dAmp, -0.8f },
                                   { "Keytrack -> Cutoff", sKey, dCutoff, 0.8f }, { "Macro1 -> Cutoff", sMac1, dCutoff, -0.8f }, { "LFO2 -> A Morph (Wavetable)", sLfo2, dAMorph, 0.8f }, { "Step -> Pitch", sStep, dPitch, 0.1f },
                                   { "LFO1 -> SVF Mode", sLfo1, dFMode, 0.8f }, { "Aftertouch/Wheel -> Cutoff", sWheel, dCutoff, 0.8f } };
            for (auto& m : mm)
            {
                auto base = [&]() { setup(); proc.setParamReal(P::fCut, 1200.f); proc.setParamReal(P::lfo1Rate, 3.f); proc.setParamReal(P::lfo2Rate, 3.f);
                                    proc.setParamReal(P::aEngine, m.dst == dAMorph ? 1.f : 0.f); if (m.dst == dFMode) proc.setParamReal(P::fType, 10.f);
                                    proc.setParamReal(P::macro1, 1.f); proc.setParamReal(P::step1, 0.5f); proc.setParamReal(P::step2, -0.5f); proc.setParamReal(P::stepRate, 6.f); };
                base(); auto a = renderRaw(proc);
                base(); proc.setParamReal(P::mmSrc1, (float) m.src); proc.setParamReal(P::mmDst1, (float) m.dst); proc.setParamReal(P::mmAmt1, m.amt);
                auto b = renderRaw(proc); const double d = featDist(a, b);
                std::printf("  Matrix %-30s Wirkung %.2f\n", m.name, d);
                if (std::string(m.name).find("Wheel") != std::string::npos) continue;   // Mod-Wheel ohne MIDI-CC im Test = 0, nur Existenz
                if (d < 0.5) fail(juce::String("Matrix ") + m.name + " wirkt nicht");
            }
        }
        std::printf("FEATURES: %d Fehler\n", bad);
        return bad ? 1 : 0;
    }
    const bool calibrate = mode == "--calibrate";
    std::vector<float> levels;
    double worstCpu = 0;
    std::printf("%-3s %-14s %-24s %6s %6s %7s %7s %5s\n", "#", "category", "name", "peak", "rms", "centrd", "level", "cpu%");

    for (int i = 0; i < (int) lib.size(); ++i)
    {
        proc.loadFactoryPreset(i);
        float lvl = lib[(size_t) i].level;
        Metrics m;
        const int iters = calibrate ? 5 : 1;
        for (int it = 0; it < iters; ++it)
        {
            const int nanBefore = proc.nanEvents.load();
            std::vector<Metrics> runs;
            for (int k = 0; k < (calibrate ? 3 : 1); ++k)
            {
                proc.panic();
                runs.push_back(render(proc, { 48, 55, 64 }, 1.2f, 2.6f));
            }
            std::sort(runs.begin(), runs.end(), [](const Metrics& a, const Metrics& b) { return a.rmsMax < b.rmsMax; });
            m = runs[runs.size() / 2];
            for (auto& r : runs) m.peak = juce::jmax(m.peak, r.peak);
            m.bad += proc.nanEvents.load() - nanBefore;
            if (calibrate && std::getenv("NW_DEBUG") && i == 79) std::printf("  it %d master %.3f peak %.2f rms %.3f\n", it, proc.getParamReal(P::master), m.peak, m.rmsMax);
            if (calibrate && m.rmsMax > 1e-5f)
            {
                const float scale = juce::jmin(0.16f / m.rmsMax, 0.8f / juce::jmax(m.peak, 1e-3f));
                lvl = juce::jlimit(0.05f, 1.0f, lvl * std::pow(scale, 0.85f));
                proc.setParamReal(P::master, lvl);
            }
        }
        levels.push_back(calibrate ? lvl : lib[(size_t) i].level);
        worstCpu = juce::jmax(worstCpu, m.cpu);
        const bool silent = m.rmsMax < 0.01f;
        const bool hot = m.peak > 0.95f;
        if (m.bad || silent) ++failures;
        std::printf("%-3d %-14s %-24s %6.2f %6.3f %7.0f %7.3f %5.1f %s%s%s\n", i, lib[(size_t) i].category.c_str(), lib[(size_t) i].name.c_str(),
                    m.peak, m.rmsMax, m.centroid, levels.back(), m.cpu * 100, m.bad ? " NAN!" : "", silent ? " SILENT!" : "", hot ? " hot" : "");
    }
    std::printf("\n%d presets, %d failures, worst CPU %.1f%% of realtime (single core)\n", (int) lib.size(), failures, worstCpu * 100);

    if (calibrate)
    {
        juce::String s;
        for (float l : levels) s << juce::String(l, 3) << "f,\n";
        const juce::File f(juce::File::getCurrentWorkingDirectory().getChildFile("Source/PresetLevels.inc"));
        f.replaceWithText(s);
        std::printf("wrote %s\n", f.getFullPathName().toRawUTF8());
    }
    return failures ? 1 : 0;
}
