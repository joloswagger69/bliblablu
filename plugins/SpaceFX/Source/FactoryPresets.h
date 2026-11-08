#pragma once
// Werkspresets. Werte in Klartext der Parameter (Prozent-Regler als 0..1, Zeiten in ms/s, Frequenzen in Hz). Nicht genannte Parameter = Standardwert.
// Ob sie "gut klingen", ist ungehoert; getestet wird nur, dass sie sich messbar unterscheiden und stabil sind.
#include <utility>
#include <vector>

namespace spf
{
struct FactoryPreset { const char* category; const char* name; std::vector<std::pair<const char*, float>> v; };

inline const char* kCategories[10] = { "Clean Mixing", "Ambient", "Atmospheric", "Dub", "Techno", "Jungle", "DnB", "Experimental", "Huge Spaces", "Creative FX" };

// route: 0 Delay>Reverb, 1 Reverb>Delay, 2 Parallel, 3 Delay only, 4 Reverb only
// d_type: 0 Digital 1 PingPong 2 Stereo 3 Multitap 4 Tape 5 Reverse 6 Pitch 7 Modulated 8 Filtered
// r_mode: 0 Room 1 Hall 2 Plate 3 Shimmer 4 Reverse 5 Modulated 6 Infinite
// d_note: 0 1/1, 3 1/2, 6 1/4, 7 1/4 dotted, 8 1/4 triplet, 9 1/8, 10 1/8 dotted, 11 1/8 triplet, 12 1/16, 14 1/16 triplet
inline const std::vector<FactoryPreset>& factoryPresets()
{
    static const std::vector<FactoryPreset> p = {
        // ---- Clean Mixing
        { "Clean Mixing", "Subtle Slap", { {"route",3}, {"d_type",0}, {"d_sync",0}, {"d_time",95}, {"d_fb",0.12f}, {"d_mix",0.18f}, {"d_lcut",150}, {"d_hcut",6500}, {"d_mod",0.1f} } },
        { "Clean Mixing", "Vocal Plate", { {"route",4}, {"r_mode",2}, {"r_decay",1.6f}, {"r_pre",22}, {"r_size",0.45f}, {"r_damp",0.45f}, {"r_diff",0.85f}, {"r_mix",0.22f}, {"r_lcut",180}, {"r_hcut",10000} } },
        { "Clean Mixing", "Short Room", { {"route",4}, {"r_mode",0}, {"r_decay",0.7f}, {"r_pre",6}, {"r_size",0.3f}, {"r_damp",0.5f}, {"r_mix",0.2f}, {"r_lcut",120} } },
        { "Clean Mixing", "Quarter Note Echo", { {"route",0}, {"d_type",0}, {"d_note",6}, {"d_fb",0.3f}, {"d_mix",0.22f}, {"d_lcut",200}, {"d_hcut",5500}, {"r_mode",1}, {"r_decay",1.4f}, {"r_pre",20}, {"r_mix",0.12f} } },
        { "Clean Mixing", "Tight Eighths", { {"route",2}, {"d_type",2}, {"d_note",9}, {"d_fb",0.25f}, {"d_mix",0.2f}, {"d_width",1.2f}, {"d_hcut",7000}, {"r_mode",0}, {"r_decay",0.9f}, {"r_mix",0.15f} } },
        // ---- Ambient
        { "Ambient", "Endless Meadow", { {"route",0}, {"d_type",7}, {"d_note",7}, {"d_fb",0.5f}, {"d_mix",0.35f}, {"d_mod",0.5f}, {"d_hcut",8000}, {"r_mode",1}, {"r_decay",9}, {"r_pre",30}, {"r_size",0.8f}, {"r_damp",0.5f}, {"r_mix",0.55f}, {"r_mod",0.5f} } },
        { "Ambient", "Soft Clouds", { {"route",1}, {"r_mode",5}, {"r_decay",6}, {"r_size",0.7f}, {"r_damp",0.55f}, {"r_mix",0.6f}, {"d_type",0}, {"d_note",6}, {"d_fb",0.4f}, {"d_mix",0.25f}, {"d_hcut",6000} } },
        { "Ambient", "Drifting Hall", { {"route",2}, {"d_type",4}, {"d_note",10}, {"d_fb",0.45f}, {"d_mix",0.3f}, {"d_mod",0.6f}, {"r_mode",1}, {"r_decay",7}, {"r_damp",0.6f}, {"r_mix",0.5f} } },
        { "Ambient", "Glass Garden", { {"route",0}, {"d_type",6}, {"d_note",9}, {"d_fb",0.35f}, {"d_mix",0.25f}, {"d_pitch",12}, {"d_hcut",9000}, {"r_mode",3}, {"r_decay",8}, {"r_shim",0.45f}, {"r_mix",0.5f} } },
        { "Ambient", "Slow Tide", { {"route",4}, {"r_mode",1}, {"r_decay",12}, {"r_pre",60}, {"r_size",0.9f}, {"r_damp",0.7f}, {"r_diff",0.9f}, {"r_mix",0.6f}, {"r_mod",0.4f}, {"r_lcut",150} } },
        // ---- Atmospheric
        { "Atmospheric", "Foggy Cathedral", { {"route",4}, {"r_mode",1}, {"r_decay",14}, {"r_pre",45}, {"r_size",1}, {"r_damp",0.75f}, {"r_diff",0.95f}, {"r_mix",0.55f}, {"r_lcut",200}, {"r_hcut",7000} } },
        { "Atmospheric", "Night Train", { {"route",0}, {"d_type",1}, {"d_note",10}, {"d_fb",0.5f}, {"d_mix",0.3f}, {"d_lcut",250}, {"d_hcut",5000}, {"r_mode",2}, {"r_decay",3.5f}, {"r_mix",0.3f} } },
        { "Atmospheric", "Distant Shore", { {"route",1}, {"r_mode",3}, {"r_decay",6}, {"r_shim",0.3f}, {"r_mix",0.4f}, {"d_type",2}, {"d_note",3}, {"d_fb",0.4f}, {"d_mix",0.3f}, {"d_hcut",4500}, {"d_off",0.2f} } },
        { "Atmospheric", "Pulsing Haze", { {"route",2}, {"d_type",8}, {"d_note",9}, {"d_fb",0.6f}, {"d_mix",0.35f}, {"d_mod",0.7f}, {"d_lcut",300}, {"d_hcut",9000}, {"r_mode",5}, {"r_decay",5}, {"r_mix",0.4f}, {"r_mod",0.8f} } },
        // ---- Dub
        { "Dub", "Dub Echo", { {"route",3}, {"d_type",4}, {"d_note",7}, {"d_fb",0.62f}, {"d_mix",0.4f}, {"d_lcut",250}, {"d_hcut",3500}, {"d_mod",0.3f} } },
        { "Dub", "Tape Skank", { {"route",0}, {"d_type",4}, {"d_note",9}, {"d_fb",0.55f}, {"d_mix",0.35f}, {"d_lcut",180}, {"d_hcut",4000}, {"d_mod",0.5f}, {"r_mode",0}, {"r_decay",1.1f}, {"r_mix",0.18f} } },
        { "Dub", "Spring Dub Throw", { {"route",0}, {"d_type",0}, {"d_note",6}, {"d_fb",0.5f}, {"d_mix",0.4f}, {"d_lcut",300}, {"d_hcut",3000}, {"d_duck",0.5f}, {"r_mode",2}, {"r_decay",2.2f}, {"r_damp",0.6f}, {"r_mix",0.3f} } },
        { "Dub", "Deep Dub Siren", { {"route",3}, {"d_type",8}, {"d_note",7}, {"d_fb",0.75f}, {"d_mix",0.5f}, {"d_mod",0.8f}, {"d_lcut",100}, {"d_hcut",6000} } },
        { "Dub", "Dotted Dub", { {"route",0}, {"d_type",1}, {"d_note",10}, {"d_fb",0.58f}, {"d_mix",0.35f}, {"d_lcut",220}, {"d_hcut",4200}, {"r_mode",1}, {"r_decay",3}, {"r_damp",0.6f}, {"r_mix",0.2f} } },
        // ---- Techno
        { "Techno", "Dark Warehouse", { {"route",0}, {"d_type",0}, {"d_note",10}, {"d_fb",0.45f}, {"d_mix",0.3f}, {"d_lcut",250}, {"d_hcut",4500}, {"r_mode",1}, {"r_decay",5}, {"r_pre",20}, {"r_damp",0.6f}, {"r_mix",0.35f}, {"r_lcut",150} } },
        { "Techno", "Rolling Ping Pong", { {"route",3}, {"d_type",1}, {"d_note",9}, {"d_fb",0.5f}, {"d_mix",0.3f}, {"d_width",1.4f}, {"d_lcut",400}, {"d_hcut",8000}, {"d_duck",0.3f} } },
        { "Techno", "Industrial Plate", { {"route",4}, {"r_mode",2}, {"r_decay",2.8f}, {"r_pre",8}, {"r_diff",1}, {"r_damp",0.3f}, {"r_mix",0.35f}, {"r_hcut",12000}, {"r_lcut",200} } },
        { "Techno", "Hypnotic Multitap", { {"route",0}, {"d_type",3}, {"d_note",6}, {"d_fb",0.4f}, {"d_mix",0.35f}, {"d_lcut",300}, {"d_hcut",7000}, {"r_mode",0}, {"r_decay",1.5f}, {"r_mix",0.2f} } },
        // ---- Jungle
        { "Jungle", "Jungle Echo Chamber", { {"route",0}, {"d_type",0}, {"d_note",9}, {"d_fb",0.4f}, {"d_mix",0.3f}, {"d_hcut",6000}, {"r_mode",0}, {"r_decay",1.2f}, {"r_size",0.5f}, {"r_mix",0.35f} } },
        { "Jungle", "Amen Reverse", { {"route",0}, {"d_type",5}, {"d_note",9}, {"d_fb",0.3f}, {"d_mix",0.35f}, {"r_mode",2}, {"r_decay",1.6f}, {"r_mix",0.25f} } },
        { "Jungle", "Ragga Slap", { {"route",3}, {"d_type",0}, {"d_note",12}, {"d_fb",0.35f}, {"d_mix",0.3f}, {"d_lcut",200}, {"d_hcut",5000}, {"d_off",0.15f} } },
        { "Jungle", "Rave Stab Verb", { {"route",2}, {"d_type",1}, {"d_note",10}, {"d_fb",0.45f}, {"d_mix",0.3f}, {"r_mode",1}, {"r_decay",3.2f}, {"r_pre",10}, {"r_mix",0.4f} } },
        // ---- DnB
        { "DnB", "Liquid Space", { {"route",0}, {"d_type",2}, {"d_note",10}, {"d_fb",0.4f}, {"d_mix",0.25f}, {"d_hcut",9000}, {"r_mode",1}, {"r_decay",4}, {"r_pre",25}, {"r_mix",0.35f}, {"r_diff",0.9f} } },
        { "DnB", "Neuro Tail", { {"route",2}, {"d_type",6}, {"d_note",9}, {"d_fb",0.35f}, {"d_mix",0.25f}, {"d_pitch",-12}, {"r_mode",5}, {"r_decay",2}, {"r_mix",0.3f}, {"r_mod",0.6f} } },
        { "DnB", "Rolling Sixteenths", { {"route",3}, {"d_type",0}, {"d_note",12}, {"d_fb",0.45f}, {"d_mix",0.25f}, {"d_lcut",400}, {"d_hcut",8000}, {"d_duck",0.6f}, {"d_width",1.2f} } },
        { "DnB", "Dark Atmos DnB", { {"route",1}, {"r_mode",1}, {"r_decay",7}, {"r_damp",0.7f}, {"r_mix",0.45f}, {"r_lcut",200}, {"d_type",1}, {"d_note",10}, {"d_fb",0.5f}, {"d_mix",0.25f}, {"d_hcut",4000} } },
        // ---- Experimental
        { "Experimental", "Granular Fall", { {"route",0}, {"d_type",6}, {"d_note",9}, {"d_fb",0.7f}, {"d_mix",0.5f}, {"d_pitch",-7}, {"r_mode",5}, {"r_decay",6}, {"r_mix",0.4f} } },
        { "Experimental", "Reverse Bloom", { {"route",4}, {"r_mode",4}, {"r_decay",2.5f}, {"r_pre",0}, {"r_damp",0.3f}, {"r_mix",0.6f} } },
        { "Experimental", "Metal Drone", { {"route",0}, {"d_type",0}, {"d_sync",0}, {"d_time",12}, {"d_fb",0.9f}, {"d_mix",0.5f}, {"d_mod",0}, {"r_mode",6}, {"r_mix",0.4f}, {"r_damp",0.3f} } },
        { "Experimental", "Broken Tape", { {"route",3}, {"d_type",4}, {"d_note",8}, {"d_fb",0.72f}, {"d_mix",0.5f}, {"d_mod",1}, {"d_hcut",2500}, {"d_lcut",150} } },
        // ---- Huge Spaces
        { "Huge Spaces", "Cathedral XL", { {"route",4}, {"r_mode",1}, {"r_decay",18}, {"r_pre",70}, {"r_size",1}, {"r_damp",0.6f}, {"r_mix",0.55f} } },
        { "Huge Spaces", "Canyon Echo", { {"route",0}, {"d_type",0}, {"d_sync",0}, {"d_time",650}, {"d_fb",0.55f}, {"d_mix",0.4f}, {"d_hcut",5000}, {"r_mode",1}, {"r_decay",8}, {"r_size",0.9f}, {"r_mix",0.4f} } },
        { "Huge Spaces", "Endless Hall", { {"route",4}, {"r_mode",6}, {"r_decay",10}, {"r_mix",0.5f}, {"r_pre",40}, {"r_damp",0.4f} } },
        { "Huge Spaces", "Shimmer Heaven", { {"route",4}, {"r_mode",3}, {"r_decay",12}, {"r_shim",0.7f}, {"r_size",0.9f}, {"r_mix",0.6f}, {"r_diff",0.9f} } },
        { "Huge Spaces", "Planet Room", { {"route",0}, {"d_type",2}, {"d_note",3}, {"d_fb",0.55f}, {"d_mix",0.35f}, {"d_lcut",200}, {"d_hcut",6000}, {"r_mode",1}, {"r_decay",15}, {"r_size",1}, {"r_mix",0.5f}, {"r_mod",0.5f} } },
        // ---- Creative FX
        { "Creative FX", "Octave Climb", { {"route",3}, {"d_type",6}, {"d_note",9}, {"d_fb",0.65f}, {"d_mix",0.45f}, {"d_pitch",12}, {"d_hcut",10000} } },
        { "Creative FX", "Filter Sweep Echo", { {"route",3}, {"d_type",8}, {"d_note",10}, {"d_fb",0.7f}, {"d_mix",0.45f}, {"d_mod",1}, {"d_lcut",120}, {"d_hcut",12000} } },
        { "Creative FX", "Ducked Pad Bloom", { {"route",0}, {"d_type",0}, {"d_note",6}, {"d_fb",0.4f}, {"d_mix",0.3f}, {"d_duck",0.8f}, {"r_mode",1}, {"r_decay",6}, {"r_mix",0.5f}, {"r_duck",0.8f} } },
        { "Creative FX", "Stutter Gate", { {"route",3}, {"d_type",3}, {"d_note",12}, {"d_fb",0.6f}, {"d_mix",0.5f}, {"d_width",1.3f}, {"d_duck",0.4f}, {"d_hcut",9000} } },
        { "Creative FX", "Reverse Dub", { {"route",0}, {"d_type",5}, {"d_note",7}, {"d_fb",0.5f}, {"d_mix",0.4f}, {"d_lcut",200}, {"d_hcut",4500}, {"r_mode",2}, {"r_decay",2}, {"r_mix",0.25f} } },
    };
    return p;
}
} // namespace spf
