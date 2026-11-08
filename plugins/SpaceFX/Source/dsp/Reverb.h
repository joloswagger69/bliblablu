#pragma once
#include "Common.h"
#include <array>

namespace sfx
{
enum ReverbMode { RmRoom, RmHall, RmPlate, RmShimmer, RmReverse, RmModulated, RmInfinite, RmCount };
inline const char* reverbModeNames() { return "Room|Hall|Plate|Shimmer|Reverse|Modulated|Infinite"; }

struct ReverbParams
{
    int   mode = RmHall;
    float decay = 2.5f;                // Sekunden (RT60)
    float preMs = 15.f;
    float size = 0.5f, damp = 0.4f, diff = 0.7f;      // 0..1
    float width = 1.f;                 // 0..2
    float mod = 0.3f;                  // 0..1
    float lowHz = 20.f, highHz = 20000.f;
    float duck = 0.f, shimmer = 0.5f;  // 0..1
    bool  freeze = false;
};

// Reverb: Eingangs-Diffusion (4 Allpaesse) -> 8-Kanal-Feedback-Delay-Network mit Hadamard-Mischmatrix, Daempfungsfilter und
// RT60-genauer Rueckkopplungsverstaerkung je Leitung, modulierte Leitungslaengen, Early Reflections. Die Modi unterscheiden sich in
// Leitungslaengen, Diffusion, Daempfung, Modulation, Early-Reflection-Anteil und Zusatzstufen (Pitch-Rueckkopplung, Umkehrung).
class ReverbEngine
{
public:
    static constexpr int N = 8;

    void prepare(double sampleRate)
    {
        sr = sampleRate;
        pre.init((int) (0.30 * sr) + 64);
        for (auto& d : line) d.init((int) (0.45 * sr) + 64);
        const int apLen[4] = { 142, 107, 379, 277 };
        for (int i = 0; i < 4; ++i) { apLenS[i] = std::max(8, (int) std::lround(apLen[i] * sr / 44100.0)); ap[i].init(apLenS[i] + 8); }
        shifter.init(sr);
        const int rl = (int) (4.0 * sr) + 16;
        for (int c = 0; c < 2; ++c) for (int b = 0; b < 2; ++b) rev[c].buf[b].assign((size_t) rl, 0.f);
        sizeS.setTime(0.2f, sr / kSub); decayS.setTime(0.15f, sr / kSub); dampS.setTime(0.1f, sr / kSub); diffS.setTime(0.1f, sr / kSub);
        modS.setTime(0.1f, sr / kSub); preS.setTime(0.1f, sr / kSub); gInS.setTime(0.03f, sr); cutS[0].setTime(0.03f, sr / kSub); cutS[1].setTime(0.03f, sr / kSub);
        duckF.set(0.004f, 0.18f, sr);
        reset();
    }
    void reset()
    {
        pre.clear(); for (auto& d : line) d.clear(); for (auto& a : ap) a.clear(); shifter.clear();
        for (auto& l : lp) l.clear();
        for (auto& f : outF) for (auto& b : f) b.clear();
        for (auto& b : plateHp) b.clear();
        for (auto& r : rev) { for (auto& b : r.buf) std::fill(b.begin(), b.end(), 0.f); r.pos = 0; r.playLen = 0; r.recLen = 0; r.which = 0; }
        shimLp.clear(); dcX = dcY = 0.f; shimInj = 0.f;
        for (int i = 0; i < N; ++i) phase[i] = (float) i / (float) N;
        sizeS.reset(1.f); decayS.reset(std::log(2.5f)); dampS.reset(0.4f); diffS.reset(0.7f); modS.reset(0.3f); preS.reset(15.f); gInS.reset(1.f); cutS[0].reset(20.f); cutS[1].reset(20000.f);
        duckF.env = 0.f; first = true;
    }

    void process(const float* inL, const float* inR, float* wetL, float* wetR, int n, const ReverbParams& p)
    {
        const Cfg& cfg = kCfg[std::clamp(p.mode, 0, RmCount - 1)];
        const bool endless = p.freeze || p.mode == RmInfinite;
        const float sizeScale = 0.5f + std::clamp(p.size, 0.f, 1.f) * 1.5f;
        const float tgtDecay = std::log(std::clamp(p.decay, 0.1f, 30.f));
        if (first) { sizeS.reset(sizeScale); decayS.reset(tgtDecay); dampS.reset(p.damp); diffS.reset(p.diff); modS.reset(p.mod); preS.reset(p.preMs); first = false; }
        const float gInTarget = p.freeze ? 0.f : 1.f;
        const float cLo = std::clamp(p.lowHz, 20.f, 1000.f), cHi = std::clamp(p.highHz, 1000.f, 20000.f);
        static const float inVec[N] = { 1, 1, 1, 1, -1, -1, -1, -1 };
        static const float sL[N] = { 1, -1, 1, -1, 1, -1, 1, -1 }, sR[N] = { 1, 1, -1, -1, 1, 1, -1, -1 };
        static const float erMs[6] = { 3.1f, 7.4f, 11.7f, 16.3f, 21.9f, 29.3f }, erG[6] = { 0.7f, 0.6f, 0.5f, 0.4f, 0.3f, 0.25f };

        for (int s0 = 0; s0 < n; s0 += kSub)
        {
            const int cnt = std::min(kSub, n - s0);
            const float sc = sizeS.next(sizeScale), rt = std::exp(decayS.next(tgtDecay)), dmp = dampS.next(p.damp), df = diffS.next(p.diff), md = modS.next(p.mod), pd = preS.next(p.preMs);
            const float lo = cutS[0].next(cLo), hi = cutS[1].next(cHi);
            // abgeleitete Groessen
            float len[N], g[N]; float gAvg = 0.f;
            const float rtEff = endless ? 20.f : std::min(rt, 60.f);   // Normierung des Eingangs: bei endlosem Nachhall wie ~20 s Decay
            for (int i = 0; i < N; ++i)
            {
                len[i] = std::max(8.f, cfg.base[i] * sc * 0.001f * (float) sr);
                g[i] = endless ? 1.f : std::pow(10.f, -3.f * len[i] / ((float) sr * rt));
                gAvg += std::pow(10.f, -3.f * len[i] / ((float) sr * rtEff));
            }
            gAvg /= (float) N;
            const float inNorm = std::sqrt(std::max(1.f - gAvg * gAvg, 0.0009f));
            // Daempfung 0 = Filter aus (RT60 exakt), 1 = Grenzfrequenz ~1.6 kHz; der Modus verschiebt die Wirkung (Plate heller, Room dunkler)
            float dampHz = 22000.f * std::pow(0.073f, std::clamp(dmp * cfg.dampMul, 0.f, 1.2f));
            if (p.mode == RmInfinite) dampHz = std::max(dampHz, 6000.f);
            const float dampA = (p.freeze || dampHz >= 21000.f) ? 1.f : 1.f - std::exp(-2.f * kPi * std::clamp(dampHz, 800.f, 20000.f) / (float) sr);
            const float apG = std::clamp((0.12f + 0.6f * df) * cfg.diffMul, 0.f, 0.72f);
            const float modDepth = cfg.modScale * (0.05f + 2.0f * md) * 0.001f * (float) sr;
            const float preS_ = std::clamp(pd, 0.f, 250.f) * 0.001f * (float) sr;
            for (int c = 0; c < 2; ++c) { outF[c][0].set(1, lo, 0.7071f, sr); outF[c][1].set(0, std::max(hi, lo * 1.5f), 0.7071f, sr); }
            for (auto& b : plateHp) b.set(1, 90.f, 0.7071f, sr);
            const bool plate = p.mode == RmPlate;

            for (int k = 0; k < cnt; ++k)
            {
                const int i = s0 + k;
                const float l = inL[i], r = inR[i];
                const float gIn = gInS.next(gInTarget);
                // Eingang: Mono, Gleichanteil entfernen, Pre-Delay
                const float mono = (l + r) * 0.5f;
                const float dc = mono - dcX + 0.9995f * dcY; dcX = mono; dcY = dc;
                pre.write(dc * gIn);
                const float x = pre.read(std::max(1.f, preS_));
                float d = x;
                for (int a = 0; a < 4; ++a) d = allpass(a, d, apG);
                // Early Reflections
                float erL = 0.f, erR = 0.f;
                if (cfg.er > 0.f)
                    for (int t = 0; t < 6; ++t)
                    {
                        const float v = pre.read(std::max(1.f, preS_ + erMs[t] * sc * 0.001f * (float) sr)) * erG[t] * cfg.er;
                        if (t & 1) erR += v; else erL += v;
                    }
                // Leitungen lesen, daempfen, mischen
                float y[N];
                for (int q = 0; q < N; ++q)
                {
                    phase[q] += (0.08f + 0.05f * (float) q) / (float) sr; phase[q] -= std::floor(phase[q]);
                    const float dl = len[q] + modDepth * std::sin(2.f * kPi * phase[q]);
                    float v = line[(size_t) q].read(std::max(2.f, dl));
                    v = lp[q].z + dampA * (v - lp[q].z); lp[q].z = v;
                    y[q] = v * g[q];
                }
                float outL = 0.f, outR = 0.f;
                for (int q = 0; q < N; ++q) { outL += y[q] * sL[q]; outR += y[q] * sR[q]; }
                outL *= kOut; outR *= kOut;
                // Shimmer-Rueckkopplung: Tank-Ausgang eine Oktave hoch, zurueck in den Eingang
                float shim = 0.f;
                if (p.mode == RmShimmer)
                {
                    const float sh = shifter.process((outL + outR) * 0.5f, 2.f);
                    shimInj = knee(shimLp.process(sh)); shim = shimInj * std::clamp(p.shimmer, 0.f, 1.f) * 0.4f * inNorm;
                }
                const float inj = (d * inNorm * cfg.inGain + shim);
                // Hadamard-Mischung (orthonormal) -> verlustfrei bis auf g
                float h[N]; for (int q = 0; q < N; ++q) h[q] = y[q];
                for (int len2 = 1; len2 < N; len2 <<= 1)
                    for (int a = 0; a < N; a += len2 << 1)
                        for (int b = a; b < a + len2; ++b) { const float u = h[b], v = h[b + len2]; h[b] = u + v; h[b + len2] = u - v; }
                for (int q = 0; q < N; ++q) line[(size_t) q].write(knee(inj * inVec[q] * kInV + h[q] * kHad));
                float wl = outL + erL, wr = outR + erR;
                if (p.mode == RmReverse) { wl = reverse(0, wl, p.decay); wr = reverse(1, wr, p.decay); }
                // Breite
                const float m = (wl + wr) * 0.5f, sd = (wl - wr) * 0.5f * std::clamp(p.width, 0.f, 2.f);
                wl = m + sd; wr = m - sd;
                if (plate) { wl = plateHp[0].process(wl); wr = plateHp[1].process(wr); }
                wl = outF[0][1].process(outF[0][0].process(wl)); wr = outF[1][1].process(outF[1][0].process(wr));
                const float dg = duckGain(duckF.process(std::max(std::abs(l), std::abs(r))), p.duck);
                wetL[i] = knee(wl * dg); wetR[i] = knee(wr * dg);
            }
        }
    }

private:
    static constexpr int kSub = 32;
    static constexpr float kOut = 1.4f, kHad = 0.35355339f, kInV = 0.35355339f;   // 1/sqrt(8)
    struct Cfg { float base[N]; float er, dampMul, modScale, diffMul, inGain; };
    static constexpr Cfg kCfg[RmCount] = {
        { { 11.3f, 14.9f, 19.7f, 24.1f, 28.3f, 33.7f, 38.9f, 43.1f }, 0.55f, 1.25f, 0.3f, 0.55f, 1.f },    // Room
        { { 31.1f, 37.3f, 43.9f, 53.3f, 61.7f, 71.9f, 83.3f, 97.1f }, 0.2f, 1.f, 0.5f, 1.f, 1.f },         // Hall
        { { 17.3f, 21.7f, 25.9f, 29.3f, 33.7f, 38.9f, 43.1f, 47.9f }, 0.f, 0.6f, 0.6f, 1.2f, 1.f },        // Plate
        { { 33.7f, 39.1f, 47.3f, 55.7f, 64.1f, 73.9f, 85.9f, 99.7f }, 0.1f, 1.f, 0.8f, 1.f, 1.f },         // Shimmer
        { { 23.3f, 29.9f, 37.1f, 44.3f, 52.7f, 61.3f, 69.9f, 79.3f }, 0.f, 1.f, 0.4f, 1.f, 1.f },          // Reverse
        { { 41.3f, 47.9f, 57.1f, 63.7f, 73.3f, 83.9f, 91.7f, 103.3f }, 0.1f, 1.f, 2.5f, 1.f, 1.f },        // Modulated
        { { 37.3f, 43.1f, 51.7f, 59.9f, 67.3f, 77.1f, 89.3f, 101.3f }, 0.f, 1.f, 0.7f, 1.f, 1.f } };       // Infinite

    float allpass(int a, float x, float g)
    {
        const float z = ap[a].read((float) apLenS[a] - 1.f), v = x + g * z;
        ap[a].write(v); return z - g * v;
    }
    // Umkehr-Stufe: nimmt eine Phase auf, spielt die letzte Phase rueckwaerts ab (Swell, abrupter Schnitt). Phasenlaenge ~ Decay.
    float reverse(int c, float x, float decay)
    {
        auto& r = rev[c];
        if (r.recLen == 0) r.recLen = std::clamp((int) (decay * sr), (int) (0.3 * sr), (int) (4.0 * sr));
        r.buf[(size_t) r.which][(size_t) r.pos] = x;
        float out = 0.f;
        if (r.playLen > 0 && r.pos < r.playLen)
        {
            const int idx = r.playLen - 1 - r.pos;
            float g = 1.f; const int fade = (int) (0.004 * sr);
            if (r.pos < fade) g = (float) r.pos / (float) fade;
            if (r.playLen - r.pos < fade) g = std::min(g, (float) (r.playLen - r.pos) / (float) fade);
            out = r.buf[(size_t) (1 - r.which)][(size_t) idx] * g;
        }
        if (++r.pos >= r.recLen) { r.playLen = r.recLen; r.which = 1 - r.which; r.pos = 0; r.recLen = 0; }
        return out;
    }

    double sr = 44100.0; bool first = true;
    DelayLine pre; std::array<DelayLine, N> line; DelayLine ap[4]; int apLenS[4] = { 142, 107, 379, 277 };
    struct Lp { float z = 0.f; void clear() { z = 0.f; } };
    Lp lp[N]; float phase[N] = {};
    Biquad outF[2][2], plateHp[2]; OnePole shimLp = [] { OnePole o; o.a = 0.6f; return o; }(); PitchShifter shifter;
    struct Rev { std::vector<float> buf[2]; int pos = 0, playLen = 0, recLen = 0, which = 0; } rev[2];
    Smooth sizeS, decayS, dampS, diffS, modS, preS, gInS, cutS[2];
    Follower duckF; float dcX = 0.f, dcY = 0.f, shimInj = 0.f;
};
} // namespace sfx
