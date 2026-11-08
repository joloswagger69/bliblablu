#include "SpaceProcessor.h"
#if SPACE_WITH_GUI
#include "gui/SpaceEditor.h"
#endif

namespace spf
{
namespace
{
juce::StringArray split(const char* s) { return juce::StringArray::fromTokens(s, "|", ""); }
struct RandSpec { const char* id; float lo, hi; };
}

juce::AudioProcessorValueTreeState::ParameterLayout SpaceProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto F = [&](const char* id, const char* name, NormalisableRange<float> r, float def, const char* label = "") { l.add(std::make_unique<AudioParameterFloat>(ParameterID { id, 1 }, name, r, def, AudioParameterFloatAttributes().withLabel(label))); };
    auto I = [&](const char* id, const char* name, int lo, int hi, int def) { l.add(std::make_unique<AudioParameterInt>(ParameterID { id, 1 }, name, lo, hi, def)); };
    auto C = [&](const char* id, const char* name, const char* items, int def) { l.add(std::make_unique<AudioParameterChoice>(ParameterID { id, 1 }, name, split(items), def)); };
    auto B = [&](const char* id, const char* name, bool def) { l.add(std::make_unique<AudioParameterBool>(ParameterID { id, 1 }, name, def)); };
    using R = NormalisableRange<float>;

    C("route", "Routing", sfx::routeNames(), sfx::RtDelayReverb);
    F("out", "Output", R(-24.f, 12.f), 0.f, "dB");
    // Delay
    C("d_type", "Delay type", sfx::delayTypeNames(), sfx::DtDigital); B("d_sync", "Delay sync", true); C("d_note", "Delay note", sfx::noteNames(), 9);
    F("d_time", "Delay time", R(1.f, 4000.f, 0.f, 0.35f), 375.f, "ms"); F("d_fb", "Delay feedback", R(0.f, 1.f), 0.35f); F("d_mix", "Delay mix", R(0.f, 1.f), 0.3f);
    F("d_width", "Delay width", R(0.f, 2.f), 1.f); F("d_pan", "Delay pan", R(-1.f, 1.f), 0.f);
    F("d_lcut", "Delay low cut", R(20.f, 2000.f, 0.f, 0.3f), 20.f, "Hz"); F("d_hcut", "Delay high cut", R(500.f, 20000.f, 0.f, 0.3f), 20000.f, "Hz");
    F("d_mod", "Delay modulation", R(0.f, 1.f), 0.2f); F("d_off", "Delay stereo offset", R(-0.5f, 0.5f), 0.f); F("d_duck", "Delay ducking", R(0.f, 1.f), 0.f);
    I("d_pitch", "Delay pitch", -24, 24, 12); B("d_inf", "Delay infinite feedback", false); B("fz_d", "Delay freeze", false);
    // Reverb
    C("r_mode", "Reverb mode", sfx::reverbModeNames(), sfx::RmHall); F("r_decay", "Reverb decay", R(0.1f, 30.f, 0.f, 0.4f), 2.5f, "s");
    F("r_pre", "Reverb pre-delay", R(0.f, 250.f, 0.f, 0.6f), 15.f, "ms"); F("r_size", "Reverb size", R(0.f, 1.f), 0.5f); F("r_damp", "Reverb damping", R(0.f, 1.f), 0.4f);
    F("r_diff", "Reverb diffusion", R(0.f, 1.f), 0.7f); F("r_width", "Reverb width", R(0.f, 2.f), 1.f); F("r_mod", "Reverb modulation", R(0.f, 1.f), 0.3f);
    F("r_lcut", "Reverb low cut", R(20.f, 1000.f, 0.f, 0.3f), 20.f, "Hz"); F("r_hcut", "Reverb high cut", R(1000.f, 20000.f, 0.f, 0.3f), 20000.f, "Hz");
    F("r_mix", "Reverb mix", R(0.f, 1.f), 0.3f); F("r_duck", "Reverb ducking", R(0.f, 1.f), 0.f); F("r_shim", "Reverb shimmer", R(0.f, 1.f), 0.5f); B("fz_r", "Reverb freeze", false);
    return l;
}

SpaceProcessor::SpaceProcessor()
    : juce::AudioProcessor(BusesProperties().withInput("Input", juce::AudioChannelSet::stereo(), true).withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "NWA_PARAMS", createLayout()),
      presets(apvts, makePluginInfo().id, makePluginInfo().name.replace("//", "-"), makePluginInfo().stateVersion)
{
    engine.prepare(44100.0, 512);
}

bool SpaceProcessor::isBusesLayoutSupported(const BusesLayout& l) const
{
    const auto o = l.getMainOutputChannelSet();
    return (o == juce::AudioChannelSet::mono() || o == juce::AudioChannelSet::stereo()) && l.getMainInputChannelSet() == o;
}

void SpaceProcessor::prepareToPlay(double sampleRate, int maxBlock)
{
    sr = sampleRate > 1000.0 ? sampleRate : 44100.0;
    bl.assign((size_t) std::max(maxBlock, 1) + 8, 0.f); br = bl;
    engine.prepare(sr, std::max(maxBlock, 64));
}

void SpaceProcessor::setParam(const juce::String& id, float plain)
{
    if (auto* p = apvts.getParameter(id)) p->setValueNotifyingHost(p->convertTo0to1(plain));
}

float SpaceProcessor::resolvedDelayMs() const
{
    if (getRaw("d_sync") > 0.5f) return sfx::noteBeats(getInt("d_note")) * 60000.f / std::max(20.f, hostBpm.load());
    return getRaw("d_time");
}

sfx::EngineParams SpaceProcessor::readParams(double bpm) const
{
    sfx::EngineParams e;
    e.route = std::clamp(getInt("route"), 0, sfx::RtCount - 1);
    e.outGain = nwa::dbToGain(getRaw("out"));
    e.delayMix = getRaw("d_mix"); e.reverbMix = getRaw("r_mix");
    auto& d = e.delay;
    d.type = std::clamp(getInt("d_type"), 0, sfx::DtCount - 1);
    d.timeMs = getRaw("d_sync") > 0.5f ? sfx::noteBeats(getInt("d_note")) * 60000.f / (float) std::max(20.0, bpm) : getRaw("d_time");
    d.timeMs = std::clamp(d.timeMs, 1.f, sfx::DelayEngine::kMaxSec * 1000.f);
    d.fb = getRaw("d_fb"); d.width = getRaw("d_width"); d.pan = getRaw("d_pan"); d.lowHz = getRaw("d_lcut"); d.highHz = getRaw("d_hcut"); d.mod = getRaw("d_mod");
    d.offset = getRaw("d_off"); d.duck = getRaw("d_duck"); d.pitch = getRaw("d_pitch"); d.infinite = getRaw("d_inf") > 0.5f; d.freeze = getRaw("fz_d") > 0.5f;
    auto& r = e.reverb;
    r.mode = std::clamp(getInt("r_mode"), 0, sfx::RmCount - 1); r.decay = getRaw("r_decay"); r.preMs = getRaw("r_pre"); r.size = getRaw("r_size"); r.damp = getRaw("r_damp");
    r.diff = getRaw("r_diff"); r.width = getRaw("r_width"); r.mod = getRaw("r_mod"); r.lowHz = getRaw("r_lcut"); r.highHz = getRaw("r_hcut");
    r.duck = getRaw("r_duck"); r.shimmer = getRaw("r_shim"); r.freeze = getRaw("fz_r") > 0.5f;
    return e;
}

double SpaceProcessor::getTailLengthSeconds() const
{
    const auto e = readParams(hostBpm.load());
    double t = 0.0;
    if (e.route != sfx::RtDelayOnly) t = (e.reverb.freeze || e.reverb.mode == sfx::RmInfinite) ? 30.0 : (double) e.reverb.decay + e.reverb.preMs * 0.001;
    if (e.route != sfx::RtReverbOnly)
    {
        const double fb = (e.delay.infinite || e.delay.freeze) ? 0.999 : std::min(0.98, (double) e.delay.fb * 0.98);
        const double rep = fb < 0.01 ? 1.0 : std::min(30.0, std::log(0.001) / std::log(std::max(fb, 0.01)));       // Wiederholungen bis -60 dB
        t = std::max(t, std::min(30.0, e.delay.timeMs * 0.001 * (1.0 + rep)));
    }
    return std::min(t, 30.0);
}

void SpaceProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals nd;
    const int n = buffer.getNumSamples(), nch = buffer.getNumChannels();
    if (n <= 0 || nch <= 0) return;
    if ((int) bl.size() < n || n > engine.blockCapacity())
    {   // Host liefert mehr als angekuendigt: lieber durchreichen als abstuerzen (keine Allokation im Audio-Thread)
        return;
    }
    double bpm = hostBpm.load();
    if (auto* ph = getPlayHead()) if (auto pos = ph->getPosition()) if (auto b = pos->getBpm()) if (*b > 20.0) { bpm = *b; hostBpm.store((float) bpm); }

    float* L = bl.data(); float* R = br.data();
    std::copy(buffer.getReadPointer(0), buffer.getReadPointer(0) + n, L);
    if (nch > 1) std::copy(buffer.getReadPointer(1), buffer.getReadPointer(1) + n, R); else std::copy(L, L + n, R);
    nwa::sanitize(L, n); nwa::sanitize(R, n);
    float ip = 0.f; for (int i = 0; i < n; ++i) ip = std::max(ip, std::max(std::abs(L[i]), std::abs(R[i])));
    engine.process(L, R, n, readParams(bpm));
    nwa::sanitize(L, n); nwa::sanitize(R, n);
    float op = 0.f; for (int i = 0; i < n; ++i) op = std::max(op, std::max(std::abs(L[i]), std::abs(R[i])));
    if (nch == 1) { auto* o = buffer.getWritePointer(0); for (int i = 0; i < n; ++i) o[i] = 0.5f * (L[i] + R[i]); }
    else { std::copy(L, L + n, buffer.getWritePointer(0)); std::copy(R, R + n, buffer.getWritePointer(1)); for (int c = 2; c < nch; ++c) buffer.clear(c, 0, n); }
    float cur = inPeak.load(); if (ip > cur) inPeak.store(ip);
    cur = outPeak.load(); if (op > cur) outPeak.store(op);
}

void SpaceProcessor::resetAllToDefault()
{
    for (auto* p : getParameters())
        if (auto* rp = dynamic_cast<juce::RangedAudioParameter*>(p)) rp->setValueNotifyingHost(rp->getDefaultValue());
}

void SpaceProcessor::applyFactory(int index)
{
    const auto& all = factoryPresets();
    if (index < 0 || index >= (int) all.size()) return;
    const float keepOut = getRaw("out");
    resetAllToDefault();
    for (auto& kv : all[(size_t) index].v) setParam(kv.first, kv.second);
    setParam("out", keepOut);                       // Ausgangspegel gehoert dem Anwender, nicht dem Preset
    currentFactory = index;
    presets.currentName = {};
}

void SpaceProcessor::randomize(unsigned seed)
{
    sfx::Rng g(seed * 7919u + 13u);
    auto u = [&](float lo, float hi) { return lo + (hi - lo) * g.uni(); };
    auto pick = [&](std::initializer_list<int> v) { return *(v.begin() + (size_t) (g.next() % (uint32_t) v.size())); };
    static const RandSpec ranges[] = {
        { "d_fb", 0.15f, 0.7f }, { "d_mix", 0.15f, 0.45f }, { "d_width", 0.8f, 1.5f }, { "d_pan", -0.3f, 0.3f }, { "d_lcut", 20.f, 400.f }, { "d_hcut", 3000.f, 14000.f }, { "d_mod", 0.f, 0.6f },
        { "d_off", -0.15f, 0.15f }, { "d_duck", 0.f, 0.5f }, { "r_decay", 0.8f, 9.f }, { "r_pre", 0.f, 60.f }, { "r_size", 0.25f, 0.9f }, { "r_damp", 0.2f, 0.7f }, { "r_diff", 0.5f, 1.f },
        { "r_width", 0.8f, 1.6f }, { "r_mod", 0.f, 0.7f }, { "r_lcut", 20.f, 300.f }, { "r_hcut", 4000.f, 16000.f }, { "r_mix", 0.12f, 0.5f }, { "r_duck", 0.f, 0.4f }, { "r_shim", 0.2f, 0.6f } };
    for (auto& r : ranges) setParam(r.id, u(r.lo, r.hi));
    setParam("route", (float) pick({ 0, 0, 1, 2, 2, 3, 4 }));
    setParam("d_type", (float) pick({ 0, 1, 2, 3, 4, 5, 6, 7, 8 })); setParam("r_mode", (float) pick({ 0, 1, 1, 2, 3, 4, 5 }));
    setParam("d_sync", 1.f); setParam("d_note", (float) pick({ 3, 6, 7, 9, 9, 10, 11, 12 }));
    setParam("d_pitch", (float) pick({ -12, -7, 5, 7, 12 }));
    setParam("d_inf", 0.f); setParam("fz_d", 0.f); setParam("fz_r", 0.f);
    currentFactory = -1; presets.currentName = {};
}

void SpaceProcessor::getStateInformation(juce::MemoryBlock& dest) { nwa::state::save(apvts, makePluginInfo().id, makePluginInfo().stateVersion, dest); }
void SpaceProcessor::setStateInformation(const void* data, int size) { nwa::state::load(apvts, makePluginInfo().id, makePluginInfo().stateVersion, data, size); }

#if SPACE_WITH_GUI
juce::AudioProcessorEditor* SpaceProcessor::createEditor() { return gui::createSpaceEditor(*this); }
#else
juce::AudioProcessorEditor* SpaceProcessor::createEditor() { return nullptr; }
#endif
} // namespace spf

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new spf::SpaceProcessor(); }
