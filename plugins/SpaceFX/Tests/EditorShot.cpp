// Rendert den SPACE//FX-Editor offscreen in PNG und bedient ihn ueber seine Buttons (gleicher Pfad wie ein Klick).
// Aufruf: SpaceShot <ausgabeordner>
#include "../Source/SpaceProcessor.h"
#include <juce_gui_basics/juce_gui_basics.h>

static void pump(spf::SpaceProcessor& proc, int ms)
{
    juce::AudioBuffer<float> buf(2, 512); juce::MidiBuffer midi;
    for (int k = 0; k < ms / 15; ++k) { for (int i = 0; i < 512; ++i) { buf.setSample(0, i, (i % 200) < 4 ? 0.5f : 0.f); buf.setSample(1, i, buf.getSample(0, i)); } proc.processBlock(buf, midi); juce::MessageManager::getInstance()->runDispatchLoopUntil(15); }
}
static bool visibleChain(const juce::Component* c) { for (; c != nullptr && c->getParentComponent() != nullptr; c = c->getParentComponent()) if (! c->isVisible()) return false; return true; }
static juce::TextButton* findButton(juce::Component& c, const juce::String& text)
{
    if (auto* b = dynamic_cast<juce::TextButton*>(&c)) if (b->getButtonText() == text && visibleChain(b)) return b;
    for (auto* ch : c.getChildren()) if (auto* r = findButton(*ch, text)) return r;
    return nullptr;
}
static int failures = 0;
#define CHECK(c) do { if (! (c)) { ++failures; std::printf("FEHLER: %s (Zeile %d)\n", #c, __LINE__); } } while (0)

int main(int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI gui;
    juce::File out = juce::File::getCurrentWorkingDirectory();
    if (argc > 1) out = juce::File::getCurrentWorkingDirectory().getChildFile(argv[1]);
    out.createDirectory();

    spf::SpaceProcessor proc; proc.setPlayConfigDetails(2, 2, 44100.0, 512); proc.prepareToPlay(44100.0, 512);
    std::unique_ptr<juce::AudioProcessorEditor> ed(proc.createEditor());
    if (! ed) { std::printf("no editor\n"); return 1; }
    auto shot = [&](const char* name)
    {
        pump(proc, 300);
        auto img = ed->createComponentSnapshot(ed->getLocalBounds(), true, 1.0f);
        juce::PNGImageFormat png; juce::FileOutputStream fo(out.getChildFile(name)); fo.setPosition(0); fo.truncate(); png.writeImageToStream(img, fo);
    };
    shot("space_default.png");

    // Preset vor/zurueck
    auto* next = findButton(*ed, ">"); auto* prev = findButton(*ed, "<"); CHECK(next && prev);
    if (next && prev)
    {
        next->triggerClick(); pump(proc, 60); CHECK(proc.currentFactory == 0);
        const float fb0 = proc.getRaw("d_fb"); const int mode0 = proc.getInt("r_mode");
        next->triggerClick(); pump(proc, 60); CHECK(proc.currentFactory == 1);
        prev->triggerClick(); pump(proc, 60); CHECK(proc.currentFactory == 0); CHECK(std::abs(proc.getRaw("d_fb") - fb0) < 1e-4f && proc.getInt("r_mode") == mode0);
        for (int i = 0; i < 12; ++i) { next->triggerClick(); pump(proc, 30); }
        CHECK(proc.currentFactory == 12);
        shot("space_preset.png");
    }
    // Randomize
    if (auto* r = findButton(*ed, "Randomize"))
    {
        const float before = proc.getRaw("d_fb") + proc.getRaw("r_decay") + (float) proc.getInt("d_type") * 7.f;
        proc.setParam("out", -4.f);
        r->triggerClick(); pump(proc, 60); CHECK(proc.currentFactory == -1);
        const float after = proc.getRaw("d_fb") + proc.getRaw("r_decay") + (float) proc.getInt("d_type") * 7.f;
        CHECK(std::abs(before - after) > 1e-4f); CHECK(proc.getRaw("fz_d") < 0.5f && proc.getRaw("fz_r") < 0.5f && proc.getRaw("d_inf") < 0.5f); CHECK(std::abs(proc.getRaw("out") + 4.f) < 0.01f);
        shot("space_random.png");
    } else CHECK(false);
    // Freeze-Schalter
    {
        auto* fd = findButton(*ed, "FREEZE DELAY"); auto* fr = findButton(*ed, "FREEZE REVERB"); CHECK(fd && fr);
        if (fd) { fd->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("fz_d") > 0.5f); fd->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("fz_d") < 0.5f); }
        if (fr) { fr->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("fz_r") > 0.5f); shot("space_freeze.png"); fr->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("fz_r") < 0.5f); }
    }
    // Delay-Schalter (Tempo sync, Infinite) und Reset
    {
        auto* sy = findButton(*ed, "Tempo sync"); auto* inf = findButton(*ed, "Infinite feedback"); CHECK(sy && inf);
        if (sy) { const float s0 = proc.getRaw("d_sync"); sy->triggerClick(); pump(proc, 60); CHECK((proc.getRaw("d_sync") > 0.5f) != (s0 > 0.5f)); }
        if (inf) { inf->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("d_inf") > 0.5f); inf->triggerClick(); pump(proc, 60); CHECK(proc.getRaw("d_inf") < 0.5f); }
        if (auto* rs = findButton(*ed, "Reset")) { rs->triggerClick(); pump(proc, 60); CHECK(std::abs(proc.getRaw("d_fb") - 0.35f) < 0.2f); } else CHECK(false);
    }
    ed.reset();
    std::printf(failures == 0 ? "ok (GUI-Checks bestanden)\n" : "GUI-Checks: %d Fehler\n", failures);
    return failures == 0 ? 0 : 1;
}
