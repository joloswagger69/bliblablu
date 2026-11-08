#include "SpaceEditor.h"
#include "SpaceWidgets.h"

namespace spf::gui
{
using juce::StringArray;
static StringArray items(const char* s) { return StringArray::fromTokens(s, "|", ""); }

// Einfacher Pegelbalken (Spitzenwert mit Abfall)
class Meter : public juce::Component
{
public:
    void setLevel(float v) { const float n = std::max(v, level * 0.82f); if (std::abs(n - level) > 0.002f) { level = n; repaint(); } }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat(); g.setColour(col::background.darker(0.3f)); g.fillRoundedRectangle(r, 3.f);
        const float db = 20.f * std::log10(std::max(level, 1e-4f)); const float f = juce::jlimit(0.f, 1.f, (db + 60.f) / 60.f);
        g.setColour(db > -1.f ? juce::Colour(0xffe0605a) : db > -9.f ? col::warn : col::accent);
        g.fillRoundedRectangle(r.withWidth(r.getWidth() * f).reduced(0.f, 1.f), 3.f);
        g.setColour(col::outline); g.drawRoundedRectangle(r.reduced(0.5f), 3.f, 1.f);
    }
private:
    float level = 0.f;
};

class Panel : public juce::Component
{
public:
    Panel(SpaceProcessor& p, const juce::String& title_, juce::Colour c) : proc(p), title(title_), colour(c) {}
    ParamSlider& slider(const juce::String& name, const juce::String& id, std::function<juce::String(double)> fmt = {})
    {
        auto s = std::make_unique<ParamSlider>(name, 110); s->attach(proc.apvts, id);
        s->slider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 88, 20);
        if (fmt) { s->slider.setTextValueSuffix({}); s->slider.textFromValueFunction = std::move(fmt); s->slider.updateText(); }   // Format enthaelt die Einheit selbst
        addAndMakeVisible(*s); auto& ref = *s; comps.push_back(std::move(s)); return ref;
    }
    ParamCombo& combo(const juce::String& name, const juce::String& id, const char* list)
    {
        auto c = std::make_unique<ParamCombo>(name, items(list), 110); c->attach(proc.apvts, id); addAndMakeVisible(*c); auto& ref = *c; comps.push_back(std::move(c)); return ref;
    }
    ParamToggle& toggle(const juce::String& name, const juce::String& id)
    {
        auto t = std::make_unique<ParamToggle>(name); t->attach(proc.apvts, id); addAndMakeVisible(*t); auto& ref = *t; comps.push_back(std::move(t)); return ref;
    }
    void paint(juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat(); g.setColour(col::panel); g.fillRoundedRectangle(r, 6.f);
        g.setColour(col::outline); g.drawRoundedRectangle(r.reduced(0.5f), 6.f, 1.f);
        g.setColour(colour); g.setFont(juce::FontOptions(17.f, juce::Font::bold)); g.drawText(title, 14, 6, 300, 24, juce::Justification::left);
    }
    void resized() override { int y = 36; for (auto& c : comps) { c->setBounds(10, y, getWidth() - 20, 28); y += 33; } }
private:
    SpaceProcessor& proc; juce::String title; juce::Colour colour; std::vector<std::unique_ptr<juce::Component>> comps;
};

class SpaceEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SpaceEditor(SpaceProcessor& p)
        : juce::AudioProcessorEditor(p), proc(p), route("Routing", items(sfx::routeNames()), 62), outGain("Output", 62),
          freezeD("FREEZE DELAY"), freezeR("FREEZE REVERB"), delay(p, "DELAY", juce::Colour(0xff33c6e0)), reverb(p, "REVERB", juce::Colour(0xff8a7bd8))
    {
        setLookAndFeel(&laf); setSize(1180, 800); setResizable(false, false);
        title.setText("SPACE//FX", juce::dontSendNotification); title.setFont(juce::FontOptions(22.f, juce::Font::bold)); title.setColour(juce::Label::textColourId, col::accent); addAndMakeVisible(title);
        brand.setText("NEO//WAVE AUDIO", juce::dontSendNotification); styleLabel(brand, 11.f); addAndMakeVisible(brand);

        for (auto* c : kCategories) catBox.addItem(c, catBox.getNumItems() + 1);
        addAndMakeVisible(catBox); addAndMakeVisible(facBox); catBox.onChange = [this] { fillFactory(); };
        facBox.onChange = [this] { const int id = facBox.getSelectedId(); if (id > 0) chooseFactory(id - 1); };
        bPrev.setButtonText("<"); bNext.setButtonText(">"); bPrev.onClick = [this] { step(-1); }; bNext.onClick = [this] { step(1); };
        bPrev.setTooltip("Previous factory preset"); bNext.setTooltip("Next factory preset"); addAndMakeVisible(bPrev); addAndMakeVisible(bNext);
        bRand.setButtonText("Randomize"); bRand.setTooltip("Musical random setting (bounded ranges; Freeze / Infinite stay off; output level unchanged)"); bRand.onClick = [this] { randomize(); }; addAndMakeVisible(bRand);
        bInit.setButtonText("Reset"); bInit.setTooltip("All parameters to default"); bInit.onClick = [this] { proc.resetAllToDefault(); proc.currentFactory = -1; setStatus("All parameters reset", false); updateFactoryBoxes(); }; addAndMakeVisible(bInit);
        addAndMakeVisible(userBox); userBox.setTextWhenNothingSelected("User presets..."); userBox.onChange = [this] { loadUser(); };
        bSave.setButtonText("Save"); bSave.onClick = [this] { ask("Save preset as", proc.presets.currentName.isNotEmpty() ? proc.presets.currentName : "My Space", [this](juce::String n) { saveUser(n); }); }; addAndMakeVisible(bSave);
        bDel.setButtonText("Delete"); bDel.onClick = [this] { deleteUser(); }; addAndMakeVisible(bDel);
        styleLabel(status, 13.f, col::text); addAndMakeVisible(status);
        styleLabel(info, 13.f, col::textDim); addAndMakeVisible(info);

        route.attach(proc.apvts, "route"); outGain.attach(proc.apvts, "out"); freezeD.attach(proc.apvts, "fz_d"); freezeR.attach(proc.apvts, "fz_r");
        freezeD.setTooltip("Holds the current delay content (input is gated, feedback 100 %, soft-limited)");
        freezeR.setTooltip("Holds the current reverb tail (input is gated, infinite decay, soft-limited)");
        for (auto* c : std::initializer_list<juce::Component*> { &route, &outGain, &freezeD, &freezeR, &inMeter, &outMeter }) addAndMakeVisible(*c);
        styleLabel(inLabel, 11.f); styleLabel(outLabel, 11.f); inLabel.setText("IN", juce::dontSendNotification); outLabel.setText("OUT", juce::dontSendNotification); addAndMakeVisible(inLabel); addAndMakeVisible(outLabel);

        auto pct = [](double v) { return juce::String((int) std::lround(v * 100.0)) + " %"; };
        auto ms = [](double v) { return juce::String(v, v >= 100.0 ? 0 : 1) + " ms"; };
        auto hz = [](double v) { return v >= 1000.0 ? juce::String(v / 1000.0, 2) + " kHz" : juce::String((int) std::lround(v)) + " Hz"; };
        auto sec = [](double v) { return juce::String(v, 2) + " s"; };
        auto st = [](double v) { const int i = (int) std::lround(v); return (i > 0 ? "+" : "") + juce::String(i) + " st"; };
        delay.combo("Type", "d_type", sfx::delayTypeNames());
        delay.toggle("Tempo sync", "d_sync").setTooltip("On: delay time follows the host tempo (note value). Off: free time in ms.");
        delay.combo("Note", "d_note", sfx::noteNames());
        delay.slider("Time", "d_time", ms).slider.setTooltip("Free time in ms (used when Tempo sync is off)");
        delay.slider("Feedback", "d_fb", pct); delay.slider("Mix", "d_mix", pct); delay.slider("Width", "d_width", pct);
        delay.slider("Pan", "d_pan", [](double v) { return std::abs(v) < 1e-9 ? juce::String("C") : (v < 0 ? "L " : "R ") + juce::String((int) std::lround(std::abs(v) * 100.0)); });
        delay.slider("Low cut", "d_lcut", hz); delay.slider("High cut", "d_hcut", hz); delay.slider("Modulation", "d_mod", pct);
        delay.slider("L/R offset", "d_off", pct).slider.setTooltip("Shifts the right channel's delay time relative to the left (stereo feel)");
        delay.slider("Ducking", "d_duck", pct).slider.setTooltip("Lowers the delay while the input is loud");
        delay.slider("Pitch", "d_pitch", st).slider.setTooltip("Only used by delay type 'Pitch'");
        delay.toggle("Infinite feedback", "d_inf").setTooltip("Feedback 100 %, soft-limited so the level stays bounded");
        reverb.combo("Mode", "r_mode", sfx::reverbModeNames());
        reverb.slider("Decay", "r_decay", sec); reverb.slider("Pre-delay", "r_pre", ms); reverb.slider("Size", "r_size", pct); reverb.slider("Damping", "r_damp", pct);
        reverb.slider("Diffusion", "r_diff", pct); reverb.slider("Width", "r_width", pct); reverb.slider("Modulation", "r_mod", pct);
        reverb.slider("Low cut", "r_lcut", hz); reverb.slider("High cut", "r_hcut", hz); reverb.slider("Mix", "r_mix", pct);
        reverb.slider("Ducking", "r_duck", pct).slider.setTooltip("Lowers the reverb while the input is loud");
        reverb.slider("Shimmer", "r_shim", pct).slider.setTooltip("Only used by reverb mode 'Shimmer' (octave-up feedback)");
        addAndMakeVisible(delay); addAndMakeVisible(reverb);
        delay.resized(); reverb.resized();   // Bounds waren schon gesetzt (setSize im Konstruktor), daher Kinder explizit anordnen

        resized(); refreshUser(); updateFactoryBoxes();
        setStatus("Ready - pick a factory preset or turn the knobs. (Presets are tuned by measurement, not by ear.)", false);
        startTimerHz(20);
    }
    ~SpaceEditor() override { stopTimer(); setLookAndFeel(nullptr); }

    void paint(juce::Graphics& g) override
    {
        g.fillAll(col::background); g.setColour(col::panel); g.fillRect(0, 0, getWidth(), 118);
        g.setColour(col::outline); g.drawHorizontalLine(118, 0.f, (float) getWidth());
    }
    void resized() override
    {
        auto r = getLocalBounds(); auto top = r.removeFromTop(118).reduced(14, 8);
        auto l1 = top.removeFromTop(34);
        title.setBounds(l1.removeFromLeft(150)); brand.setBounds(l1.removeFromLeft(110)); l1.removeFromLeft(6);
        catBox.setBounds(l1.removeFromLeft(150).reduced(0, 3)); l1.removeFromLeft(4);
        bPrev.setBounds(l1.removeFromLeft(30).reduced(0, 3)); facBox.setBounds(l1.removeFromLeft(210).reduced(2, 3)); bNext.setBounds(l1.removeFromLeft(30).reduced(0, 3)); l1.removeFromLeft(6);
        bRand.setBounds(l1.removeFromLeft(88).reduced(2, 3)); bInit.setBounds(l1.removeFromLeft(56).reduced(2, 3)); l1.removeFromLeft(8);
        userBox.setBounds(l1.removeFromLeft(150).reduced(0, 3)); bSave.setBounds(l1.removeFromLeft(58).reduced(2, 3)); bDel.setBounds(l1.removeFromLeft(62).reduced(2, 3));
        top.removeFromTop(6);
        auto l2 = top.removeFromTop(28);
        route.setBounds(l2.removeFromLeft(320)); l2.removeFromLeft(10); outGain.setBounds(l2.removeFromLeft(260)); l2.removeFromLeft(10);
        freezeD.setBounds(l2.removeFromLeft(130).reduced(2, 0)); freezeR.setBounds(l2.removeFromLeft(136).reduced(2, 0)); l2.removeFromLeft(12);
        inLabel.setBounds(l2.removeFromLeft(20)); inMeter.setBounds(l2.removeFromLeft(80).reduced(0, 8)); l2.removeFromLeft(6);
        outLabel.setBounds(l2.removeFromLeft(34)); outMeter.setBounds(l2.removeFromLeft(80).reduced(0, 8));
        top.removeFromTop(4);
        auto l3 = top.removeFromTop(26);
        info.setBounds(l3.removeFromLeft(420)); status.setBounds(l3);
        r.reduce(14, 10);
        const int w = (r.getWidth() - 14) / 2;
        delay.setBounds(r.removeFromLeft(w)); r.removeFromLeft(14); reverb.setBounds(r);
    }
private:
    void timerCallback() override
    {
        inMeter.setLevel(proc.inPeak.exchange(0.f)); outMeter.setLevel(proc.outPeak.exchange(0.f));
        const float bpm = proc.hostBpm.load(); const float ms = proc.resolvedDelayMs();
        juce::String t;
        if (proc.getRaw("d_sync") > 0.5f) t = juce::String(ms, 0) + " ms  (" + items(sfx::noteNames())[juce::jlimit(0, 17, proc.getInt("d_note"))] + " @ " + juce::String(bpm, 1) + " BPM)";
        else t = juce::String(ms, 0) + " ms  (free time, host " + juce::String(bpm, 1) + " BPM)";
        if (t != lastInfo) { lastInfo = t; info.setText("Delay: " + t, juce::dontSendNotification); }
        if (proc.currentFactory != shownFactory) updateFactoryBoxes();
    }
    void setStatus(const juce::String& m, bool warn) { status.setText(m, juce::dontSendNotification); status.setColour(juce::Label::textColourId, warn ? col::warn : col::text); }
    void fillFactory()
    {
        facBox.clear(juce::dontSendNotification); const auto cat = catBox.getText(); const auto& fp = factoryPresets();
        for (int i = 0; i < (int) fp.size(); ++i) if (cat == fp[(size_t) i].category) facBox.addItem(fp[(size_t) i].name, i + 1);
        facBox.setSelectedId(proc.currentFactory + 1, juce::dontSendNotification);
        if (facBox.getSelectedId() == 0) facBox.setTextWhenNothingSelected("Factory presets...");
    }
    void updateFactoryBoxes()
    {
        shownFactory = proc.currentFactory; const auto& fp = factoryPresets();
        if (shownFactory >= 0 && shownFactory < (int) fp.size())
        {
            for (int c = 0; c < catBox.getNumItems(); ++c) if (catBox.getItemText(c) == fp[(size_t) shownFactory].category) catBox.setSelectedItemIndex(c, juce::dontSendNotification);
        }
        else if (catBox.getSelectedId() == 0) catBox.setSelectedItemIndex(0, juce::dontSendNotification);
        fillFactory();
    }
    void chooseFactory(int i)
    {
        if (i < 0 || i >= proc.numFactory()) return;
        proc.applyFactory(i); proc.presets.currentName = {}; userBox.setSelectedId(0, juce::dontSendNotification);
        setStatus(juce::String("Factory preset: ") + factoryPresets()[(size_t) i].category + " / " + factoryPresets()[(size_t) i].name, false); shownFactory = proc.currentFactory;
    }
    void step(int d)
    {
        const int n = proc.numFactory(); int i = proc.currentFactory < 0 ? (d > 0 ? 0 : n - 1) : (proc.currentFactory + d + n) % n;
        chooseFactory(i); updateFactoryBoxes();
    }
    void randomize()
    {
        proc.randomize((unsigned) (juce::Time::getMillisecondCounter() ^ 0x5bd1e995u)); proc.currentFactory = -1; updateFactoryBoxes();
        setStatus("Random setting (musical ranges, Freeze / Infinite off) - press again for another one", false);
    }
    void ask(const juce::String& heading, const juce::String& initial, std::function<void(juce::String)> cb)
    {
        auto* w = new juce::AlertWindow(heading, "Name:", juce::MessageBoxIconType::NoIcon, this);
        w->addTextEditor("name", initial);
        w->addButton("OK", 1, juce::KeyPress(juce::KeyPress::returnKey)); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([w, cb = std::move(cb)](int res) { if (res == 1) cb(w->getTextEditorContents("name")); }), true);
    }
    void refreshUser()
    {
        userBox.clear(juce::dontSendNotification); const auto names = proc.presets.list();
        for (int i = 0; i < names.size(); ++i) userBox.addItem(names[i], i + 1);
        userBox.setText(proc.presets.currentName, juce::dontSendNotification);
    }
    void saveUser(const juce::String& raw)
    {
        const auto name = nwa::PresetManager::sanitizeName(raw);
        if (name.isEmpty()) { setStatus("Invalid preset name", true); return; }
        const auto st = proc.presets.save(name);
        setStatus(st == nwa::PresetManager::Status::ok ? "Preset saved: " + name : "Could not save preset", st != nwa::PresetManager::Status::ok);
        if (st == nwa::PresetManager::Status::ok) proc.presets.currentName = name;
        refreshUser();
    }
    void loadUser()
    {
        const auto name = userBox.getText(); if (name.isEmpty()) return;
        const auto st = proc.presets.load(name);
        if (st == nwa::PresetManager::Status::ok) { proc.currentFactory = -1; proc.presets.currentName = name; updateFactoryBoxes(); setStatus("Preset loaded: " + name, false); }
        else setStatus("Could not load preset '" + name + "'", true);
    }
    void deleteUser()
    {
        const auto name = userBox.getText(); if (name.isEmpty()) return;
        auto* w = new juce::AlertWindow("Delete preset", "Delete '" + name + "' permanently?", juce::MessageBoxIconType::QuestionIcon, this);
        w->addButton("Delete", 1); w->addButton("Cancel", 0, juce::KeyPress(juce::KeyPress::escapeKey));
        w->enterModalState(true, juce::ModalCallbackFunction::create([this, name](int res)
        { if (res == 1) { proc.presets.remove(name); if (proc.presets.currentName == name) proc.presets.currentName = {}; refreshUser(); setStatus("Preset deleted", false); } }), true);
    }

    SpaceProcessor& proc; nwa::LookAndFeel laf;
    juce::Label title, brand, status, info, inLabel, outLabel; juce::ComboBox catBox, facBox, userBox; juce::TextButton bPrev, bNext, bRand, bInit, bSave, bDel;
    ParamCombo route; ParamSlider outGain; ParamToggle freezeD, freezeR; Meter inMeter, outMeter; Panel delay, reverb;
    juce::String lastInfo; int shownFactory = -2;
};

juce::AudioProcessorEditor* createSpaceEditor(SpaceProcessor& p) { return new SpaceEditor(p); }
} // namespace spf::gui
