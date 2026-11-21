// VOLTA·4 – Parameterliste (alle automatisierbar und per MIDI steuerbar).
#pragma once
#include <cstdio>
#include <string>
#include <vector>

namespace volta
{
enum class PT { Float, Choice, Bool, Int };
struct PInfo { std::string id, name; PT type; float min, max, def; int defaultCC; };

inline const std::vector<PInfo>& paramList()
{
    static const std::vector<PInfo> list = []
    {
        std::vector<PInfo> v;
        auto F = [&] (const char* id, const char* name, float def, int cc) { v.push_back ({ id, name, PT::Float, 0.f, 1.f, def, cc }); };
        F ("bpm", "Tempo", .45f, 20);  F ("swing", "Swing", 0.f, 21);  F ("vol", "Lautstaerke", .75f, 22);
        F ("dtime", "Echo-Zeit", .5f, 23);  F ("dfb", "Echo-Feedback", .35f, 24);  F ("dmix", "Echo-Rueckweg", .6f, 25);
        F ("l1r", "LFO 1 Rate", .3f, 26);  F ("l2r", "LFO 2 Rate", .15f, 27);  F ("prob", "Prob", .45f, 28);  F ("div", "Teiler", .29f, 29);
        v.push_back ({ "l1s", "LFO 1 Form", PT::Choice, 0, 3, 0, 82 });
        v.push_back ({ "l2s", "LFO 2 Form", PT::Choice, 0, 3, 1, 83 });

        struct ChDef { const char* id; const char* name; float d[7]; int cc; };
        const ChDef chs[4] = {
            { "kick",  "Kick",  { .3f, .4f, .6f, .5f, .75f, .85f, 0.f },  30 },
            { "snare", "Snare", { .4f, .35f, .5f, .6f, .75f, .7f, .25f }, 40 },
            { "bass",  "Bass",  { .4f, .3f, .35f, .5f, .8f, .7f, .15f },  50 },
            { "hats",  "Hats",  { .5f, .15f, .6f, .6f, .75f, .5f, .1f },  102 } };
        const char* keys[7]  = { "tune", "decay", "tone", "snap", "mod", "level", "fx" };
        const char* names[7] = { "Tonhoehe", "Ausklang", "Farbe", "Biss", "CV-Tiefe", "Pegel", "Echo" };
        for (const auto& c : chs)
            for (int k = 0; k < 7; ++k)
                v.push_back ({ std::string (c.id) + "_" + keys[k], std::string (c.name) + " " + names[k], PT::Float, 0.f, 1.f, c.d[k], c.cc + k });

        const char* pats[4] = { "x...x...x...x.x.", "....x.......x...", "x..x..x...xx..x.", "..x....x.x...x.." };
        for (int r = 0; r < 4; ++r)
        {
            const std::string L (1, (char) ('A' + r));
            v.push_back ({ "len_" + L,  "Seq " + L + " Laenge", PT::Int, 1, 16, 16, 84 + r });
            v.push_back ({ "mute_" + L, "Seq " + L + " Mute",   PT::Bool, 0, 1, 0, 70 + r });
            v.push_back ({ "solo_" + L, "Seq " + L + " Solo",   PT::Bool, 0, 1, 0, 74 + r });
        }
        for (int r = 0; r < 4; ++r)
            for (int k = 0; k < 16; ++k)
            {
                char id[16], nm[32];
                std::snprintf (id, sizeof (id), "seq_%c_%02d", 'A' + r, k + 1);
                std::snprintf (nm, sizeof (nm), "Seq %c Schritt %02d", 'A' + r, k + 1);
                v.push_back ({ id, nm, PT::Bool, 0, 1, pats[r][k] == 'x' ? 1.f : 0.f, -1 });
            }
        v.push_back ({ "play", "Play", PT::Bool, 0, 1, 0, 80 });
        v.push_back ({ "host", "Host-Sync", PT::Bool, 0, 1, 0, 81 });
        return v;
    }();
    return list;
}
} // namespace volta
