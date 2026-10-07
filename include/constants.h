// Static WoW game data: class-identifying spells, interrupt/CC abilities and
// cooldowns, battle rez spell IDs, enemy ability timings, and combat log event
// filters. All data is constexpr for compile-time evaluation.

#ifndef SHOTCALLERENGINEWOW_CONSTANTS_H
#define SHOTCALLERENGINEWOW_CONSTANTS_H

#include <array>
#include <chrono>
#include <string_view>

namespace Constants {

inline constexpr std::array<std::pair<int, std::string_view>, 27> identifying_spells = { {
    // Death Knight
    { 48743, "Death Knight" }, // Death's Advance (all specs, movement)
    { 48707, "Death Knight" }, // Anti-Magic Shell (all specs, defensive)
    // Demon Hunter
    { 258920, "Demon Hunter" }, // Immolation Aura (all specs)
    { 198793, "Demon Hunter" }, // Vengeful Retreat (all specs, movement)
    // Druid
    { 1126, "Druid" }, // Mark of the Wild (all specs, buff)
    { 8936, "Druid" }, // Regrowth (all specs, self-heal)
    // Evoker
    { 364342, "Evoker" }, // Blessing of the Bronze (all specs, buff)
    { 361469, "Evoker" }, // Living Flame (all specs, heal/damage)
    // Hunter
    { 5384, "Hunter" }, // Feign Death (all specs)
    // Mage
    { 1459, "Mage" }, // Arcane Intellect (all specs, buff)
    { 1953, "Mage" }, // Blink (all specs, movement)
    { 212653, "Mage" }, // Shimmer (all specs, talent replaces Blink)
    // Monk
    { 116670, "Monk" }, // Vivify (all specs, self-heal)
    { 109132, "Monk" }, // Roll (all specs, movement)
    { 115008, "Monk" }, // Chi Torpedo (all specs, talent replaces Roll)
    // Paladin
    { 19750, "Paladin" }, // Flash of Light (all specs, self-heal)
    { 190784, "Paladin" }, // Divine Steed (all specs, movement)
    // Priest
    { 21562, "Priest" }, // Power Word: Fortitude (all specs, buff)
    { 2061, "Priest" }, // Flash Heal (all specs, self-heal)
    // Rogue
    { 1784, "Rogue" }, // Stealth (all specs)
    { 36554, "Rogue" }, // Shadowstep (all specs, movement)
    // Shaman
    { 8004, "Shaman" }, // Healing Surge (all specs, self-heal)
    { 462854, "Shaman" }, // Skyfury (all specs, mastery buff)
    // Warlock
    { 111771, "Warlock" }, // Demonic Gateway (all specs, movement utility)
    { 29893, "Warlock" }, // Create Soulwell (all specs, healthstone well)
    // Warrior
    { 6673, "Warrior" }, // Battle Shout (all specs, buff)
    { 6544, "Warrior" }, // Heroic Leap (all specs, movement)
} };

constexpr std::string_view get_class_from_identifying_spells(int spell_id)
{
    for (const auto& [id, class_name] : identifying_spells) {
        if (id == spell_id) {
            return class_name;
        }
    }

    return "";
}

// Current spec IDs mapped to class and to the combined "Spec Class" name.
inline constexpr std::array<std::tuple<int, std::string_view, std::string_view>, 40> spec_data = { {
    { 250, "Death Knight", "Blood Death Knight" },
    { 251, "Death Knight", "Frost Death Knight" },
    { 252, "Death Knight", "Unholy Death Knight" },
    { 577, "Demon Hunter", "Havoc Demon Hunter" },
    { 581, "Demon Hunter", "Vengeance Demon Hunter" },
    { 1480, "Demon Hunter", "Devourer Demon Hunter" },
    { 102, "Druid", "Balance Druid" },
    { 103, "Druid", "Feral Druid" },
    { 104, "Druid", "Guardian Druid" },
    { 105, "Druid", "Restoration Druid" },
    { 1467, "Evoker", "Devastation Evoker" },
    { 1468, "Evoker", "Preservation Evoker" },
    { 1473, "Evoker", "Augmentation Evoker" },
    { 253, "Hunter", "Beast Mastery Hunter" },
    { 254, "Hunter", "Marksmanship Hunter" },
    { 255, "Hunter", "Survival Hunter" },
    { 62, "Mage", "Arcane Mage" },
    { 63, "Mage", "Fire Mage" },
    { 64, "Mage", "Frost Mage" },
    { 268, "Monk", "Brewmaster Monk" },
    { 269, "Monk", "Windwalker Monk" },
    { 270, "Monk", "Mistweaver Monk" },
    { 65, "Paladin", "Holy Paladin" },
    { 66, "Paladin", "Protection Paladin" },
    { 70, "Paladin", "Retribution Paladin" },
    { 256, "Priest", "Discipline Priest" },
    { 257, "Priest", "Holy Priest" },
    { 258, "Priest", "Shadow Priest" },
    { 259, "Rogue", "Assassination Rogue" },
    { 260, "Rogue", "Outlaw Rogue" },
    { 261, "Rogue", "Subtlety Rogue" },
    { 262, "Shaman", "Elemental Shaman" },
    { 263, "Shaman", "Enhancement Shaman" },
    { 264, "Shaman", "Restoration Shaman" },
    { 265, "Warlock", "Affliction Warlock" },
    { 266, "Warlock", "Demonology Warlock" },
    { 267, "Warlock", "Destruction Warlock" },
    { 71, "Warrior", "Arms Warrior" },
    { 72, "Warrior", "Fury Warrior" },
    { 73, "Warrior", "Protection Warrior" },
} };

// Returns the class for a spec ID, or an empty view for an unknown spec.
constexpr std::string_view get_class_from_spec(int spec_id)
{
    for (const auto& [id, class_name, spec_name] : spec_data) {
        if (id == spec_id) {
            return class_name;
        }
    }

    return "";
}

// Returns the combined "Spec Class" name, or an empty view for an unknown spec.
constexpr std::string_view get_spec_name(int spec_id)
{
    for (const auto& [id, class_name, spec_name] : spec_data) {
        if (id == spec_id) {
            return spec_name;
        }
    }

    return "";
}

inline constexpr std::array<std::string_view, 38> ignorable_events = {
    "RANGE_DAMAGE",
    "RANGE_MISSED",
    "SPELL_AURA_APPLIED",
    "SPELL_AURA_APPLIED_DOSE",
    "SPELL_AURA_BROKEN",
    "SPELL_AURA_BROKEN_SPELL",
    "SPELL_AURA_REFRESH",
    "SPELL_AURA_REMOVED",
    "SPELL_AURA_REMOVED_DOSE",
    "SPELL_CAST_FAILED",
    "SPELL_CAST_START",
    "SPELL_CREATE",
    "SPELL_DAMAGE",
    "SPELL_DISPEL_FAILED",
    "SPELL_INSTAKILL",
    "SPELL_INTERRUPT",
    "SPELL_LEECH",
    "SPELL_MISSED",
    "SPELL_STOLEN",
    "SPELL_SUMMON",
    "SPELL_EMPOWER_START",
    "SPELL_EMPOWER_END",
    "SPELL_EMPOWER_INTERRUPT",
    "SPELL_PERIODIC_DAMAGE",
    "SPELL_PERIODIC_DRAIN",
    "SPELL_PERIODIC_ENERGIZE",
    "SPELL_PERIODIC_LEECH",
    "SPELL_PERIODIC_MISSED",
    "SPELL_BUILDING_DAMAGE",
    "SPELL_BUILDING_HEAL",
    "ENVIRONMENTAL_DAMAGE",
    "DAMAGE_SHIELD",
    "DAMAGE_SHIELD_MISSED",
    "DAMAGE_SPLIT",
    "PARTY_KILL",
    "UNIT_DIED",
    "UNIT_DESTROYED",
    "UNIT_DISSIPATES",
};

constexpr bool is_ignorable_event(std::string_view event)
{
    for (const auto& item : ignorable_events) {
        if (event == item) {
            return true;
        }
    }

    return false;
}

inline constexpr std::array<std::tuple<std::string_view, int, std::chrono::seconds>, 14>
    interrupt_data = { {
        { "Death Knight", 47528, std::chrono::seconds { 15 } }, // Mind Freeze
        { "Demon Hunter", 183752, std::chrono::seconds { 15 } }, // Disrupt
        { "Druid", 106839, std::chrono::seconds { 15 } }, // Skull Bash (Main kick)
        { "Druid", 78675, std::chrono::seconds { 60 } }, // Druid (Moonkin)
        { "Evoker", 351338, std::chrono::seconds { 20 } }, // Quell
        { "Hunter", 187707, std::chrono::seconds { 15 } }, // Muzzle
        { "Mage", 2139, std::chrono::seconds { 24 } }, // Counterspell
        { "Monk", 116705, std::chrono::seconds { 15 } }, // Spear Hand Strike
        { "Paladin", 96231, std::chrono::seconds { 15 } }, // Rebuke
        { "Priest", 15487, std::chrono::seconds { 45 } }, // Silence
        { "Rogue", 1766, std::chrono::seconds { 15 } }, // Kick
        { "Shaman", 57994, std::chrono::seconds { 12 } }, // Wind Shear
        { "Warlock", 19647, std::chrono::seconds { 24 } }, // Spell Lock (Pet)
        { "Warrior", 6552, std::chrono::seconds { 15 } } // Pummel
    } };

constexpr int get_interrupt_id(std::string_view player_class)
{
    for (const auto& [class_name, interrupt_id, interrupt_cd] : interrupt_data) {
        if (player_class == class_name) {
            return interrupt_id;
        }
    }

    return { };
}

constexpr std::chrono::seconds get_interrupt_cd(std::string_view player_class)
{
    for (const auto& [class_name, interrupt_id, interrupt_cd] : interrupt_data) {
        if (player_class == class_name) {
            return interrupt_cd;
        }
    }

    return { };
}

// Returns the class that owns an interrupt spell, or empty for an unknown id.
constexpr std::string_view get_class_from_interrupt_spell(int spell_id)
{
    for (const auto& [class_name, interrupt_id, interrupt_cd] : interrupt_data) {
        if (interrupt_id == spell_id) {
            return class_name;
        }
    }

    return "";
}

inline constexpr std::array<
    std::tuple<std::string_view, std::string_view, int, std::chrono::seconds>, 29>
    crowd_control_data = {
        { { "Death Knight", "Blinding Sleet", 207127, std::chrono::seconds { 60 } },
            { "Death Knight", "Gorefiend's Grasp", 207167, std::chrono::seconds { 90 } },
            { "Demon Hunter", "Chaos Nova", 179057, std::chrono::seconds { 60 } },
            { "Demon Hunter", "Sigil of Silence", 202138, std::chrono::seconds { 90 } },
            { "Demon Hunter", "Sigil of Misery", 207684, std::chrono::seconds { 90 } },
            { "Demon Hunter", "Sigil of Chains", 204598, std::chrono::seconds { 90 } },
            { "Druid", "Mass Entanglement", 102359, std::chrono::seconds { 30 } },
            { "Druid", "Ursol's Vortex", 102793, std::chrono::seconds { 60 } },
            { "Druid", "Typhoon", 132469, std::chrono::seconds { 30 } },
            { "Evoker", "Landslide", 371900, std::chrono::seconds { 90 } },
            { "Evoker", "Deep Breath", 358269, std::chrono::seconds { 120 } },
            { "Evoker", "Tail Swipe", 368725, std::chrono::seconds { 90 } },
            { "Hunter", "Binding Shot", 109248, std::chrono::seconds { 45 } },
            { "Mage", "Frost Nova", 122, std::chrono::seconds { 30 } },
            { "Mage", "Dragon's Breath", 31661, std::chrono::seconds { 45 } },
            { "Mage", "Ring of Frost", 113724, std::chrono::seconds { 45 } },
            { "Mage", "Blast Wave", 157981, std::chrono::seconds { 30 } },
            { "Monk", "Leg Sweep", 119381, std::chrono::seconds { 60 } },
            { "Monk", "Ring of Peace", 116844, std::chrono::seconds { 60 } },
            { "Paladin", "Blinding Light", 105421, std::chrono::seconds { 90 } },
            { "Priest", "Psychic Scream", 8122, std::chrono::seconds { 60 } },
            { "Shaman", "Capacitor Totem", 192058, std::chrono::seconds { 60 } },
            { "Shaman", "Earthgrab Totem", 51485, std::chrono::seconds { 30 } },
            { "Shaman", "Thunderstorm", 51490, std::chrono::seconds { 45 } },
            { "Shaman", "Sundering", 197214, std::chrono::seconds { 40 } },
            { "Warlock", "Howl of Terror", 5484, std::chrono::seconds { 45 } },
            { "Warlock", "Shadowfury", 30283, std::chrono::seconds { 60 } },
            { "Warrior", "Intimidating Shout", 5246, std::chrono::seconds { 90 } },
            { "Warrior", "Shockwave", 46968, std::chrono::seconds { 40 } } }
    };

// Returns the class that owns a crowd-control spell, or empty for an unknown id.
constexpr std::string_view get_class_from_cc_spell(int spell_id)
{
    for (const auto& [class_name, spell_name, cc_spell_id, cc_cooldown] : crowd_control_data) {
        if (cc_spell_id == spell_id) {
            return class_name;
        }
    }

    return "";
}

inline constexpr std::array<int, 5> battle_rez_ids = { 10609, 376999, 20707, 61999, 407133 };

constexpr bool is_battle_rez(int spell_id)
{
    for (const auto& id : battle_rez_ids) {
        if (id == spell_id) {
            return true;
        }
    }
    return false;
}

struct EnemySpellEntry {
    std::string_view enemy_id;
    int spell_id;
    long long first_cast_ms;
    long long cooldown_ms;
    std::string_view callout;
    bool is_interruptable;
    bool is_ccable;
};

inline constexpr std::array<EnemySpellEntry, 54> enemy_data = { {
    // Eco-dome
    { "245092", 1215850, 20000, 37000, "AoE", false, true },
    { "234883", 1221152, 6500, 18200, "AoE", false, true },
    { "242631", 1235368, 6900, 15800, "Tank Frontal", false, true },
    { "236995", 1226111, 15000, 20600, "Ejection", false, true },
    { "234957", 1221483, 15000, 20600, "Dispel", false, true },
    { "234962", 1221679, 6000, 13300, "Leap", false, true },
    // Tazavesh
    { "180567", 357827, 5000, 17000, "Leap", false, true },
    { "246285", 1240912, 14300, 23000, "Buster", false, true },
    { "246285", 1240821, 8000, 23000, "Spread", false, true },
    { "178165", 355429, 11300, 23000, "AOE", false, true },
    { "178141", 355132, 9700, 27900, "Fish sticks", false, true },
    { "180429", 357238, 13600, 26700, "Pulsar", false, true },
    { "179386", 368661, 8300, 14500, "Toss", false, true },
    { "177716", 351119, 8000, 18200, "Tee Pee", true, true },
    { "177816", 355915, 7300, 17000, "Dispel", false, true },
    { "180431", 357260, 13300, 21800, "Unstable Rift", true, true },
    // Halls of Atonement
    { "164557", 326409, 8900, 23000, "AOE", false, true },
    { "167607", 1235326, 15900, 32800, "Stop casting", false, true },
    { "164562", 326450, 15300, 24200, "Loyal Beast", true, true },
    { "165414", 325876, 9700, 24200, "Dispel", false, true },
    // Floodgate
    { "230748", 465827, 6800, 19400, "Warp blood", false, true },
    { "231014", 465120, 8300, 17000, "Loaderbots spinning", false, true },
    // Dawnbreaker
    { "214761", 432448, 8300, 23000, "Seed", false, true },
    { "214761", 431364, 3300, 10900, "Ray", false, true },
    { "210966", 451107, 4900, 20600, "Cocoon", false, true },
    { "228540", 431309, 12400, 23000, "Curse", false, true },
    { "213892", 431309, 12400, 23000, "Curse", false, true },
    { "211261", 451102, 14300, 27800, "Aoe", false, true },
    { "211261", 451119, 8300, 12100, "Dot", false, true },
    { "211262", 451119, 3900, 12100, "Dot", false, true },
    { "211263", 451119, 4900, 12100, "Dot", false, true },
    { "211263", 450854, 12100, 24300, "Orb", false, true },
    // Ara-kara
    { "216293", 434793, 4000, 16900, "AoE Barrage", true, true },
    { "217531", 434802, 9600, 20800, "Fear", true, true },
    { "218324", 438877, 12100, 21900, "AoE", false, true },
    { "216338", 1241693, 6000, 30300, "AoE", false, true },
    { "223253", 448248, 4800, 20600, "Volley", true, true },
    { "216364", 433841, 5800, 19000, "Volley", true, true },
    // Priory of the Sacred Flame
    { "206696", 427609, 20400, 23000, "Stop casting", false, true },
    { "206696", 427621, 3800, 15700, "Impale bleed", false, true },
    { "221760", 444743, 9500, 24300, "Volley", true, true },
    { "212826", 448485, 5900, 12100, "Tank Buster", false, true },
    { "212826", 448492, 14700, 15700, "AoE", false, true },
    { "212831", 427897, 10800, 18200, "AoE", false, true },
    { "239833", 424431, 26100, 37600, "AoE", false, true },
    { "206704", 448791, 15500, 21700, "AoE", false, true },
    { "206699", 446776, 7000, 15800, "Leap bleed", false, true },
    // Cinderbrew Meadery
    { "214697", 463206, 8100, 18100, "Knock", true, true }, // Tenderize
    { "210269", 463218, 8500, 24200, "DoT", true, true }, // Volatile Keg
    { "223423", 448619, 9100, 30300, "Charge", true, true }, // Reckless Delivery
    { "220946", 442995, 10300, 23000, "AoE", true, true }, // Swarming Surprise
    { "222964", 441434, 8700, 23000, "Batch", false, true }, // Failed Batch
    { "220141", 440687, 5900, 25400, "Volley", true, true }, // Honey Volley
    { "218671", 437956, 10500, 18200, "Dispel", true, true }, // Erupting Inferno
} };

constexpr bool is_tracked_enemy(std::string_view enemy_id)
{
    for (const auto& entry : enemy_data) {
        if (entry.enemy_id == enemy_id) {
            return true;
        }
    }
    return false;
}

constexpr bool is_enemy_ccable(std::string_view enemy_id)
{
    for (const auto& entry : enemy_data) {
        if (entry.enemy_id == enemy_id) {
            return entry.is_ccable;
        }
    }
    return false;
}

constexpr bool is_interrupt(int spell_id)
{
    for (const auto& [class_name, id, cd] : interrupt_data) {
        if (id == spell_id) {
            return true;
        }
    }
    return false;
}

constexpr bool is_crowd_control(int spell_id)
{
    for (const auto& [class_name, spell_name, id, cd] : crowd_control_data) {
        if (id == spell_id) {
            return true;
        }
    }
    return false;
}

} // namespace Constants

#endif // SHOTCALLERENGINEWOW_CONSTANTS_H
