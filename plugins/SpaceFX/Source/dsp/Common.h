#pragma once
// Gemeinsame DSP-Bausteine fuer SPACE//FX. Bewusst ohne JUCE-Abhaengigkeit (nur std), damit sie headless getestet werden koennen.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace sfx
{
constexpr float kPi = 3.14159265358979f;

// Transparent bis 0.9, danach weich begrenzt auf maximal 1.0. Sitzt in allen Rueckkopplungsschleifen -> Schleifen koennen nie ueber +-1 wachsen.
inline float knee(float x) noexcept
{
    const float a = std::abs(x);
    if (a <= 0.9f) return x;
    const float o = 0.9f + 0.1f * std::tanh((a - 0.9f) * 10.f);
    return x < 0.f ? -o : o;
}

// Einpol-Glaetter. Zeitkonstante in Sekunden.
struct Smooth
{
    float v = 0.f, c = 1.f;
    void setTime(float sec, double sr) { c = 1.f - std::exp(-1.f / (std::max(sec, 1.0e-4f) * (float) sr)); }
    void reset(float x) { v = x; }
    float next(float target) { v += (target - v) * c; return v; }
};

inline int nextPow2(int n) { int p = 1; while (p < n) p <<= 1; return p; }

// Ringpuffer mit Hermite-Interpolation. read(d): d = Verzoegerung in Samples relativ zum zuletzt geschriebenen (d >= 1 nutzen).
struct DelayLine
{
    std::vector<float> buf; int mask = 0, w = 0;
    void init(int minSize) { const int n = nextPow2(std::max(minSize, 8)); buf.assign((size_t) n, 0.f); mask = n - 1; w = 0; }
    void clear() { std::fill(buf.begin(), buf.end(), 0.f); w = 0; }
    int size() const { return mask + 1; }
    void write(float x) { buf[(size_t) w] = x; w = (w + 1) & mask; }
    float read(float d) const
    {
        d = std::clamp(d, 1.f, (float) (mask - 4));
        const int di = (int) d; const float t = d - (float) di;
        const int i0 = w - 1 - di;
        const float ym1 = buf[(size_t) ((i0 + 1) & mask)], y0 = buf[(size_t) (i0 & mask)], y1 = buf[(size_t) ((i0 - 1) & mask)], y2 = buf[(size_t) ((i0 - 2) & mask)];
        const float c1 = 0.5f * (y1 - ym1), c2 = ym1 - 2.5f * y0 + 2.f * y1 - 0.5f * y2, c3 = 0.5f * (y2 - ym1) + 1.5f * (y0 - y1);
        return ((c3 * t + c2) * t + c1) * t + y0;
    }
};

// RBJ-Biquad (Tief-/Hochpass, Bandpass)
struct Biquad
{
    float b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0, z1 = 0, z2 = 0;
    void clear() { z1 = z2 = 0; }
    void set(int type /*0 LP,1 HP,2 BP*/, float f, float q, double sr)
    {
        f = std::clamp(f, 10.f, (float) sr * 0.45f);
        const float w = 2.f * kPi * f / (float) sr, c = std::cos(w), s = std::sin(w), al = s / (2.f * q), a0 = 1.f + al;
        if (type == 0)      { b0 = (1 - c) * 0.5f; b1 = 1 - c; b2 = b0; }
        else if (type == 1) { b0 = (1 + c) * 0.5f; b1 = -(1 + c); b2 = b0; }
        else                { b0 = al; b1 = 0; b2 = -al; }
        a1 = -2.f * c / a0; a2 = (1.f - al) / a0; b0 /= a0; b1 /= a0; b2 /= a0;
    }
    float process(float x) { const float y = b0 * x + z1; z1 = b1 * x - a1 * y + z2; z2 = b2 * x - a2 * y; return y; }
};

// Einpol-Tiefpass
struct OnePole
{
    float a = 1.f, z = 0.f;
    void setFreq(float f, double sr) { a = 1.f - std::exp(-2.f * kPi * std::clamp(f, 1.f, (float) sr * 0.45f) / (float) sr); }
    void clear() { z = 0.f; }
    float process(float x) { z += a * (x - z); return z; }
};

// Zustandsvariabler Filter (Chamberlin) fuer resonanten Bandpass, stabil bis ~sr/6.
struct Svf
{
    float lo = 0, bd = 0;
    void clear() { lo = bd = 0; }
    float bandpass(float x, float f, float q, double sr)
    {
        const float g = 2.f * std::sin(kPi * std::min(f, (float) sr * 0.16f) / (float) sr), damp = 1.f / q;
        lo += g * bd; const float hi = x - lo - damp * bd; bd += g * hi;
        return bd;
    }
};

// Pitch-Shifter mit zwei ueberblendeten Abspielkoepfen (Delay-Line-Verfahren). ratio = 2^(Halbtoene/12).
struct PitchShifter
{
    DelayLine d; float phase = 0.f; float win = 2048.f;
    void init(double sr) { win = (float) (0.06 * sr); d.init((int) win + 16); phase = 0.f; }
    void clear() { d.clear(); phase = 0.f; }
    float process(float x, float ratio)
    {
        d.write(x);
        phase += (1.f - ratio) / win;
        phase -= std::floor(phase);
        float p2 = phase + 0.5f; p2 -= std::floor(p2);
        const float g1 = 0.5f - 0.5f * std::cos(2.f * kPi * phase), g2 = 0.5f - 0.5f * std::cos(2.f * kPi * p2);
        return d.read(phase * win + 1.f) * g1 + d.read(p2 * win + 1.f) * g2;
    }
};

// Pegelfolger (Attack/Release in Sekunden)
struct Follower
{
    float env = 0.f, ca = 0.f, cr = 0.f;
    void set(float att, float rel, double sr) { ca = std::exp(-1.f / (att * (float) sr)); cr = std::exp(-1.f / (rel * (float) sr)); }
    float process(float x) { const float a = std::abs(x); env = a > env ? a + ca * (env - a) : a + cr * (env - a); return env; }
};

// Ducking-Verstaerkung aus dem Eingangspegel: amount 0..1
inline float duckGain(float env, float amount) noexcept
{
    const float t = std::clamp((env - 0.01f) / 0.15f, 0.f, 1.f);
    return 1.f - amount * t * t * (3.f - 2.f * t);
}

// Notenwert -> Viertelnoten. Index: 6 Grundwerte (1/1..1/32) x {normal, punktiert, Triole}
inline int noteCount() { return 18; }
inline float noteBeats(int idx) noexcept
{
    static const float base[6] = { 4.f, 2.f, 1.f, 0.5f, 0.25f, 0.125f };
    idx = std::clamp(idx, 0, 17);
    const float b = base[idx / 3]; const int k = idx % 3;
    return k == 0 ? b : k == 1 ? b * 1.5f : b * (2.f / 3.f);
}
inline const char* noteNames() { return "1/1|1/1 dotted|1/1 triplet|1/2|1/2 dotted|1/2 triplet|1/4|1/4 dotted|1/4 triplet|1/8|1/8 dotted|1/8 triplet|1/16|1/16 dotted|1/16 triplet|1/32|1/32 dotted|1/32 triplet"; }

struct Rng
{
    uint64_t s;
    explicit Rng(uint64_t seed = 1) : s(seed * 0x9E3779B97F4A7C15ull + 0x1234567ull) {}
    uint32_t next() { s += 0x9E3779B97F4A7C15ull; uint64_t z = s; z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull; z = (z ^ (z >> 27)) * 0x94D049BB133111EBull; return (uint32_t) ((z ^ (z >> 31)) >> 32); }
    float uni() { return (float) (next() >> 8) * (1.f / 16777216.f); }
};
} // namespace sfx
