#pragma once
#include "Common.h"
#include <array>

namespace sfx
{
enum DelayType { DtDigital, DtPingPong, DtStereo, DtMultitap, DtTape, DtReverse, DtPitch, DtModulated, DtFiltered, DtCount };
inline const char* delayTypeNames() { return "Digital|Ping Pong|Stereo|Multitap|Tape|Reverse|Pitch|Modulated|Filtered"; }

struct DelayParams
{
    int   type = DtDigital;
    float timeMs = 375.f;              // bereits aufgeloest (Sync -> ms)
    float fb = 0.35f;                  // 0..1
    float width = 1.f, pan = 0.f;      // 0..2, -1..1
    float lowHz = 20.f, highHz = 20000.f;
    float mod = 0.2f;                  // 0..1
    float offset = 0.f;                // -0.5..0.5 (rechter Kanal relativ zur Zeit)
    float duck = 0.f;                  // 0..1
    float pitch = 12.f;                // Halbtoene, nur Typ Pitch
    bool  infinite = false, freeze = false;
};

// Liefert nur das Effektsignal (wet). Mix, Routing und Ausgangspegel macht die uebergeordnete Engine.
class DelayEngine
{
public:
    static constexpr float kMaxSec = 6.5f;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        const int sz = (int) (2.0 * kMaxSec * sr) + 4096;           // Reverse braucht bis zu 2x die Zeit
        for (auto& d : dl) d.init(sz);
        for (auto& p : ps) p.init(sr);
        timeSm[0].setTime(0.06f, sr); timeSm[1].setTime(0.06f, sr);
        fbS.setTime(0.02f, sr); gInS.setTime(0.03f, sr); modS.setTime(0.05f, sr); duckF.set(0.004f, 0.18f, sr);
        widthS.setTime(0.03f, sr); panS.setTime(0.03f, sr);
        for (auto& o : tapeLp) o.setFreq(5500.f, sr);
        cutS[0].setTime(0.03f, sr / kSub); cutS[1].setTime(0.03f, sr / kSub);
        reset();
    }
    void reset()
    {
        for (auto& d : dl) d.clear();
        for (auto& p : ps) p.clear();
        for (auto& f : filt) for (auto& b : f) { b[0].clear(); b[1].clear(); }
        for (auto& s : svf) s.clear();
        for (auto& o : tapeLp) o.clear();
        lfoA = lfoB = 0.f; revPh[0] = 0.f; revPh[1] = 0.5f; noiseV = noiseT = 0.f;
        timeSm[0].reset(0.f); timeSm[1].reset(0.f); fbS.reset(0.f); gInS.reset(1.f); modS.reset(0.f); widthS.reset(1.f); panS.reset(0.f);
        cutS[0].reset(20.f); cutS[1].reset(20000.f); duckF.env = 0.f; first = true;
    }

    void process(const float* inL, const float* inR, float* wetL, float* wetR, int n, const DelayParams& p)
    {
        const float maxS = kMaxSec * (float) sr;
        const bool tape = p.type == DtTape;
        timeSm[0].setTime(tape ? 0.25f : 0.06f, sr); timeSm[1].setTime(tape ? 0.25f : 0.06f, sr);
        const float t0 = std::clamp(p.timeMs * 0.001f * (float) sr, 48.f, maxS);
        float tgtL = t0, tgtR = t0 * (1.f + p.offset) * (p.type == DtStereo ? 0.75f : 1.f);
        tgtR = std::clamp(tgtR, 48.f, maxS);
        if (first) { timeSm[0].reset(tgtL); timeSm[1].reset(tgtR); fbS.reset(0.f); first = false; }
        const float fbTarget = (p.infinite || p.freeze) ? 1.f : std::clamp(p.fb, 0.f, 1.f) * 0.98f;
        const float gInTarget = p.freeze ? 0.f : 1.f;
        const float ratio = std::pow(2.f, std::clamp(p.pitch, -24.f, 24.f) / 12.f);
        static const float depthMs[DtCount] = { 2.5f, 2.5f, 2.5f, 2.5f, 4.f, 0.f, 2.f, 14.f, 0.f };
        const float depth = depthMs[p.type] * 0.001f * (float) sr;
        const float rateA = p.type == DtModulated ? 0.6f : tape ? 0.55f : 0.8f, rateB = p.type == DtModulated ? 0.83f : tape ? 7.1f : 1.3f;
        const float cLo = std::clamp(p.lowHz, 20.f, 2000.f), cHi = std::clamp(p.highHz, 500.f, 20000.f);

        for (int s0 = 0; s0 < n; s0 += kSub)
        {
            const int cnt = std::min(kSub, n - s0);
            const float lo = cutS[0].next(cLo), hi = cutS[1].next(cHi);
            for (int c = 0; c < 2; ++c) { filt[c][0][0].set(1, lo, 0.7071f, sr); filt[c][0][1].set(0, std::max(hi, lo * 1.5f), 0.7071f, sr); }
            // nur Koeffizienten uebernehmen (Zustaende bleiben erhalten)
            for (int c = 0; c < 2; ++c) { copyCoef(filt[c][1][0], filt[c][0][0]); copyCoef(filt[c][1][1], filt[c][0][1]); }
            const float fcBase = std::sqrt(cLo * cHi);
            for (int k = 0; k < cnt; ++k)
            {
                const int i = s0 + k;
                const float l = inL[i], r = inR[i];
                const float tL = timeSm[0].next(tgtL) - 1.f, tR = timeSm[1].next(tgtR) - 1.f;    // read(d) liefert x[n-1-d] -> Echo exakt nach t Samples
                const float fbg = fbS.next(fbTarget), gIn = gInS.next(gInTarget), md = modS.next(p.mod);
                lfoA += rateA / (float) sr; lfoA -= std::floor(lfoA); lfoB += rateB / (float) sr; lfoB -= std::floor(lfoB);
                const float sAL = std::sin(2.f * kPi * lfoA), sAR = std::sin(2.f * kPi * (lfoA + 0.25f)), sBL = std::sin(2.f * kPi * lfoB), sBR = std::sin(2.f * kPi * (lfoB + 0.25f));
                float modL = sAL * depth * md, modR = sAR * depth * md;
                if (tape)
                {   // Wow (langsam) + Flutter (schnell) + gefilterte Zufallsdrift
                    if (--noiseT <= 0.f) { noiseT = (float) sr * 0.01f; noiseTarget = rng.uni() * 2.f - 1.f; }
                    noiseV += (noiseTarget - noiseV) * 0.002f;
                    modL = (sAL + 0.04f * sBL + 0.3f * noiseV) * depth * md; modR = (sAR + 0.04f * sBR + 0.3f * noiseV) * depth * md;
                }
                float outL = 0.f, outR = 0.f;
                switch (p.type)
                {
                    case DtPingPong:
                    {
                        const float yL = dl[0].read(tL + modL), yR = dl[1].read(tR + modR);
                        const float fL = chain(0, 0, yL), fR = chain(1, 0, yR);
                        dl[0].write(knee((l + r) * 0.5f * gIn + fR * fbg)); dl[1].write(knee(fL * fbg));
                        outL = fL; outR = fR; break;
                    }
                    case DtMultitap:
                    {
                        static const float frac[4] = { 0.25f, 0.5f, 0.75f, 1.f }, gain[4] = { 0.55f, 0.7f, 0.85f, 1.f }, ang[4] = { 0.15f, 0.85f, 0.35f, 0.65f };
                        float sL = 0.f, sR = 0.f, t4L = 0.f, t4R = 0.f;
                        for (int q = 0; q < 4; ++q)
                        {
                            const float aL = dl[0].read(std::max(1.f, (tL + modL) * frac[q])), aR = dl[1].read(std::max(1.f, (tR + modR) * frac[q]));
                            sL += aL * gain[q] * std::cos(ang[q] * kPi * 0.5f) * 1.2f; sR += aR * gain[q] * std::sin(ang[q] * kPi * 0.5f) * 1.2f;
                            if (q == 3) { t4L = aL; t4R = aR; }
                        }
                        outL = chain(0, 0, sL * 0.6f); outR = chain(1, 0, sR * 0.6f);
                        dl[0].write(knee(l * gIn + chain(0, 1, t4L) * fbg)); dl[1].write(knee(r * gIn + chain(1, 1, t4R) * fbg));
                        break;
                    }
                    case DtReverse:
                    {
                        float res[2];
                        const float tt[2] = { tL, tR }, ii[2] = { l, r };
                        for (int c = 0; c < 2; ++c)
                        {
                            const float Ls = std::clamp(tt[c], 0.03f * (float) sr, 6.f * (float) sr);
                            revPh[c] += 1.f / Ls; revPh[c] -= std::floor(revPh[c]);
                            float p2 = revPh[c] + 0.5f; p2 -= std::floor(p2);
                            const float w1 = 0.5f - 0.5f * std::cos(2.f * kPi * revPh[c]), w2 = 0.5f - 0.5f * std::cos(2.f * kPi * p2);
                            const float y = dl[(size_t) c].read(1.f + 2.f * revPh[c] * Ls) * w1 + dl[(size_t) c].read(1.f + 2.f * p2 * Ls) * w2;
                            res[c] = chain(c, 0, y);
                            dl[(size_t) c].write(knee(ii[c] * gIn + res[c] * fbg));
                        }
                        outL = res[0]; outR = res[1]; break;
                    }
                    default:
                    {   // Digital, Stereo, Tape, Pitch, Modulated, Filtered: zwei unabhaengige Leitungen mit Eigenrueckkopplung
                        float y[2] = { dl[0].read(tL + modL), dl[1].read(tR + modR) };
                        const float ii[2] = { l, r };
                        for (int c = 0; c < 2; ++c)
                        {
                            float f = chain(c, 0, y[c]);
                            if (p.type == DtTape) f = tapeLp[c].process(std::tanh(f * 1.6f) * (1.f / 1.6f) * 1.05f);
                            else if (p.type == DtPitch) f = ps[(size_t) c].process(f, ratio);
                            else if (p.type == DtFiltered)
                            {
                                const float fc = std::clamp(fcBase * std::pow(2.f, md * 3.f * (c == 0 ? sAL : sAR)), 60.f, 12000.f);
                                f = svf[c].bandpass(f, fc, 2.5f, sr) * (1.f / 2.5f) * 1.6f;
                            }
                            (c == 0 ? outL : outR) = f;
                            dl[(size_t) c].write(knee(ii[c] * gIn + f * fbg));
                        }
                        break;
                    }
                }
                // Ausgangsstufe: Breite, Pan, Ducking
                const float wd = widthS.next(p.width), pn = panS.next(p.pan);
                const float m = (outL + outR) * 0.5f, sd = (outL - outR) * 0.5f * wd;
                outL = m + sd; outR = m - sd;
                const float ang = (std::clamp(pn, -1.f, 1.f) + 1.f) * kPi * 0.25f;
                const float dg = duckGain(duckF.process(std::max(std::abs(l), std::abs(r))), p.duck);
                wetL[i] = knee(outL * std::cos(ang) * 1.41421356f * dg); wetR[i] = knee(outR * std::sin(ang) * 1.41421356f * dg);
            }
        }
    }

private:
    static constexpr int kSub = 32;
    static void copyCoef(Biquad& to, const Biquad& from) { to.b0 = from.b0; to.b1 = from.b1; to.b2 = from.b2; to.a1 = from.a1; to.a2 = from.a2; }
    float chain(int ch, int set, float x) { auto& f = filt[ch][set]; return f[1].process(f[0].process(x)); }

    double sr = 44100.0; bool first = true;
    std::array<DelayLine, 2> dl; std::array<PitchShifter, 2> ps;
    Smooth timeSm[2], fbS, gInS, modS, widthS, panS, cutS[2];
    Biquad filt[2][2][2];                    // [Kanal][Satz: 0 Ausgang, 1 Rueckkopplung (Multitap)][0 HP, 1 LP]
    Svf svf[2]; OnePole tapeLp[2];
    Follower duckF; Rng rng { 7 };
    float lfoA = 0.f, lfoB = 0.f, revPh[2] = { 0.f, 0.5f }, noiseV = 0.f, noiseT = 0.f, noiseTarget = 0.f;
};
} // namespace sfx
