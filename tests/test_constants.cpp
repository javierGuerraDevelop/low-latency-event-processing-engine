#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <set>
#include <string_view>
#include <utility>

#include "constants.h"

// --- get_class_from_identifying_spells ---

TEST(Constants, IdentifyingSpell_BattleShout_Warrior)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(6673), "Warrior");
}

TEST(Constants, IdentifyingSpell_HeroicLeap_Warrior)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(6544), "Warrior");
}

TEST(Constants, IdentifyingSpell_DeathsAdvance_DeathKnight)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(48743), "Death Knight");
}

TEST(Constants, IdentifyingSpell_ImmolationAura_DemonHunter)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(258920), "Demon Hunter");
}

TEST(Constants, IdentifyingSpell_MarkOfTheWild_Druid)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(1126), "Druid");
}

TEST(Constants, IdentifyingSpell_FirstEntry)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(48743), "Death Knight");
}

TEST(Constants, IdentifyingSpell_LastEntry)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(6544), "Warrior");
}

TEST(Constants, IdentifyingSpell_UnknownReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(999999), "");
}

TEST(Constants, IdentifyingSpell_ZeroReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(0), "");
}

TEST(Constants, IdentifyingSpell_NegativeReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_identifying_spells(-1), "");
}

// --- is_ignorable_event ---

TEST(Constants, IgnorableEvent_Known)
{
    EXPECT_TRUE(Constants::is_ignorable_event("SPELL_DAMAGE"));
    EXPECT_TRUE(Constants::is_ignorable_event("SPELL_AURA_APPLIED"));
    EXPECT_TRUE(Constants::is_ignorable_event("UNIT_DIED"));
}

TEST(Constants, IgnorableEvent_NonIgnorable)
{
    EXPECT_FALSE(Constants::is_ignorable_event("SPELL_CAST_SUCCESS"));
}

TEST(Constants, IgnorableEvent_EmptyString)
{
    EXPECT_FALSE(Constants::is_ignorable_event(""));
}

TEST(Constants, IgnorableEvent_CaseSensitive)
{
    EXPECT_FALSE(Constants::is_ignorable_event("spell_damage"));
    EXPECT_FALSE(Constants::is_ignorable_event("Spell_Damage"));
}

// --- get_interrupt_id / get_interrupt_cd ---

TEST(Constants, InterruptId_Warrior)
{
    EXPECT_EQ(Constants::get_interrupt_id("Warrior"), 6552);
}

TEST(Constants, InterruptCd_Warrior)
{
    EXPECT_EQ(Constants::get_interrupt_cd("Warrior"), std::chrono::seconds { 15 });
}

TEST(Constants, InterruptId_Shaman)
{
    EXPECT_EQ(Constants::get_interrupt_id("Shaman"), 57994);
}

TEST(Constants, InterruptCd_Shaman)
{
    EXPECT_EQ(Constants::get_interrupt_cd("Shaman"), std::chrono::seconds { 12 });
}

TEST(Constants, InterruptId_Mage)
{
    EXPECT_EQ(Constants::get_interrupt_id("Mage"), 2139);
}

TEST(Constants, InterruptCd_Mage)
{
    EXPECT_EQ(Constants::get_interrupt_cd("Mage"), std::chrono::seconds { 24 });
}

TEST(Constants, InterruptId_Druid_ReturnsFirst)
{
    EXPECT_EQ(Constants::get_interrupt_id("Druid"), 106839);
    EXPECT_EQ(Constants::get_interrupt_cd("Druid"), std::chrono::seconds { 15 });
}

TEST(Constants, InterruptId_UnknownReturnsZero)
{
    EXPECT_EQ(Constants::get_interrupt_id("UnknownClass"), 0);
}

TEST(Constants, InterruptCd_UnknownReturnsZero)
{
    EXPECT_EQ(Constants::get_interrupt_cd("UnknownClass"), std::chrono::seconds { 0 });
}

// --- is_battle_rez ---

TEST(Constants, BattleRez_AllFiveIds)
{
    EXPECT_TRUE(Constants::is_battle_rez(10609));
    EXPECT_TRUE(Constants::is_battle_rez(376999));
    EXPECT_TRUE(Constants::is_battle_rez(20707));
    EXPECT_TRUE(Constants::is_battle_rez(61999));
    EXPECT_TRUE(Constants::is_battle_rez(407133));
}

TEST(Constants, BattleRez_UnknownReturnsFalse)
{
    EXPECT_FALSE(Constants::is_battle_rez(0));
    EXPECT_FALSE(Constants::is_battle_rez(999999));
}

// --- is_tracked_enemy / enemy_data ---

TEST(Constants, TrackedEnemy_KnownNPC)
{
    EXPECT_TRUE(Constants::is_tracked_enemy("216293"));
}

TEST(Constants, TrackedEnemy_UnknownReturnsFalse)
{
    EXPECT_FALSE(Constants::is_tracked_enemy("000000"));
}

TEST(Constants, TrackedEnemy_EmptyReturnsFalse)
{
    EXPECT_FALSE(Constants::is_tracked_enemy(""));
}

TEST(Constants, EnemyData_TableIsValid)
{
    static_assert(Constants::enemy_data.size() == 54, "enemy_data row count");

    std::set<std::pair<std::string_view, int>> seen_pairs;
    std::map<Constants::Mechanic, int> mechanic_counts;
    for (const auto& row : Constants::enemy_data) {
        EXPECT_GT(row.cooldown.count(), 0);
        EXPECT_GE(row.first_cast.count(), 0);
        EXPECT_FALSE(row.callout.empty());
        EXPECT_TRUE(seen_pairs.emplace(row.enemy_id, row.spell_id).second)
            << row.enemy_id << "/" << row.spell_id;
        ++mechanic_counts[row.mechanic];
    }

    EXPECT_EQ(mechanic_counts.size(), 6u);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::Kick], 0);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::Stun], 0);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::Dispel], 0);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::TankHit], 0);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::Movement], 0);
    EXPECT_GT(mechanic_counts[Constants::Mechanic::Awareness], 0);
}

TEST(Constants, EnemyData_KickRowsMatchPreviousInterruptableRows)
{
    int kick_rows = 0;
    for (const auto& row : Constants::enemy_data) {
        if (row.mechanic == Constants::Mechanic::Kick) {
            ++kick_rows;
        }
    }
    EXPECT_EQ(kick_rows, 14);
}

TEST(Constants, EnemyData_MechanicMapping)
{
    const auto mechanic_for = [](std::string_view enemy_id, int spell_id) {
        for (const auto& row : Constants::enemy_data) {
            if (row.enemy_id == enemy_id && row.spell_id == spell_id) {
                return std::optional<Constants::Mechanic> { row.mechanic };
            }
        }
        return std::optional<Constants::Mechanic> { };
    };

    EXPECT_EQ(mechanic_for("210269", 463218), Constants::Mechanic::Kick);
    EXPECT_EQ(mechanic_for("164557", 326409), Constants::Mechanic::Stun);
    EXPECT_EQ(mechanic_for("234957", 1221483), Constants::Mechanic::Dispel);
    EXPECT_EQ(mechanic_for("242631", 1235368), Constants::Mechanic::TankHit);
    EXPECT_EQ(mechanic_for("236995", 1226111), Constants::Mechanic::Movement);
    EXPECT_EQ(mechanic_for("214761", 431364), Constants::Mechanic::Awareness);
    EXPECT_FALSE(mechanic_for("000000", 1).has_value());
}

// --- is_interrupt / is_crowd_control ---

TEST(Constants, IsInterrupt_KnownId)
{
    EXPECT_TRUE(Constants::is_interrupt(6552)); // Pummel (Warrior)
    EXPECT_TRUE(Constants::is_interrupt(57994)); // Wind Shear (Shaman)
}

TEST(Constants, IsInterrupt_UnknownReturnsFalse)
{
    EXPECT_FALSE(Constants::is_interrupt(999999));
}

TEST(Constants, IsCrowdControl_KnownId)
{
    EXPECT_TRUE(Constants::is_crowd_control(179057)); // Chaos Nova (DH)
    EXPECT_TRUE(Constants::is_crowd_control(119381)); // Leg Sweep (Monk)
}

TEST(Constants, IsCrowdControl_UnknownReturnsFalse)
{
    EXPECT_FALSE(Constants::is_crowd_control(999999));
}

TEST(Constants, InterruptIsNotCC)
{
    // Pummel is an interrupt, not a CC
    EXPECT_TRUE(Constants::is_interrupt(6552));
    EXPECT_FALSE(Constants::is_crowd_control(6552));
}

TEST(Constants, CCIsNotInterrupt)
{
    // Chaos Nova is CC, not an interrupt
    EXPECT_TRUE(Constants::is_crowd_control(179057));
    EXPECT_FALSE(Constants::is_interrupt(179057));
}

// --- get_class_from_spec / get_spec_name ---

TEST(Constants, SpecClass_FixturePlayers)
{
    EXPECT_EQ(Constants::get_class_from_spec(65), "Paladin");
    EXPECT_EQ(Constants::get_class_from_spec(267), "Warlock");
    EXPECT_EQ(Constants::get_class_from_spec(252), "Death Knight");
    EXPECT_EQ(Constants::get_class_from_spec(262), "Shaman");
    EXPECT_EQ(Constants::get_class_from_spec(73), "Warrior");
}

TEST(Constants, SpecName_FixturePlayers)
{
    EXPECT_EQ(Constants::get_spec_name(65), "Holy Paladin");
    EXPECT_EQ(Constants::get_spec_name(267), "Destruction Warlock");
    EXPECT_EQ(Constants::get_spec_name(252), "Unholy Death Knight");
    EXPECT_EQ(Constants::get_spec_name(262), "Elemental Shaman");
    EXPECT_EQ(Constants::get_spec_name(73), "Protection Warrior");
}

TEST(Constants, Spec_UnknownReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_spec(999999), "");
    EXPECT_EQ(Constants::get_class_from_spec(0), "");
    EXPECT_EQ(Constants::get_spec_name(999999), "");
    EXPECT_EQ(Constants::get_spec_name(0), "");
}

// --- get_class_from_interrupt_spell / get_class_from_cc_spell ---

TEST(Constants, ClassFromInterruptSpell_Known)
{
    EXPECT_EQ(Constants::get_class_from_interrupt_spell(6552), "Warrior");
    EXPECT_EQ(Constants::get_class_from_interrupt_spell(57994), "Shaman");
}

TEST(Constants, ClassFromInterruptSpell_UnknownReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_interrupt_spell(999999), "");
    EXPECT_EQ(Constants::get_class_from_interrupt_spell(0), "");
}

TEST(Constants, ClassFromCcSpell_Known)
{
    EXPECT_EQ(Constants::get_class_from_cc_spell(46968), "Warrior");
    EXPECT_EQ(Constants::get_class_from_cc_spell(179057), "Demon Hunter");
}

TEST(Constants, ClassFromCcSpell_UnknownReturnsEmpty)
{
    EXPECT_EQ(Constants::get_class_from_cc_spell(999999), "");
    EXPECT_EQ(Constants::get_class_from_cc_spell(0), "");
}

// --- per-spell ability lookups ---

TEST(Constants, InterruptLookups)
{
    EXPECT_EQ(Constants::get_interrupt_name(6552), "Pummel");
    EXPECT_EQ(Constants::get_interrupt_cooldown(6552), std::chrono::seconds { 15 });
    EXPECT_EQ(Constants::get_interrupt_name(78675), "Solar Beam");
    EXPECT_EQ(Constants::get_interrupt_cooldown(78675), std::chrono::seconds { 60 });
    EXPECT_EQ(Constants::get_interrupt_name(999999), "");
    EXPECT_EQ(Constants::get_interrupt_cooldown(999999), std::chrono::seconds { 0 });
}

TEST(Constants, CrowdControlLookups)
{
    EXPECT_EQ(Constants::get_crowd_control_name(122), "Frost Nova");
    EXPECT_EQ(Constants::get_crowd_control_cooldown(122), std::chrono::seconds { 30 });
    EXPECT_EQ(Constants::get_crowd_control_name(999999), "");
    EXPECT_EQ(Constants::get_crowd_control_cooldown(999999), std::chrono::seconds { 0 });
}
