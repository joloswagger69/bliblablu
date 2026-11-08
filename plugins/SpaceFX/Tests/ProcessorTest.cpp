// Prozessor-Tests fuer SPACE//FX: Parameter, Tempo-Sync mit Host-Tempo, Werkspresets (Anzahl, Kategorien, Gueltigkeit, Unterscheidbarkeit, Stabilitaet),
// Randomizer-Grenzen, State, Mono/Stereo, Blockgroessen, Fuzz. Kein Klangurteil.
#include "../Source/SpaceProcessor.h"
#include <juce_events/juce_events.h>
#include <cstdio>
#include <set>

using namespace spf;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

struct FakeHead : juce::AudioPlayHead
{
    double bpm = 120.0;
    juce::Optional<PositionInfo> getPosition() const override { PositionInfo p; p.setBpm(bpm); p.setIsPlaying(true); p.setPpqPosition(0.0); return p; }
};
struct Out { std::vector<float> l, r; };
static void prep(SpaceProcessor& p, double sr, int blk, int ch = 2) { p.setPlayConfigDetails(ch, ch, sr, blk); p.prepareToPlay(sr, blk); }
static Out run(SpaceProcessor& p, const std::vector<float>& in, int blk = 512)
{
    Out o; juce::AudioBuffer<float> b(2, blk);
    for (size_t pos = 0; pos < in.size(); pos += (size_t) blk)
    {
        const int n = (int) std::min((size_t) blk, in.size() - pos); b.setSize(2, n, false, false, true);
        for (int i = 0; i < n; ++i) { b.setSample(0, i, in[pos + (size_t) i]); b.setSample(1, i, in[pos + (size_t) i]); }
        juce::MidiBuffer m; p.processBlock(b, m);
        o.l.insert(o.l.end(), b.getReadPointer(0), b.getReadPointer(0) + n); o.r.insert(o.r.end(), b.getReadPointer(1), b.getReadPointer(1) + n);
    }
    return o;
}
static float peak(const Out& o) { float m = 0; for (size_t i = 0; i < o.l.size(); ++i) m = std::max({ m, std::abs(o.l[i]), std::abs(o.r[i]) }); return m; }
static bool finite(const Out& o) { for (size_t i = 0; i < o.l.size(); ++i) if (! std::isfinite(o.l[i]) || ! std::isfinite(o.r[i])) return false; return true; }
static double energy(const std::vector<float>& v, int a, int b) { double e = 0; for (int i = a; i < b && i < (int) v.size(); ++i) e += (double) v[(size_t) i] * v[(size_t) i]; return e; }
static std::vector<float> testSignal(int n, double sr)
{   // Klicks im Viertelraster (120 bpm) + Rauschburst + Sinus-Stueck
    std::vector<float> s((size_t) n, 0.f); sfx::Rng g(11);
    for (int k = 0; k * (int) (sr * 0.5) < n; ++k) { const int st = k * (int) (sr * 0.5); for (int i = 0; i < 200 && st + i < n; ++i) s[(size_t) (st + i)] = (g.uni() * 2.f - 1.f) * 0.6f * (1.f - (float) i / 200.f); }
    for (int i = (int) (sr * 1.0); i < (int) (sr * 1.2) && i < n; ++i) s[(size_t) i] += 0.3f * (float) std::sin(2 * 3.14159265 * 330.0 * i / sr);
    return s;
}

static int realMain()
{
    juce::ScopedJuceInitialiser_GUI init;
    // ---------------------------------------------------------------- Parameter
    {
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 512);
        CHECK(p->getParameters().size() >= 30, "Parameteranzahl %d", p->getParameters().size());
        std::set<juce::String> ids; for (auto* prm : p->getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) CHECK(ids.insert(r->getParameterID()).second, "doppelte ID");
        for (const char* id : { "route", "out", "d_type", "d_sync", "d_note", "d_time", "d_fb", "d_mix", "d_width", "d_pan", "d_lcut", "d_hcut", "d_mod", "d_off", "d_duck", "d_pitch", "d_inf", "fz_d",
                                "r_mode", "r_decay", "r_pre", "r_size", "r_damp", "r_diff", "r_width", "r_mod", "r_lcut", "r_hcut", "r_mix", "r_duck", "r_shim", "fz_r" })
            CHECK(p->apvts.getParameter(id) != nullptr, "Parameter %s fehlt", id);
        // Stille rein -> Stille raus
        std::vector<float> sil(44100, 0.f); auto o = run(*p, sil); CHECK(finite(o) && peak(o) < 1e-6f, "Stille");
        CHECK(p->getTailLengthSeconds() > 0.5, "Tail %.2f", p->getTailLengthSeconds());
    }
    // ---------------------------------------------------------------- Tempo-Sync (Host-Tempo)
    for (double bpm : { 90.0, 120.0, 140.0 })
    {
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 512); FakeHead head; head.bpm = bpm; p->setPlayHead(&head);
        p->resetAllToDefault(); p->setParam("route", 3.f); p->setParam("d_sync", 1.f); p->setParam("d_note", 6.f); p->setParam("d_fb", 0.f); p->setParam("d_mix", 1.f); p->setParam("d_mod", 0.f);
        std::vector<float> in((size_t) (44100 * 3), 0.f); in[0] = 1.f; auto o = run(*p, in);
        int pk = 0; for (int i = 1000; i < (int) in.size(); ++i) if (std::abs(o.l[(size_t) i]) > std::abs(o.l[(size_t) pk]) || pk == 0) pk = i;
        const double expect = 60.0 / bpm * 44100.0;
        CHECK(std::abs(pk - expect) < 4.0, "bpm %.0f: Echo bei %d erwartet %.1f", bpm, pk, expect);
        // 1/8 dotted & 1/4 triplet
        p->setParam("d_note", 10.f); auto p2 = std::make_unique<SpaceProcessor>(); prep(*p2, 44100.0, 512); p2->setPlayHead(&head); p2->resetAllToDefault(); p2->setParam("route", 3.f); p2->setParam("d_note", 10.f); p2->setParam("d_fb", 0.f); p2->setParam("d_mix", 1.f); p2->setParam("d_mod", 0.f);
        auto o2 = run(*p2, in); int pk2 = 1000; for (int i = 1000; i < (int) in.size(); ++i) if (std::abs(o2.l[(size_t) i]) > std::abs(o2.l[(size_t) pk2])) pk2 = i;
        CHECK(std::abs(pk2 - 0.75 * expect) < 4.0, "1/8 dotted: %d erwartet %.1f", pk2, 0.75 * expect);
        CHECK(std::abs(p2->resolvedDelayMs() - 0.75f * 60000.f / (float) bpm) < 1.f, "resolvedDelayMs");
    }
    {   // freie Zeit (Sync aus)
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 48000.0, 256); p->resetAllToDefault(); p->setParam("route", 3.f); p->setParam("d_sync", 0.f); p->setParam("d_time", 250.f); p->setParam("d_fb", 0.f); p->setParam("d_mix", 1.f); p->setParam("d_mod", 0.f);
        std::vector<float> in((size_t) 48000, 0.f); in[0] = 1.f; auto o = run(*p, in, 256); int pk = 100; for (int i = 100; i < 48000; ++i) if (std::abs(o.l[(size_t) i]) > std::abs(o.l[(size_t) pk])) pk = i;
        CHECK(std::abs(pk - 12000) < 4, "freie Zeit: %d", pk);
    }
    // ---------------------------------------------------------------- Werkspresets
    {
        const auto& fp = factoryPresets();
        CHECK(fp.size() >= 40, "nur %d Werkspresets", (int) fp.size());
        std::set<juce::String> names; std::map<juce::String, int> perCat;
        for (auto& f : fp) { CHECK(names.insert(f.name).second, "doppelter Name %s", f.name); ++perCat[f.category]; }
        for (auto* c : kCategories) CHECK(perCat[c] >= 4, "Kategorie %s hat %d", c, perCat[c]);
        CHECK(perCat.size() == 10, "Kategorien %d", (int) perCat.size());
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 512);
        for (auto& f : fp) for (auto& kv : f.v)
        {
            auto* prm = p->apvts.getParameter(kv.first); CHECK(prm != nullptr, "%s: Parameter %s existiert nicht", f.name, kv.first);
            if (! prm) continue;
            const auto r = prm->getNormalisableRange(); (void) r;
            const float n01 = prm->convertTo0to1(kv.second); const float back = prm->convertFrom0to1(n01);
            CHECK(n01 >= 0.f && n01 <= 1.f && std::abs(kv.second - back) <= 0.02f * std::max(1.f, std::abs(kv.second)) + 0.5f * (prm->getNumSteps() > 0 && prm->getNumSteps() < 100 ? 1.f : 0.f), "%s: %s=%g ausserhalb (%g)", f.name, kv.first, kv.second, back);
        }
        // Rendern: endlich, begrenzt, nicht still; paarweise unterscheidbar
        const double sr = 44100.0; const int n = (int) (sr * 6); auto sig = testSignal(n, sr); FakeHead head; p->setPlayHead(&head);
        std::vector<std::vector<double>> sigs;
        for (int i = 0; i < (int) fp.size(); ++i)
        {
            p->engineResetForTest(); p->applyFactory(i); auto o = run(*p, sig, 512);
            CHECK(finite(o), "%s: NaN", fp[(size_t) i].name); CHECK(peak(o) < 1.6f, "%s: Peak %.2f", fp[(size_t) i].name, peak(o));
            const double wet = energy(o.l, 0, n) - energy(sig, 0, n);
            CHECK(energy(o.l, 0, n) > 0.2 * energy(sig, 0, n), "%s: Ausgang viel leiser als Eingang", fp[(size_t) i].name); (void) wet;
            std::vector<double> v; const int st = (int) (sr * 0.1);
            for (int a = 0; a + st <= n; a += st) { v.push_back(10 * std::log10(energy(o.l, a, a + st) + 1e-10)); v.push_back(10 * std::log10(energy(o.r, a, a + st) + 1e-10)); }
            // Differenz zum Trocken-Signal hebt den Effektanteil hervor
            sigs.push_back(v);
        }
        double minD = 1e9; int ma = 0, mb = 0;
        for (size_t a = 0; a < sigs.size(); ++a) for (size_t b = a + 1; b < sigs.size(); ++b)
        { double d = 0; for (size_t i = 0; i < sigs[a].size(); ++i) d += std::abs(sigs[a][i] - sigs[b][i]); d /= (double) sigs[a].size(); if (d < minD) { minD = d; ma = (int) a; mb = (int) b; } }
        std::printf("Werkspresets: %d, kleinster Abstand der Pegelhuellkurven %.2f dB (%s <-> %s)\n", (int) fp.size(), minD, fp[(size_t) ma].name, fp[(size_t) mb].name);
        CHECK(minD > 0.25, "Zwei Presets klingen praktisch gleich: %s / %s (%.3f dB)", fp[(size_t) ma].name, fp[(size_t) mb].name, minD);
        // Preset B direkt == Preset A dann B (kein Rest von A)
        auto q1 = std::make_unique<SpaceProcessor>(); prep(*q1, 44100.0, 512); q1->setPlayHead(&head); q1->applyFactory(5); q1->applyFactory(20);
        auto q2 = std::make_unique<SpaceProcessor>(); prep(*q2, 44100.0, 512); q2->setPlayHead(&head); q2->applyFactory(20);
        for (auto* prm : q1->getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) CHECK(std::abs(r->getValue() - q2->apvts.getParameter(r->getParameterID())->getValue()) < 1e-4f, "Rest von Preset A in %s", r->getParameterID().toRawUTF8());
    }
    // ---------------------------------------------------------------- Randomizer
    {
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 512); FakeHead head; p->setPlayHead(&head); p->setParam("out", -3.f);
        const double sr = 44100.0; auto sig = testSignal((int) (sr * 3), sr); std::set<juce::String> sigset;
        for (unsigned s = 1; s <= 120; ++s)
        {
            p->randomize(s);
            CHECK(p->getRaw("fz_d") < 0.5f && p->getRaw("fz_r") < 0.5f && p->getRaw("d_inf") < 0.5f, "Randomizer schaltet Freeze/Infinite ein");
            CHECK(p->getRaw("d_fb") <= 0.7001f && p->getRaw("d_fb") >= 0.1499f && p->getRaw("r_decay") <= 9.001f && p->getRaw("d_mix") <= 0.4501f && p->getRaw("r_mix") <= 0.5001f && p->getRaw("r_mix") >= 0.1199f, "Randomizer ausserhalb der Grenzen (seed %u)", s);
            CHECK(std::abs(p->getRaw("out") + 3.f) < 0.01f, "Randomizer aendert Ausgangspegel");
            if (s <= 25) { p->engineResetForTest(); auto o = run(*p, sig); CHECK(finite(o) && peak(o) < 1.6f, "Randomizer-Preset %u: Peak %.2f", s, peak(o)); }
            sigset.insert(juce::String(p->getRaw("d_type")) + "/" + juce::String(p->getRaw("r_mode")) + "/" + juce::String(p->getRaw("route")) + "/" + juce::String(p->getRaw("d_fb"), 3));
        }
        CHECK(sigset.size() > 60, "Randomizer liefert zu wenig Vielfalt (%d)", (int) sigset.size());
        p->randomize(5); const float a = p->getRaw("d_fb"); p->randomize(5); CHECK(std::abs(p->getRaw("d_fb") - a) < 1e-5f, "Randomizer nicht reproduzierbar");
    }
    // ---------------------------------------------------------------- State, Presets (Dateien), Layouts
    {
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 512); p->applyFactory(8); p->setParam("d_pitch", 7.f); p->setParam("out", 2.5f);
        juce::MemoryBlock mb; p->getStateInformation(mb);
        auto q = std::make_unique<SpaceProcessor>(); prep(*q, 44100.0, 512); q->setStateInformation(mb.getData(), (int) mb.getSize());
        for (auto* prm : p->getParameters()) if (auto* r = dynamic_cast<juce::RangedAudioParameter*>(prm)) CHECK(std::abs(r->getValue() - q->apvts.getParameter(r->getParameterID())->getValue()) < 1e-5f, "State %s", r->getParameterID().toRawUTF8());
        const char junk[] = "garbage"; q->setStateInformation(junk, 7); q->setStateInformation(nullptr, 0);
        auto dir = juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("space_pm_" + juce::String(juce::Time::currentTimeMillis()));
        nwa::PresetManager pm(p->apvts, makePluginInfo().id, "SPACE-FX", makePluginInfo().stateVersion, dir);
        CHECK(pm.save("My Test") == nwa::PresetManager::Status::ok, "Preset speichern"); p->setParam("d_pitch", -3.f);
        CHECK(pm.load("My Test") == nwa::PresetManager::Status::ok && p->getInt("d_pitch") == 7, "Preset laden"); CHECK(pm.remove("My Test"), "Preset loeschen"); dir.deleteRecursively();
        // Mono
        auto m = std::make_unique<SpaceProcessor>(); prep(*m, 44100.0, 256, 1); m->applyFactory(0); juce::AudioBuffer<float> b(1, 256); float pk = 0;
        for (int i = 0; i < 100; ++i) { for (int k = 0; k < 256; ++k) b.setSample(0, k, i == 0 && k == 0 ? 1.f : 0.f); juce::MidiBuffer mm; m->processBlock(b, mm); pk = std::max(pk, b.getMagnitude(0, 256)); }
        CHECK(pk > 0.01f && pk <= 1.6f, "Mono %.3f", pk);
        juce::AudioProcessor::BusesLayout l; l.inputBuses.add(juce::AudioChannelSet::stereo()); l.outputBuses.add(juce::AudioChannelSet::stereo()); CHECK(p->isBusesLayoutSupported(l), "stereo");
        l.inputBuses.getReference(0) = juce::AudioChannelSet::mono(); CHECK(! p->isBusesLayoutSupported(l), "mono/stereo gemischt");
    }
    // ---------------------------------------------------------------- Blockgroessen + Samplerate
    {
        const double sr = 48000.0; auto sig = testSignal((int) (sr * 3), sr); FakeHead head;
        auto render = [&](int blk) { auto p = std::make_unique<SpaceProcessor>(); prep(*p, sr, 2048); p->setPlayHead(&head); p->applyFactory(3); return run(*p, sig, blk); };
        auto a = render(512);
        for (int blk : { 1, 13, 64, 2048 }) { auto b = render(blk); double d = 0, e = 0; for (size_t i = 0; i < a.l.size(); ++i) { d += std::pow(a.l[i] - b.l[i], 2); e += a.l[i] * a.l[i]; } CHECK(d < 1e-5 * e + 1e-10, "Blockgroesse %d (rel %.3g)", blk, d / (e + 1e-30)); }
        for (double r : { 22050.0, 44100.0, 88200.0, 192000.0 }) { auto p = std::make_unique<SpaceProcessor>(); prep(*p, r, 256); p->setPlayHead(&head); p->applyFactory(20); auto s2 = testSignal((int) (r * 2), r); auto o = run(*p, s2, 256); CHECK(finite(o) && peak(o) < 1.6f, "sr %.0f", r); }
    }
    // ---------------------------------------------------------------- Fuzz: zufaellige Parameter, Routing-Wechsel, Blockgroessen
    {
        auto p = std::make_unique<SpaceProcessor>(); prep(*p, 44100.0, 1024); FakeHead head; p->setPlayHead(&head); juce::Random rnd(21); juce::AudioBuffer<float> b(2, 1024); bool ok = true; float mx = 0;
        for (int it = 0; it < 800 && ok; ++it)
        {
            for (auto* prm : p->getParameters()) if (rnd.nextInt(8) == 0) prm->setValueNotifyingHost(rnd.nextFloat());
            if (it % 50 == 0) head.bpm = 40.0 + rnd.nextInt(200);
            const int n = 1 + rnd.nextInt(1024); b.setSize(2, n, false, false, true);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) b.setSample(c, i, (rnd.nextFloat() * 2.f - 1.f) * 0.8f);
            juce::MidiBuffer m; p->processBlock(b, m);
            for (int c = 0; c < 2; ++c) for (int i = 0; i < n; ++i) { const float v = b.getSample(c, i); if (! std::isfinite(v)) ok = false; mx = std::max(mx, std::abs(v)); }
        }
        CHECK(ok, "Fuzz: NaN/Inf"); CHECK(mx < 40.f, "Fuzz: Peak %.2f (Dry-Eingang 0.8 + Out +12 dB)", mx);
        std::printf("Fuzz-Spitzenpegel (zufaellige Parameter, Ausgang bis +12 dB): %.2f\n", mx);
    }
    std::printf(failures ? "\n%d FEHLER\n" : "\nALLE OK\n", failures);
    return failures ? 1 : 0;
}

int main()
{
    struct Runner : juce::Thread { int rc = 1; Runner() : juce::Thread("test", 64u * 1024u * 1024u) {} void run() override { rc = realMain(); } } r;
    r.startThread(); r.waitForThreadToExit(-1); return r.rc;
}
