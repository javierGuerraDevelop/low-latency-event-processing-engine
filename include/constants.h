// Static WoW game data: class-identifying spells, interrupt/CC abilities and
// cooldowns, battle rez spell IDs, enemy ability timings, and combat log event
// filters. All data is constexpr for compile-time evaluation.

#ifndef SHOTCALLERENGINEWOW_CONSTANTS_H
#define SHOTCALLERENGINEWOW_CONSTANTS_H

#include <array>
#include <chrono>
#include <cstdint>
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

// How a callout is delivered; only Kick and Stun assign a party member.
enum class Mechanic : std::uint8_t { Kick,
    Stun,
    Dispel,
    TankHit,
    Movement,
    Awareness };

// Immutable static data for one tracked enemy ability.
struct EnemySpellProfile {
    std::string_view enemy_id;
    int spell_id;
    std::chrono::milliseconds first_cast;
    std::chrono::milliseconds cooldown;
    std::string_view callout;
    Mechanic mechanic;
};

inline constexpr std::array<EnemySpellProfile, 54> enemy_data = { {
    // Eco-dome
    { "245092", 1215850, std::chrono::milliseconds { 20000 }, std::chrono::milliseconds { 37000 }, "AoE", Mechanic::Stun },
    { "234883", 1221152, std::chrono::milliseconds { 6500 }, std::chrono::milliseconds { 18200 }, "AoE", Mechanic::Stun },
    { "242631", 1235368, std::chrono::milliseconds { 6900 }, std::chrono::milliseconds { 15800 }, "Tank Frontal", Mechanic::TankHit },
    { "236995", 1226111, std::chrono::milliseconds { 15000 }, std::chrono::milliseconds { 20600 }, "Ejection", Mechanic::Movement },
    { "234957", 1221483, std::chrono::milliseconds { 15000 }, std::chrono::milliseconds { 20600 }, "Dispel", Mechanic::Dispel },
    { "234962", 1221679, std::chrono::milliseconds { 6000 }, std::chrono::milliseconds { 13300 }, "Leap", Mechanic::Movement },
    // Tazavesh
    { "180567", 357827, std::chrono::milliseconds { 5000 }, std::chrono::milliseconds { 17000 }, "Leap", Mechanic::Movement },
    { "246285", 1240912, std::chrono::milliseconds { 14300 }, std::chrono::milliseconds { 23000 }, "Buster", Mechanic::TankHit },
    { "246285", 1240821, std::chrono::milliseconds { 8000 }, std::chrono::milliseconds { 23000 }, "Spread", Mechanic::Awareness },
    { "178165", 355429, std::chrono::milliseconds { 11300 }, std::chrono::milliseconds { 23000 }, "AOE", Mechanic::Stun },
    { "178141", 355132, std::chrono::milliseconds { 9700 }, std::chrono::milliseconds { 27900 }, "Fish sticks", Mechanic::Awareness },
    { "180429", 357238, std::chrono::milliseconds { 13600 }, std::chrono::milliseconds { 26700 }, "Pulsar", Mechanic::Awareness },
    { "179386", 368661, std::chrono::milliseconds { 8300 }, std::chrono::milliseconds { 14500 }, "Toss", Mechanic::Movement },
    { "177716", 351119, std::chrono::milliseconds { 8000 }, std::chrono::milliseconds { 18200 }, "Tee Pee", Mechanic::Kick },
    { "177816", 355915, std::chrono::milliseconds { 7300 }, std::chrono::milliseconds { 17000 }, "Dispel", Mechanic::Dispel },
    { "180431", 357260, std::chrono::milliseconds { 13300 }, std::chrono::milliseconds { 21800 }, "Unstable Rift", Mechanic::Kick },
    // Halls of Atonement
    { "164557", 326409, std::chrono::milliseconds { 8900 }, std::chrono::milliseconds { 23000 }, "AOE", Mechanic::Stun },
    { "167607", 1235326, std::chrono::milliseconds { 15900 }, std::chrono::milliseconds { 32800 }, "Stop casting", Mechanic::Stun },
    { "164562", 326450, std::chrono::milliseconds { 15300 }, std::chrono::milliseconds { 24200 }, "Loyal Beast", Mechanic::Kick },
    { "165414", 325876, std::chrono::milliseconds { 9700 }, std::chrono::milliseconds { 24200 }, "Dispel", Mechanic::Dispel },
    // Floodgate
    { "230748", 465827, std::chrono::milliseconds { 6800 }, std::chrono::milliseconds { 19400 }, "Warp blood", Mechanic::Awareness },
    { "231014", 465120, std::chrono::milliseconds { 8300 }, std::chrono::milliseconds { 17000 }, "Loaderbots spinning", Mechanic::Awareness },
    // Dawnbreaker
    { "214761", 432448, std::chrono::milliseconds { 8300 }, std::chrono::milliseconds { 23000 }, "Seed", Mechanic::Awareness },
    { "214761", 431364, std::chrono::milliseconds { 3300 }, std::chrono::milliseconds { 10900 }, "Ray", Mechanic::Awareness },
    { "210966", 451107, std::chrono::milliseconds { 4900 }, std::chrono::milliseconds { 20600 }, "Cocoon", Mechanic::Awareness },
    { "228540", 431309, std::chrono::milliseconds { 12400 }, std::chrono::milliseconds { 23000 }, "Curse", Mechanic::Dispel },
    { "213892", 431309, std::chrono::milliseconds { 12400 }, std::chrono::milliseconds { 23000 }, "Curse", Mechanic::Dispel },
    { "211261", 451102, std::chrono::milliseconds { 14300 }, std::chrono::milliseconds { 27800 }, "Aoe", Mechanic::Stun },
    { "211261", 451119, std::chrono::milliseconds { 8300 }, std::chrono::milliseconds { 12100 }, "Dot", Mechanic::Dispel },
    { "211262", 451119, std::chrono::milliseconds { 3900 }, std::chrono::milliseconds { 12100 }, "Dot", Mechanic::Dispel },
    { "211263", 451119, std::chrono::milliseconds { 4900 }, std::chrono::milliseconds { 12100 }, "Dot", Mechanic::Dispel },
    { "211263", 450854, std::chrono::milliseconds { 12100 }, std::chrono::milliseconds { 24300 }, "Orb", Mechanic::Awareness },
    // Ara-kara
    { "216293", 434793, std::chrono::milliseconds { 4000 }, std::chrono::milliseconds { 16900 }, "AoE Barrage", Mechanic::Kick },
    { "217531", 434802, std::chrono::milliseconds { 9600 }, std::chrono::milliseconds { 20800 }, "Fear", Mechanic::Kick },
    { "218324", 438877, std::chrono::milliseconds { 12100 }, std::chrono::milliseconds { 21900 }, "AoE", Mechanic::Stun },
    { "216338", 1241693, std::chrono::milliseconds { 6000 }, std::chrono::milliseconds { 30300 }, "AoE", Mechanic::Stun },
    { "223253", 448248, std::chrono::milliseconds { 4800 }, std::chrono::milliseconds { 20600 }, "Volley", Mechanic::Kick },
    { "216364", 433841, std::chrono::milliseconds { 5800 }, std::chrono::milliseconds { 19000 }, "Volley", Mechanic::Kick },
    // Priory of the Sacred Flame
    { "206696", 427609, std::chrono::milliseconds { 20400 }, std::chrono::milliseconds { 23000 }, "Stop casting", Mechanic::Stun },
    { "206696", 427621, std::chrono::milliseconds { 3800 }, std::chrono::milliseconds { 15700 }, "Impale bleed", Mechanic::TankHit },
    { "221760", 444743, std::chrono::milliseconds { 9500 }, std::chrono::milliseconds { 24300 }, "Volley", Mechanic::Kick },
    { "212826", 448485, std::chrono::milliseconds { 5900 }, std::chrono::milliseconds { 12100 }, "Tank Buster", Mechanic::TankHit },
    { "212826", 448492, std::chrono::milliseconds { 14700 }, std::chrono::milliseconds { 15700 }, "AoE", Mechanic::Stun },
    { "212831", 427897, std::chrono::milliseconds { 10800 }, std::chrono::milliseconds { 18200 }, "AoE", Mechanic::Stun },
    { "239833", 424431, std::chrono::milliseconds { 26100 }, std::chrono::milliseconds { 37600 }, "AoE", Mechanic::Stun },
    { "206704", 448791, std::chrono::milliseconds { 15500 }, std::chrono::milliseconds { 21700 }, "AoE", Mechanic::Stun },
    { "206699", 446776, std::chrono::milliseconds { 7000 }, std::chrono::milliseconds { 15800 }, "Leap bleed", Mechanic::Movement },
    // Cinderbrew Meadery
    { "214697", 463206, std::chrono::milliseconds { 8100 }, std::chrono::milliseconds { 18100 }, "Knock", Mechanic::Kick }, // Tenderize
    { "210269", 463218, std::chrono::milliseconds { 8500 }, std::chrono::milliseconds { 24200 }, "DoT", Mechanic::Kick }, // Volatile Keg
    { "223423", 448619, std::chrono::milliseconds { 9100 }, std::chrono::milliseconds { 30300 }, "Charge", Mechanic::Kick }, // Reckless Delivery
    { "220946", 442995, std::chrono::milliseconds { 10300 }, std::chrono::milliseconds { 23000 }, "AoE", Mechanic::Kick }, // Swarming Surprise
    { "222964", 441434, std::chrono::milliseconds { 8700 }, std::chrono::milliseconds { 23000 }, "Batch", Mechanic::Stun }, // Failed Batch
    { "220141", 440687, std::chrono::milliseconds { 5900 }, std::chrono::milliseconds { 25400 }, "Volley", Mechanic::Kick }, // Honey Volley
    { "218671", 437956, std::chrono::milliseconds { 10500 }, std::chrono::milliseconds { 18200 }, "Dispel", Mechanic::Kick }, // Erupting Inferno
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
