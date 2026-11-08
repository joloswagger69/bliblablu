#pragma once
#include "Delay.h"
#include "Reverb.h"

namespace sfx
{
enum Route { RtDelayReverb, RtReverbDelay, RtParallel, RtDelayOnly, RtReverbOnly, RtCount };
inline const char* routeNames() { return "Delay > Reverb|Reverb > Delay|Parallel|Delay only|Reverb only"; }

struct EngineParams
{
    int route = RtDelayReverb;
    DelayParams delay; ReverbParams reverb;
    float delayMix = 0.3f, reverbMix = 0.3f;
    float outGain = 1.f;                // linear
};

// Beide Engines laufen unabhaengig. Mix = Dry/Wet je Engine. Ein Routing-Wechsel blendet den Ausgang kurz aus (8 ms), schaltet um und
// loescht dabei die nicht mehr benutzten Engines (kein altes Echo/Hall-Ende beim spaeteren Wiedereinschalten).
class SpaceEngine
{
public:
    void prepare(double sampleRate, int maxBlock)
    {
        sr = sampleRate; delay.prepare(sr); reverb.prepare(sr);
        const size_t n = (size_t) std::max(maxBlock, 64);
        for (auto* v : { &aL, &aR, &bL, &bR, &cL, &cR, &dL, &dR }) v->assign(n, 0.f);
        fadeStep = 1.f / (0.008f * (float) sr); active = -1; gain = 0.f; outS.setTime(0.02f, sr); outS.reset(1.f);
    }
    void reset() { delay.reset(); reverb.reset(); active = -1; gain = 0.f; }
    int blockCapacity() const { return (int) aL.size(); }

    // In-place. L/R enthalten Eingang und erhalten den Ausgang.
    void process(float* L, float* R, int n, const EngineParams& p)
    {
        if (active < 0) { active = p.route; gain = 1.f; clearUnused(active); }
        float* x[2] = { L, R };
        // Routing-Wechsel: erst ausblenden
        const bool changing = (p.route != active);
        for (int s0 = 0; s0 < n; )
        {
            const int cnt = std::min(n - s0, blockCapacity());
            runBlock(x[0] + s0, x[1] + s0, cnt, p, changing);
            s0 += cnt;
        }
    }
    float delayTailActive() const { return 0.f; }

private:
    void clearUnused(int route)
    {
        const bool useD = route != RtReverbOnly, useR = route != RtDelayOnly;
        if (! useD) delay.reset();
        if (! useR) reverb.reset();
    }
    void runBlock(float* L, float* R, int n, const EngineParams& p, bool changing)
    {
        const float mD = std::clamp(p.delayMix, 0.f, 1.f), mR = std::clamp(p.reverbMix, 0.f, 1.f);
        const float* zeroL = dL.data(); (void) zeroL;
        auto copyIn = [&](std::vector<float>& a, std::vector<float>& b) { std::copy(L, L + n, a.begin()); std::copy(R, R + n, b.begin()); };
        // Engine-Eingaenge und Ausgabe je nach Routing (activeRoute, nicht gewuenschte Route)
        const int rt = active;
        const bool useD = rt != RtReverbOnly, useR = rt != RtDelayOnly;
        if (rt == RtDelayReverb)
        {
            delay.process(L, R, aL.data(), aR.data(), n, p.delay);
            for (int i = 0; i < n; ++i) { bL[(size_t) i] = L[i] * (1.f - mD) + aL[(size_t) i] * mD; bR[(size_t) i] = R[i] * (1.f - mD) + aR[(size_t) i] * mD; }
            reverb.process(bL.data(), bR.data(), cL.data(), cR.data(), n, p.reverb);
            for (int i = 0; i < n; ++i) { L[i] = bL[(size_t) i] * (1.f - mR) + cL[(size_t) i] * mR; R[i] = bR[(size_t) i] * (1.f - mR) + cR[(size_t) i] * mR; }
        }
        else if (rt == RtReverbDelay)
        {
            reverb.process(L, R, aL.data(), aR.data(), n, p.reverb);
            for (int i = 0; i < n; ++i) { bL[(size_t) i] = L[i] * (1.f - mR) + aL[(size_t) i] * mR; bR[(size_t) i] = R[i] * (1.f - mR) + aR[(size_t) i] * mR; }
            delay.process(bL.data(), bR.data(), cL.data(), cR.data(), n, p.delay);
            for (int i = 0; i < n; ++i) { L[i] = bL[(size_t) i] * (1.f - mD) + cL[(size_t) i] * mD; R[i] = bR[(size_t) i] * (1.f - mD) + cR[(size_t) i] * mD; }
        }
        else if (rt == RtParallel)
        {
            delay.process(L, R, aL.data(), aR.data(), n, p.delay);
            reverb.process(L, R, cL.data(), cR.data(), n, p.reverb);
            const float dry = 1.f - std::max(mD, mR);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * dry + aL[(size_t) i] * mD + cL[(size_t) i] * mR; R[i] = R[i] * dry + aR[(size_t) i] * mD + cR[(size_t) i] * mR; }
        }
        else if (rt == RtDelayOnly)
        {
            delay.process(L, R, aL.data(), aR.data(), n, p.delay);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * (1.f - mD) + aL[(size_t) i] * mD; R[i] = R[i] * (1.f - mD) + aR[(size_t) i] * mD; }
        }
        else
        {
            reverb.process(L, R, cL.data(), cR.data(), n, p.reverb);
            for (int i = 0; i < n; ++i) { L[i] = L[i] * (1.f - mR) + cL[(size_t) i] * mR; R[i] = R[i] * (1.f - mR) + cR[(size_t) i] * mR; }
        }
        (void) useD; (void) useR; (void) copyIn;
        // Ausgangspegel + Routing-Ueberblendung
        for (int i = 0; i < n; ++i)
        {
            if (changing) { gain = std::max(0.f, gain - fadeStep); }
            else gain = std::min(1.f, gain + fadeStep);
            const float g = outS.next(p.outGain) * gain;
            L[i] *= g; R[i] *= g;
        }
        if (changing && gain <= 0.f) { active = p.route; clearUnused(active); }
    }

    double sr = 44100.0; DelayEngine delay; ReverbEngine reverb;
    std::vector<float> aL, aR, bL, bR, cL, cR, dL, dR;
    int active = -1; float gain = 0.f, fadeStep = 0.01f; Smooth outS;
};
} // namespace sfx
