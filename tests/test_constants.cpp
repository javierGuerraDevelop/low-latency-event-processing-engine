#include <gtest/gtest.h>

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

// --- is_tracked_enemy / is_enemy_ccable ---

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

TEST(Constants, EnemyCcable_KnownNPC)
{
    EXPECT_TRUE(Constants::is_enemy_ccable("216293"));
}

TEST(Constants, EnemyCcable_UnknownReturnsFalse)
{
    EXPECT_FALSE(Constants::is_enemy_ccable("000000"));
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
