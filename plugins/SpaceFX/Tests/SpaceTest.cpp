// DSP-Tests fuer SPACE//FX (Delay, Reverb, Routing). Rein numerisch, ohne JUCE. Misst Eigenschaften (Zeiten, Pegel, Stabilitaet, Spektrum),
// urteilt nicht ueber Klang.
#include "../Source/dsp/Engine.h"
#include <chrono>
#include <cstdio>
#include <memory>

using namespace sfx;
static int failures = 0;
#define CHECK(c, ...) do { if (! (c)) { ++failures; std::printf("FAIL %s:%d  %s  ", __FILE__, __LINE__, #c); std::printf(__VA_ARGS__); std::printf("\n"); } } while (0)

using Buf = std::vector<float>;
static double SR = 44100.0;
static int ms2s(double ms) { return (int) std::lround(ms * 0.001 * SR); }

struct Stereo { Buf l, r; };
static Stereo silence(int n) { return { Buf((size_t) n, 0.f), Buf((size_t) n, 0.f) }; }
static Stereo impulse(int n, float a = 1.f) { auto s = silence(n); s.l[0] = a; s.r[0] = a; return s; }
static Stereo noise(int n, float amp, unsigned seed = 3) { Rng g(seed); auto s = silence(n); for (int i = 0; i < n; ++i) { s.l[(size_t) i] = (g.uni() * 2 - 1) * amp; s.r[(size_t) i] = (g.uni() * 2 - 1) * amp; } return s; }
static Stereo sine(int n, double f, float amp) { auto s = silence(n); for (int i = 0; i < n; ++i) s.l[(size_t) i] = s.r[(size_t) i] = amp * (float) std::sin(2 * 3.14159265358979 * f * i / SR); return s; }
static float peakOf(const Buf& b, int from = 0, int to = -1) { float m = 0; if (to < 0) to = (int) b.size(); for (int i = from; i < to; ++i) m = std::max(m, std::abs(b[(size_t) i])); return m; }
static int argPeak(const Buf& b, int from, int to) { int k = from; for (int i = from; i < to; ++i) if (std::abs(b[(size_t) i]) > std::abs(b[(size_t) k])) k = i; return k; }
static double energy(const Buf& b, int from, int to) { double e = 0; for (int i = from; i < to && i < (int) b.size(); ++i) e += (double) b[(size_t) i] * b[(size_t) i]; return e; }
static bool allFinite(const Stereo& s) { for (size_t i = 0; i < s.l.size(); ++i) if (! std::isfinite(s.l[i]) || ! std::isfinite(s.r[i])) return false; return true; }
static double goertzel(const Buf& b, int from, int to, double f)
{
    const double w = 2 * 3.14159265358979 * f / SR, c = 2 * std::cos(w); double s1 = 0, s2 = 0;
    for (int i = from; i < to; ++i) { const double s = b[(size_t) i] + c * s1 - s2; s2 = s1; s1 = s; }
    return (s1 * s1 + s2 * s2 - c * s1 * s2) / std::max(1, to - from);
}

static Stereo runDelay(const Stereo& in, const DelayParams& p, int blk = 512)
{
    auto e = std::make_unique<DelayEngine>(); e->prepare(SR);
    const int n = (int) in.l.size(); Stereo o = silence(n);
    for (int s = 0; s < n; s += blk) { const int c = std::min(blk, n - s); e->process(in.l.data() + s, in.r.data() + s, o.l.data() + s, o.r.data() + s, c, p); }
    return o;
}
static Stereo runReverb(const Stereo& in, const ReverbParams& p, int blk = 512)
{
    auto e = std::make_unique<ReverbEngine>(); e->prepare(SR);
    const int n = (int) in.l.size(); Stereo o = silence(n);
    for (int s = 0; s < n; s += blk) { const int c = std::min(blk, n - s); e->process(in.l.data() + s, in.r.data() + s, o.l.data() + s, o.r.data() + s, c, p); }
    return o;
}
static DelayParams dp(int type, float ms, float fb) { DelayParams p; p.type = type; p.timeMs = ms; p.fb = fb; p.mod = 0.f; return p; }

// RT60 aus Schroeder-Rueckwaertsintegration (T20 x 3)
static double rt60(const Buf& ir)
{
    std::vector<double> e(ir.size()); double acc = 0;
    for (size_t i = ir.size(); i-- > 0;) { acc += (double) ir[i] * ir[i]; e[i] = acc; }
    const double e0 = e[0]; int a = -1, b = -1;
    for (size_t i = 0; i < e.size(); ++i) { const double db = 10 * std::log10(e[i] / e0 + 1e-30); if (a < 0 && db <= -5) a = (int) i; if (b < 0 && db <= -25) { b = (int) i; break; } }
    if (a < 0 || b < 0 || b <= a) return -1;
    return 3.0 * (b - a) / SR;
}

int main()
{
    // ---------------------------------------------------------------- Bausteine
    {
        DelayLine d; d.init(1000); for (int i = 0; i < 500; ++i) d.write((float) i);
        CHECK(std::abs(d.read(10.f) - 489.f) < 1e-3f, "DelayLine ganzzahlig %.3f", d.read(10.f));
        CHECK(std::abs(d.read(10.5f) - 488.5f) < 1e-2f, "DelayLine Interpolation %.3f", d.read(10.5f));
        CHECK(knee(0.5f) == 0.5f && knee(-0.5f) == -0.5f && knee(100.f) <= 1.0f && knee(-100.f) >= -1.0f && knee(0.95f) < 0.95f + 1e-6f, "knee");
        float prev = -2; bool mono = true; for (float x = 0; x < 5; x += 0.01f) { const float y = knee(x); if (y < prev) mono = false; prev = y; } CHECK(mono, "knee monoton");
        CHECK(noteBeats(6) == 1.f && noteBeats(9) == 0.5f && noteBeats(10) == 0.75f && std::abs(noteBeats(11) - 1.f / 3.f) < 1e-6f && noteBeats(0) == 4.f && noteBeats(3) == 2.f && noteBeats(15) == 0.125f, "Notenwerte");
        CHECK(std::abs(noteBeats(7) - 1.5f) < 1e-6f && std::abs(noteBeats(8) - 2.f / 3.f) < 1e-6f, "punktiert/Triole");
    }
    // ---------------------------------------------------------------- Delay
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        SR = sr; const int n = ms2s(3000);
        auto o = runDelay(impulse(n), dp(DtDigital, 100.f, 0.5f));
        const int e1 = argPeak(o.l, 10, ms2s(150)), e2 = argPeak(o.l, ms2s(150), ms2s(250)), e3 = argPeak(o.l, ms2s(250), ms2s(350));
        CHECK(std::abs(e1 - ms2s(100)) <= 2, "sr=%.0f Echo1 bei %d erwartet %d", sr, e1, ms2s(100));
        CHECK(std::abs(e2 - ms2s(200)) <= 3 && std::abs(e3 - ms2s(300)) <= 4, "Echo2/3 %d %d", e2, e3);
        auto ew = [&](int c) { return std::sqrt(energy(o.l, c - 12, c + 13)); };      // Energie statt Spitze: das Antialias-Filter verschmiert den Impuls
        const double a1 = ew(e1), a2 = ew(e2), a3 = ew(e3);
        CHECK(a1 > 0.5 && std::abs(a2 / a1 - 0.49) < 0.05 && std::abs(a3 / a2 - 0.49) < 0.05, "Rueckkopplung %.3f %.3f %.3f", a1, a2, a3);
        CHECK(peakOf(o.l, 0, e1 - 20) < 1e-4f, "Signal vor dem ersten Echo");
    }
    SR = 44100.0;
    {   // Ping-Pong: erstes Echo links, zweites rechts
        const int n = ms2s(1200); auto o = runDelay(impulse(n), dp(DtPingPong, 200.f, 0.7f));
        const int t = ms2s(200);
        CHECK(peakOf(o.l, t - 3, t + 3) > 0.4f && peakOf(o.r, t - 3, t + 3) < 0.02f, "PingPong Echo1 nur links (%.3f/%.3f)", peakOf(o.l, t - 3, t + 3), peakOf(o.r, t - 3, t + 3));
        CHECK(peakOf(o.r, 2 * t - 6, 2 * t + 6) > 0.2f && peakOf(o.l, 2 * t - 6, 2 * t + 6) < 0.02f, "PingPong Echo2 nur rechts");
        CHECK(peakOf(o.l, 3 * t - 8, 3 * t + 8) > 0.1f, "PingPong Echo3 links");
    }
    {   // Stereo-Offset und Stereo-Typ
        const int n = ms2s(1200); auto p = dp(DtDigital, 200.f, 0.f); p.offset = 0.5f; auto o = runDelay(impulse(n), p);
        CHECK(std::abs(argPeak(o.l, 10, n) - ms2s(200)) <= 2 && std::abs(argPeak(o.r, 10, n) - ms2s(300)) <= 3, "Offset L=%d R=%d", argPeak(o.l, 10, n), argPeak(o.r, 10, n));
        auto q = dp(DtStereo, 200.f, 0.f); auto o2 = runDelay(impulse(n), q);
        CHECK(std::abs(argPeak(o2.l, 10, n) - ms2s(200)) <= 2 && std::abs(argPeak(o2.r, 10, n) - ms2s(150)) <= 3, "Stereo-Typ L=%d R=%d", argPeak(o2.l, 10, n), argPeak(o2.r, 10, n));
    }
    {   // Multitap: 4 Taps bei 1/4, 1/2, 3/4, 1
        const int n = ms2s(1200); auto o = runDelay(impulse(n), dp(DtMultitap, 400.f, 0.f));
        for (int k = 1; k <= 4; ++k) { const int c = ms2s(100.0 * k); CHECK(peakOf(o.l, c - 3, c + 3) + peakOf(o.r, c - 3, c + 3) > 0.15f, "Tap %d fehlt", k); }
        CHECK(peakOf(o.l, ms2s(120), ms2s(180)) < 0.02f && peakOf(o.l, ms2s(420), ms2s(1100)) < 0.02f, "Signal zwischen den Taps");
    }
    {   // Pitch: Sinus 440 Hz -> Echo bei 880 Hz (+12)
        const int n = ms2s(2500); auto in = sine(ms2s(1200), 440.0, 0.5f); in.l.resize((size_t) n, 0.f); in.r.resize((size_t) n, 0.f);
        auto p = dp(DtPitch, 300.f, 0.f); p.pitch = 12.f; auto o = runDelay(in, p);
        const int a = ms2s(500), b = ms2s(1200);
        const double e880 = goertzel(o.l, a, b, 880.0), e440 = goertzel(o.l, a, b, 440.0);
        CHECK(e880 > 8.0 * e440 && e880 > 1e-4, "Pitch +12: e880=%.5f e440=%.5f", e880, e440);
        p.pitch = -12.f; auto o2 = runDelay(in, p); const double d220 = goertzel(o2.l, a, b, 220.0), d440 = goertzel(o2.l, a, b, 440.0);
        CHECK(d220 > 8.0 * d440 && d220 > 1e-4, "Pitch -12: e220=%.5f e440=%.5f", d220, d440);
    }
    {   // Reverse: Energie vorhanden, anders als Digital, endlich
        const int n = ms2s(3000); auto in = noise(ms2s(100), 0.5f); in.l.resize((size_t) n, 0.f); in.r.resize((size_t) n, 0.f);
        auto o = runDelay(in, dp(DtReverse, 400.f, 0.f)), d = runDelay(in, dp(DtDigital, 400.f, 0.f));
        CHECK(allFinite(o) && energy(o.l, 0, n) > 0.05 * energy(in.l, 0, n), "Reverse Energie");
        double diff = 0; for (int i = 0; i < n; ++i) diff += std::abs(o.l[(size_t) i] - d.l[(size_t) i]); CHECK(diff > 1.0, "Reverse = Digital?");
        // Das umgekehrte Burst-Ende kommt zuerst: Energie im Fenster vor 2*t ist hoeher als sie bei digitalem Echo vor dem Burst waere (Burst beginnt vorne)
        CHECK(energy(o.l, 0, ms2s(400)) > 0.0, "Reverse beginnt sofort mit Umkehr-Material");
    }
    {   // Typen: alle endlich, begrenzt, extreme Parameter
        for (int t = 0; t < DtCount; ++t)
        {
            auto p = dp(t, 1.f, 1.f); p.fb = 1.f; p.mod = 1.f; p.width = 2.f; p.pan = 1.f; p.offset = 0.5f; p.duck = 1.f; p.lowHz = 2000.f; p.highHz = 500.f; p.infinite = true;
            auto o = runDelay(noise(ms2s(3000), 0.9f), p); CHECK(allFinite(o) && peakOf(o.l) < 1.6f && peakOf(o.r) < 1.6f, "Typ %d extrem: Peak %.2f", t, std::max(peakOf(o.l), peakOf(o.r)));
            p.timeMs = 4000.f; p.infinite = false; p.fb = 0.98f; p.lowHz = 20.f; p.highHz = 20000.f; auto o2 = runDelay(noise(ms2s(2000), 0.9f), p); CHECK(allFinite(o2) && peakOf(o2.l) < 1.6f, "Typ %d lang", t);
        }
    }
    {   // Ducking
        const int n = ms2s(3000); auto in = noise(n, 0.5f); auto p = dp(DtDigital, 50.f, 0.5f);
        auto a = runDelay(in, p); p.duck = 1.f; auto b = runDelay(in, p);
        CHECK(energy(b.l, ms2s(500), n) < 0.2 * energy(a.l, ms2s(500), n), "Ducking wirkt nicht (%.3g vs %.3g)", energy(b.l, ms2s(500), n), energy(a.l, ms2s(500), n));
        // Nach dem Signal kommt das Echo wieder
        auto burst = silence(n); for (int i = 0; i < ms2s(300); ++i) burst.l[(size_t) i] = burst.r[(size_t) i] = 0.5f * (i % 50 < 25 ? 1.f : -1.f);
        p.timeMs = 600.f; p.fb = 0.6f; auto c = runDelay(burst, p); CHECK(energy(c.l, ms2s(900), ms2s(1400)) > 0.001, "Echo kommt nach dem Ducking nicht zurueck");
    }
    {   // Low-/High-Cut
        const int n = ms2s(1000); auto in100 = sine(n, 100, 0.5f), in8k = sine(n, 8000, 0.5f);
        auto p = dp(DtDigital, 50.f, 0.f); auto ref = runDelay(in100, p); p.lowHz = 1000.f; auto cut = runDelay(in100, p);
        CHECK(energy(cut.l, ms2s(500), n) < 0.05 * energy(ref.l, ms2s(500), n), "Low Cut 100 Hz: %.3g/%.3g", energy(cut.l, ms2s(500), n), energy(ref.l, ms2s(500), n));
        p = dp(DtDigital, 50.f, 0.f); auto ref2 = runDelay(in8k, p); p.highHz = 1000.f; auto cut2 = runDelay(in8k, p);
        CHECK(energy(cut2.l, ms2s(500), n) < 0.01 * energy(ref2.l, ms2s(500), n), "High Cut 8 kHz");
    }
    {   // Freeze / Infinite: stabil, kein Pegelanstieg, kein Zerfall
        const int fr = ms2s(1000); auto p = dp(DtDigital, 120.f, 0.5f);
        auto e = std::make_unique<DelayEngine>(); e->prepare(SR);
        Stereo o = silence(1); (void) o;
        const int total = (int) (SR * 61), blk = 512; Buf wl((size_t) blk), wr((size_t) blk), il((size_t) blk), ir((size_t) blk);
        Rng g(5); double eEarly = 0, eLate = 0; float mx = 0;
        for (int s = 0; s < total; s += blk)
        {
            for (int i = 0; i < blk; ++i) { const float v = (s + i) < fr ? (g.uni() * 2 - 1) * 0.5f : 0.f; il[(size_t) i] = ir[(size_t) i] = v; }
            p.freeze = (s >= fr + 4096);
            e->process(il.data(), ir.data(), wl.data(), wr.data(), blk, p);
            for (int i = 0; i < blk; ++i) { mx = std::max(mx, std::abs(wl[(size_t) i])); const double t = (s + i) / SR; if (t > 2 && t < 4) eEarly += (double) wl[(size_t) i] * wl[(size_t) i]; if (t > 58 && t < 60) eLate += (double) wl[(size_t) i] * wl[(size_t) i]; }
        }
        CHECK(eEarly > 1e-3 && eLate > 0.3 * eEarly && eLate < 3.0 * eEarly && mx < 1.01f, "Freeze: frueh=%.4g spaet=%.4g max=%.3f", eEarly, eLate, mx);
        // Infinite Feedback mit Dauer-Eingang
        auto q = dp(DtPingPong, 90.f, 0.5f); q.infinite = true; auto o2 = runDelay(noise((int) (SR * 30), 0.9f), q, 256);
        CHECK(allFinite(o2) && peakOf(o2.l) <= 1.0001f && peakOf(o2.r) <= 1.0001f, "Infinite Peak %.3f", std::max(peakOf(o2.l), peakOf(o2.r)));
    }
    {   // Block- und Samplerate-Unabhaengigkeit des Delays
        auto in = noise(ms2s(1500), 0.4f); auto p = dp(DtTape, 230.f, 0.5f); p.mod = 0.5f;
        auto a = runDelay(in, p, 512), b = runDelay(in, p, 37); double d = 0, e = 0; for (size_t i = 0; i < a.l.size(); ++i) { d += std::pow(a.l[i] - b.l[i], 2); e += a.l[i] * a.l[i]; }
        CHECK(d < 1e-6 * e + 1e-12, "Delay blockabhaengig (rel %.3g)", d / (e + 1e-30));
    }
    // ---------------------------------------------------------------- Reverb
    std::printf("\nReverb RT60 (Soll 2.0 s, Daempfung 0):\n");
    for (double sr : { 44100.0, 48000.0, 96000.0 })
    {
        SR = sr;
        for (int m : { RmRoom, RmHall, RmPlate, RmShimmer, RmModulated })
        {
            ReverbParams p; p.mode = m; p.decay = 2.f; p.damp = 0.f; p.preMs = 0.f; p.mod = 0.f; p.shimmer = 0.f; p.size = 0.5f;
            auto o = runReverb(impulse((int) (SR * 5.0)), p); const double rt = rt60(o.l);
            if (sr == 44100.0) std::printf("  mode %d: RT60 = %.2f s\n", m, rt);
            CHECK(rt > 1.2 && rt < 3.2, "sr=%.0f mode %d RT60 %.2f", sr, m, rt);
            CHECK(allFinite(o), "NaN");
        }
    }
    SR = 44100.0;
    {   // Decay-Regler wirkt monoton
        double prev = 0; for (float d : { 0.5f, 1.f, 2.f, 4.f, 8.f })
        { ReverbParams p; p.decay = d; p.damp = 0.f; p.mod = 0.f; p.preMs = 0.f; auto o = runReverb(impulse((int) (SR * 12)), p); const double rt = rt60(o.l); CHECK(rt > prev * 1.2, "Decay %.1f -> RT %.2f (vorher %.2f)", d, rt, prev); prev = rt; }
        double dprev = 1e9; for (float dm : { 0.f, 0.5f, 1.f }) { ReverbParams p; p.decay = 4.f; p.damp = dm; p.mod = 0.f; p.preMs = 0.f; auto o = runReverb(impulse((int) (SR * 8)), p); const double rt = rt60(o.l); CHECK(rt < dprev + 0.05, "Damping %.1f RT %.2f", dm, rt); dprev = rt; }
        double sprev = 0; for (float sz : { 0.f, 0.5f, 1.f }) { ReverbParams p; p.size = sz; p.decay = 3.f; p.mod = 0.f; p.preMs = 0.f; auto o = runReverb(impulse((int) (SR * 1)), p);
            int first = 0; for (int i = 0; i < (int) o.l.size(); ++i) if (std::abs(o.l[(size_t) i]) > 0.01f) { first = i; break; } (void) first;
            // Groesse: Mittelpunkt der Energie wandert nach hinten
            double num = 0, den = 0; for (int i = 0; i < (int) o.l.size(); ++i) { const double e = (double) o.l[(size_t) i] * o.l[(size_t) i]; num += e * i; den += e; } const double cen = num / den;
            CHECK(cen > sprev, "Size %.1f Schwerpunkt %.0f (vorher %.0f)", sz, cen, sprev); sprev = cen; }
    }
    {   // Pre-Delay, DC, Duck, Cuts
        ReverbParams p; p.preMs = 100.f; p.mod = 0.f; auto o = runReverb(impulse((int) (SR * 1)), p);
        int first = 0; for (int i = 0; i < (int) o.l.size(); ++i) if (std::abs(o.l[(size_t) i]) > 1e-5f) { first = i; break; }
        CHECK(first >= ms2s(100) - 3, "Pre-Delay: erstes Signal bei %d, erwartet >= %d", first, ms2s(100));
        Stereo dc = silence((int) (SR * 8)); for (auto& v : dc.l) v = 0.5f; for (auto& v : dc.r) v = 0.5f;
        auto od = runReverb(dc, ReverbParams()); double mean = 0; int c = 0; for (int i = (int) (SR * 6); i < (int) od.l.size(); ++i) { mean += od.l[(size_t) i]; ++c; } CHECK(std::abs(mean / c) < 0.02, "Gleichanteil %.4f", mean / c);
        auto n1 = noise((int) (SR * 4), 0.5f); ReverbParams q; auto a = runReverb(n1, q); q.duck = 1.f; auto b = runReverb(n1, q); CHECK(energy(b.l, (int) SR, (int) (SR * 4)) < 0.2 * energy(a.l, (int) SR, (int) (SR * 4)), "Reverb-Ducking");
        auto s100 = sine((int) (SR * 3), 100, 0.5f); ReverbParams l1; l1.lowHz = 20.f; auto r1 = runReverb(s100, l1); l1.lowHz = 1000.f; auto r2 = runReverb(s100, l1); CHECK(energy(r2.l, (int) SR, (int) (SR * 3)) < 0.05 * energy(r1.l, (int) SR, (int) (SR * 3)), "Reverb Low Cut");
        auto s8k = sine((int) (SR * 3), 8000, 0.5f); ReverbParams h1; auto q1 = runReverb(s8k, h1); h1.highHz = 1000.f; auto q2 = runReverb(s8k, h1); CHECK(energy(q2.l, (int) SR, (int) (SR * 3)) < 0.02 * energy(q1.l, (int) SR, (int) (SR * 3)), "Reverb High Cut");
    }
    {   // Modi unterscheiden sich messbar (Energie-Huellkurve in 40-ms-Schritten, gleiche Einstellungen) und im Spektrum
        std::printf("\nModus-Abstand (mittlere dB-Differenz der Huellkurve, Impuls, gleiche Einstellungen):\n");
        const int modes[6] = { RmRoom, RmHall, RmPlate, RmShimmer, RmModulated, RmInfinite };
        std::vector<std::vector<double>> env; std::vector<double> cen;
        for (int m : modes)
        {
            ReverbParams p; p.mode = m; p.decay = 2.5f; p.damp = 0.4f; p.diff = 0.7f; p.size = 0.5f; p.mod = 0.3f; p.preMs = 0.f; p.shimmer = 0.5f;
            auto o = runReverb(impulse((int) (SR * 2)), p); std::vector<double> e; const int st = ms2s(40);
            for (int i = 0; i + st <= (int) o.l.size(); i += st) e.push_back(10 * std::log10(energy(o.l, i, i + st) + energy(o.r, i, i + st) + 1e-12));
            env.push_back(e);
            // Spektralschwerpunkt ueber grobes Goertzel-Raster
            double num = 0, den = 0; for (double f = 100; f < 12000; f *= 1.25) { const double en = goertzel(o.l, (int) (SR * 0.2), (int) (SR * 2), f); num += en * f; den += en; } cen.push_back(num / den);
        }
        double minD = 1e9;
        for (int a = 0; a < 6; ++a) for (int b = a + 1; b < 6; ++b)
        { double d = 0; for (size_t i = 0; i < env[0].size(); ++i) d += std::abs(env[(size_t) a][i] - env[(size_t) b][i]); d /= (double) env[0].size(); minD = std::min(minD, d); std::printf("  %d-%d: %.2f dB\n", modes[a], modes[b], d); }
        // Verschiedene Familien muessen deutlich anders sein; Hall/Shimmer/Modulated/Infinite sind bewusst Varianten derselben Architektur und werden ueber ihre
        // eigenen Merkmale geprueft (Oktav-Energie, Modulationsseitenbaender, Endlosigkeit).
        for (int a : { 0, 1, 2 }) for (int b : { 0, 1, 2 }) if (a < b) { double d = 0; for (size_t i = 0; i < env[0].size(); ++i) d += std::abs(env[(size_t) a][i] - env[(size_t) b][i]); d /= (double) env[0].size(); const double cr = std::abs(cen[(size_t) a] - cen[(size_t) b]) / std::min(cen[(size_t) a], cen[(size_t) b]); CHECK(d > 1.0 || cr > 0.15, "Room/Hall/Plate zu aehnlich (%d-%d: %.2f dB, Schwerpunkt %.0f%%)", a, b, d, cr * 100); }
        (void) minD;
        for (size_t i = 0; i < 6; ++i) std::printf("  Modus %d: Spektralschwerpunkt %.0f Hz\n", modes[i], cen[i]);
        double cmin = 1e9, cmax = 0; for (double c : cen) { cmin = std::min(cmin, c); cmax = std::max(cmax, c); } std::printf("  Spektralschwerpunkte %.0f..%.0f Hz\n", cmin, cmax);
    }
    {   // Shimmer: Oktave oben
        const int n = (int) (SR * 4); auto in = sine((int) (SR * 0.6), 440, 0.4f); in.l.resize((size_t) n, 0.f); in.r.resize((size_t) n, 0.f);
        ReverbParams p; p.mode = RmShimmer; p.shimmer = 1.f; p.decay = 4.f; p.preMs = 0.f; auto sh = runReverb(in, p); p.mode = RmHall; auto hl = runReverb(in, p);
        const int a = (int) (SR * 0.8), b = (int) (SR * 2.5);
        const double sh880 = goertzel(sh.l, a, b, 880), hl880 = goertzel(hl.l, a, b, 880), shr = sh880 / (goertzel(sh.l, a, b, 440) + 1e-12), hlr = hl880 / (goertzel(hl.l, a, b, 440) + 1e-12);
        std::printf("\nShimmer: 880/440-Verhaeltnis %.3f (Hall %.4f)\n", shr, hlr);
        CHECK(sh880 > 10 * hl880 && shr > 5 * hlr, "Shimmer erzeugt keine Oktave");
    }
    {   // Modulated: Seitenbaender um einen 1-kHz-Sinus (Hall ohne Modulation bleibt rein)
        const int n = (int) (SR * 6); auto in = sine((int) (SR * 3), 1000, 0.4f); in.l.resize((size_t) n, 0.f); in.r.resize((size_t) n, 0.f);
        auto side = [&](int mode, float mod) { ReverbParams p; p.mode = mode; p.mod = mod; p.decay = 6.f; p.preMs = 0.f; auto o = runReverb(in, p); const int a = (int) (SR * 1.5), b = (int) (SR * 3);
            double sb = 0; for (double df : { 3.0, 6.0, 10.0, 15.0, 25.0, 40.0 }) sb += goertzel(o.l, a, b, 1000 + df) + goertzel(o.l, a, b, 1000 - df); return sb / (goertzel(o.l, a, b, 1000.0) + 1e-12); };
        const double sMod = side(RmModulated, 1.f), sHall = side(RmHall, 0.f); std::printf("\nSeitenbandverhaeltnis: Modulated(mod=1) %.4f, Hall(mod=0) %.4f\n", sMod, sHall);
        CHECK(sMod > 3.0 * sHall, "Modulation erzeugt keine Seitenbaender");
        ReverbParams q; q.mode = RmInfinite; q.preMs = 0.f; q.decay = 1.f; auto inf = runReverb(impulse((int) (SR * 30)), q); CHECK(energy(inf.l, (int) (SR * 20), (int) (SR * 30)) > 0.3 * energy(inf.l, (int) (SR * 1), (int) (SR * 11)), "Infinite klingt ab");
    }
    {   // Reverse-Modus: Swell (Energie steigt innerhalb einer Phase) und Verzoegerung
        ReverbParams p; p.mode = RmReverse; p.decay = 1.f; p.preMs = 0.f; auto o = runReverb(impulse((int) (SR * 4)), p);
        const int L = (int) (0.3 * SR) > (int) SR ? (int) SR : (int) SR;     // Phasenlaenge = decay (1 s), min 0.3 s
        const double e1 = energy(o.l, L, L + L / 2) + energy(o.r, L, L + L / 2), e2 = energy(o.l, L + L / 2, 2 * L) + energy(o.r, L + L / 2, 2 * L);
        CHECK(e2 > 2.0 * e1, "Reverse-Reverb schwillt nicht an (%.3g -> %.3g)", e1, e2);
        CHECK(peakOf(o.l, 0, L - 10) < 1e-6f, "Reverse: Signal vor der ersten Phase");
    }
    {   // Stabilitaet: alle Modi x extreme Einstellungen, auch Freeze
        for (int m = 0; m < RmCount; ++m) for (int fz = 0; fz < 2; ++fz)
        {
            ReverbParams p; p.mode = m; p.decay = 30.f; p.size = 1.f; p.damp = 0.f; p.diff = 1.f; p.mod = 1.f; p.shimmer = 1.f; p.width = 2.f; p.preMs = 250.f; p.freeze = fz == 1;
            auto in = noise((int) (SR * 12), 0.9f); auto o = runReverb(in, p, 256);
            const float pk = std::max(peakOf(o.l), peakOf(o.r)); CHECK(allFinite(o) && pk < 2.5f, "Reverb Modus %d freeze=%d extrem: Peak %.2f", m, fz, pk);
            p.freeze = false; p.decay = 0.1f; p.size = 0.f; auto o2 = runReverb(in, p, 100); CHECK(allFinite(o2) && std::max(peakOf(o2.l), peakOf(o2.r)) < 2.5f, "Modus %d kurz", m);
        }
        // Freeze-Haltbarkeit: Pegel bleibt ueber 40 s erhalten
        ReverbParams p; p.mode = RmHall; p.decay = 3.f; auto e = std::make_unique<ReverbEngine>(); e->prepare(SR);
        const int blk = 512; Buf il((size_t) blk), ir((size_t) blk), wl((size_t) blk), wr((size_t) blk); Rng g(9); double e1 = 0, e2 = 0; float mx = 0;
        for (int s = 0; s < (int) (SR * 42); s += blk)
        {
            for (int i = 0; i < blk; ++i) il[(size_t) i] = ir[(size_t) i] = (s < (int) SR) ? (g.uni() * 2 - 1) * 0.5f : 0.f;
            p.freeze = s >= (int) SR; e->process(il.data(), ir.data(), wl.data(), wr.data(), blk, p);
            for (int i = 0; i < blk; ++i) { mx = std::max(mx, std::abs(wl[(size_t) i])); const double t = (s + i) / SR; if (t > 2 && t < 4) e1 += (double) wl[(size_t) i] * wl[(size_t) i]; if (t > 38 && t < 40) e2 += (double) wl[(size_t) i] * wl[(size_t) i]; }
        }
        CHECK(e1 > 1e-4 && e2 > 0.3 * e1 && e2 < 3.0 * e1 && mx < 1.5f, "Reverb-Freeze: frueh %.4g spaet %.4g max %.2f", e1, e2, mx);
        // Pegel: weisses Rauschen rein -> Hall-Pegel in der Naehe des Eingangs (Decay-unabhaengig)
        for (float d : { 0.5f, 2.f, 8.f, 20.f }) { ReverbParams q; q.mode = RmHall; q.decay = d; q.damp = 0.f; q.preMs = 0.f; auto in = noise((int) (SR * 40), 0.3f); auto o = runReverb(in, q); const double re = energy(o.l, (int) (SR * 25), (int) (SR * 40)) / energy(in.l, (int) (SR * 25), (int) (SR * 40)); std::printf("  Pegelverhaeltnis Decay %.1f: %.2f\n", d, re); CHECK(re > 0.05 && re < 4.0, "Pegel bei Decay %.1f: %.2f", d, re); }
    }
    // ---------------------------------------------------------------- Routing
    {
        const int n = (int) (SR * 6); auto in = impulse(n); EngineParams ep; ep.delay = dp(DtDigital, 500.f, 0.f); ep.delayMix = 1.f; ep.reverbMix = 1.f; ep.reverb.decay = 1.5f; ep.reverb.preMs = 0.f; ep.reverb.mod = 0.f;
        auto run = [&](int route, int blk = 480) { auto e = std::make_unique<SpaceEngine>(); e->prepare(SR, blk); EngineParams q = ep; q.route = route; Stereo o = in; for (int s = 0; s < n; s += blk) { const int c = std::min(blk, n - s); e->process(o.l.data() + s, o.r.data() + s, c, q); } return o; };
        auto dOnly = run(RtDelayOnly), rOnly = run(RtReverbOnly), dr = run(RtDelayReverb), rd = run(RtReverbDelay), par = run(RtParallel);
        CHECK(peakOf(dOnly.l, ms2s(480), ms2s(520)) > 0.5f && peakOf(dOnly.l, ms2s(600), n) < 0.01f, "Delay only: Echo %.2f / Rest %.4f", peakOf(dOnly.l, ms2s(480), ms2s(520)), peakOf(dOnly.l, ms2s(600), n));
        CHECK(peakOf(rOnly.l, ms2s(480), ms2s(520)) < peakOf(rOnly.l, 0, ms2s(300)) * 3 && energy(rOnly.l, ms2s(600), n) > 1e-4, "Reverb only: Hall vorhanden");
        CHECK(energy(dr.l, ms2s(800), n) > 1e-5 && energy(rd.l, ms2s(800), n) > 1e-5 && energy(par.l, ms2s(800), n) > 1e-5, "Serien-/Parallelrouting liefern Hall");
        // Delay>Reverb: Echo wird verhallt -> Hall nach 500 ms staerker als bei Reverb>Delay (dort verhallt der Impuls, wird dann verzoegert)
        double a = energy(dr.l, ms2s(550), ms2s(900)), b = energy(rd.l, ms2s(550), ms2s(900)); CHECK(std::abs(a - b) > 1e-6 && a != b, "Routing-Reihenfolge ohne Wirkung");
        // Beide Engines unabhaengig: Mix 0 an einer Engine laesst die andere unveraendert
        EngineParams q = ep; q.route = RtParallel; q.reverbMix = 0.f; auto e = std::make_unique<SpaceEngine>(); e->prepare(SR, 480); Stereo o = in; for (int s = 0; s < n; s += 480) { const int c = std::min(480, n - s); e->process(o.l.data() + s, o.r.data() + s, c, q); }
        CHECK(peakOf(o.l, ms2s(600), n) < 0.01f, "Parallel mit Reverb-Mix 0 = nur Delay");
        // Umschalten: kein Knacken (Sprung zwischen Samples), alte Engine geloescht
        EngineParams z = ep; z.delayMix = 0.f; z.reverbMix = 1.f; z.route = RtReverbOnly; auto sw = std::make_unique<SpaceEngine>(); sw->prepare(SR, 480);
        Stereo s2 = sine(n, 220, 0.5f); float maxJump = 0; float last = 0;
        for (int s = 0; s < n; s += 480)
        {
            if (s == 480 * 40) { z.route = RtDelayOnly; }
            if (s == 480 * 80) { z.route = RtParallel; }
            if (s == 480 * 120) { z.route = RtReverbDelay; }
            const int c = std::min(480, n - s); sw->process(s2.l.data() + s, s2.r.data() + s, c, z);
            for (int i = 0; i < c; ++i) { maxJump = std::max(maxJump, std::abs(s2.l[(size_t) (s + i)] - last)); last = s2.l[(size_t) (s + i)]; }
        }
        CHECK(allFinite(s2) && maxJump < 0.1f, "Routing-Wechsel springt (%.3f)", maxJump);
        // Nach Wechsel auf Delay only ist der Hall weg
        auto e3 = std::make_unique<SpaceEngine>(); e3->prepare(SR, 480); EngineParams w = ep; w.route = RtReverbOnly; w.delayMix = 0.f; w.reverbMix = 1.f; Stereo t = impulse(n);
        for (int s = 0; s < n; s += 480) { if (s == 480 * 10) { w.route = RtDelayOnly; w.delay.fb = 0.f; w.delayMix = 1.f; } const int c = std::min(480, n - s); e3->process(t.l.data() + s, t.r.data() + s, c, w); }
        CHECK(peakOf(t.l, 480 * 20, n) < 1e-3f, "Alter Hall nach Routing-Wechsel noch hoerbar (%.5f)", peakOf(t.l, 480 * 20, n));
    }
    // ---------------------------------------------------------------- Rechenlast
    {
        SR = 48000.0; auto e = std::make_unique<SpaceEngine>(); e->prepare(SR, 512); EngineParams p; p.route = RtParallel; p.delay.type = DtPitch; p.reverb.mode = RmShimmer; p.delayMix = 0.5f; p.reverbMix = 0.5f;
        Buf l((size_t) 512), r((size_t) 512); Rng g(2); const int blocks = (int) (SR * 20 / 512); const auto t0 = std::chrono::steady_clock::now();
        for (int b = 0; b < blocks; ++b) { for (int i = 0; i < 512; ++i) l[(size_t) i] = r[(size_t) i] = (g.uni() - 0.5f) * 0.5f; e->process(l.data(), r.data(), 512, p); }
        const double sec = std::chrono::duration<double>(std::chrono::steady_clock::now() - t0).count();
        std::printf("\nRechenlast (Pitch-Delay + Shimmer parallel, 48 kHz stereo, 1 Kern, Linux): %.1f%% eines Kerns (20 s Audio in %.2f s)\n", 100.0 * sec / 20.0, sec);
    }
    std::printf(failures ? "\n%d FEHLER\n" : "\nALLE OK\n", failures);
    return failures ? 1 : 0;
}
