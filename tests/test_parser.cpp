#include <gtest/gtest.h>

#include <string>

#include "parser.h"

namespace ch = std::chrono;

namespace {

// Literal records from text_files/combat_log_large.txt (Cinderbrew Meadery).
constexpr const char* COMBAT_LOG_VERSION_LINE
    = R"(6/14/2025 18:01:13.780-4  COMBAT_LOG_VERSION,22,ADVANCED_LOG_ENABLED,1,BUILD_VERSION,11.1.5,PROJECT_ID,1)";
constexpr const char* ZONE_CHANGE_LINE
    = R"(6/14/2025 18:01:13.780-4  ZONE_CHANGE,2661,"Cinderbrew Meadery",23)";
constexpr const char* CHALLENGE_MODE_START_LINE
    = R"(6/14/2025 18:03:13.665-4  CHALLENGE_MODE_START,"Cinderbrew Meadery",2661,506,13,[9,10,147])";
constexpr const char* COMBATANT_INFO_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-3676-0CD71E8D,0,18349,6177,527978,103877,0,0,0,17455,17455,17455,250,0,19997,19997,19997,3163,6188,2821,2821,2821,131649,65,[(81496,102465,1)])";
constexpr const char* SWING_MISSED_LINE
    = R"(6/14/2025 18:03:29.156-4  SWING_MISSED,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0xa48,0x80,Player-1427-0E1E8D11,"Orçaos-Ragnaros-US",0x512,0x20,PARRY,nil)";
constexpr const char* SPELL_CAST_START_LINE
    = R"(6/14/2025 18:03:36.479-4  SPELL_CAST_START,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0x10a48,0x80,0000000000000000,nil,0x80000000,0x80000000,463218,"Volatile Keg",0x4)";
constexpr const char* SPELL_CAST_SUCCESS_LINE
    = R"(6/14/2025 18:03:38.470-4  SPELL_CAST_SUCCESS,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0x10a48,0x80,0000000000000000,nil,0x80000000,0x80000000,463218,"Volatile Keg",0x4,Creature-0-4218-2661-9671-210269-0000CDF1A1,0000000000000000,164906335,208271916,0,0,42857,0,0,0,1,0,0,0,2622.50,-4897.19,2335,2.5457,81)";
constexpr const char* CREATURE_DEATH_LINE
    = R"(6/14/2025 18:03:38.808-4  UNIT_DIED,0000000000000000,nil,0x80000000,0x80000000,Creature-0-4218-2661-9671-217126-0001CDF1A1,"Over-Indulged Patron",0xa28,0x0,0)";
constexpr const char* SPELL_INTERRUPT_LINE
    = R"(6/14/2025 18:04:18.671-4  SPELL_INTERRUPT,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,Creature-0-4218-2661-9671-218671-00024DF1A1,"Venture Co. Pyromaniac",0x30a48,0x2,57994,"Wind Shear",0x8,437721,"Boiling Flames",4)";
constexpr const char* ENCOUNTER_START_LINE
    = R"(6/14/2025 18:05:58.129-4  ENCOUNTER_START,2900,"Brewmaster Aldryr",8,5,2661)";
constexpr const char* ENCOUNTER_END_LINE
    = R"(6/14/2025 18:09:13.594-4  ENCOUNTER_END,2900,"Brewmaster Aldryr",8,5,1,195486)";
constexpr const char* PLAYER_DEATH_LINE
    = R"(6/14/2025 18:23:23.553-4  UNIT_DIED,0000000000000000,nil,0x80000000,0x80000000,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,0)";

// Builds a raw combat-log line matching the WoW format.
std::string make_line(const std::string& timestamp, const std::string& event_type,
    const std::string& source_guid, const std::string& source_name, const std::string& source_flags,
    const std::string& target_guid, int spell_id, const std::string& spell_name = "\"Spell\"")
{
    return timestamp + "  " + event_type + "," + source_guid + "," + source_name + "," + source_flags
        + ",0x0," + target_guid + ",\"Target\",0x0,0x0," + std::to_string(spell_id) + ","
        + spell_name;
}

ch::system_clock::time_point utc_time(int year, ch::month month, int day, ch::hours hour,
    ch::minutes minute, ch::seconds second, ch::milliseconds millisecond = ch::milliseconds { 0 })
{
    return ch::system_clock::time_point { ch::sys_days { ch::year { year } / month
               / ch::day { static_cast<unsigned>(day) } } }
    + hour + minute + second + millisecond;
}

} // namespace

// ==================== Common prefix and CSV handling ====================

TEST(Parser, StandardSpellCastSuccess)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Healbot\"", "0x511", "Creature-0-0-0-0-216293-0", 6673,
        "\"Battle Shout\"");

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "SPELL_CAST_SUCCESS");
    EXPECT_EQ(event->name, "Healbot");
    EXPECT_EQ(event->source_id, "Player-1-ABC");
    EXPECT_EQ(event->target_id, "Creature-0-0-0-0-216293-0");
    EXPECT_EQ(event->source_flags, "0x511");
    EXPECT_EQ(event->spell_id, 6673);
}

TEST(Parser, CreatureSourceGUID_ExtractsNpcId)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Creature-0-0-0-0-216293-0", "\"Mob\"", "0xa48", "Player-1-ABC", 434793, "\"AoE Barrage\"");

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->npc_id, "216293");
}

TEST(Parser, PlayerSourceGUID_NpcIdEmpty)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Healbot\"", "0x511", "Creature-0-0-0-0-216293-0", 6673, "\"Battle Shout\"");

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->npc_id, "");
}

TEST(Parser, NameWithRealm_StripsRealm)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Healbot-Proudmoore\"", "0x511", "Player-1-DEF", 1);

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->name, "Healbot");
}

TEST(Parser, NameWithoutRealm)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Soloname\"", "0x511", "Player-1-DEF", 1);

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->name, "Soloname");
}

TEST(Parser, QuotedCommaInName_KeepsLaterFieldsAligned)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Smith, Agent\"", "0x511", "Player-1-DEF", 12345);

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->name, "Smith, Agent");
    EXPECT_EQ(event->source_flags, "0x511");
    EXPECT_EQ(event->target_id, "Player-1-DEF");
    EXPECT_EQ(event->spell_id, 12345);
}

TEST(Parser, DoubledQuoteInName_IsUnescaped)
{
    const std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS",
        "Player-1-ABC", "\"Bob \"\"The\"\" Builder\"", "0x511", "Player-1-DEF", 1);

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->name, "Bob \"The\" Builder");
}

TEST(Parser, EmptyLine_ReturnsNullopt)
{
    EXPECT_FALSE(parse_line("").has_value());
}

TEST(Parser, TooFewFields_ReturnsNullopt)
{
    EXPECT_FALSE(parse_line("a,b,c,d,e").has_value());
}

TEST(Parser, NonNumericSpellId_DefaultsToZero)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"P\"", "0x511", "Player-1-DEF", 0);
    const auto pos   = line.rfind(",0,");
    line.replace(pos + 1, 1, "notanumber");

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->spell_id, 0);
}

// ==================== Real record layouts ====================

TEST(Parser, UnitDiedCreatureRecord_HasTenFields)
{
    const auto event = parse_line(CREATURE_DEATH_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "UNIT_DIED");
    EXPECT_EQ(event->target_id, "Creature-0-4218-2661-9671-217126-0001CDF1A1");
    EXPECT_EQ(event->spell_id, 0);
}

TEST(Parser, UnitDiedPlayerRecord_HasTenFields)
{
    const auto event = parse_line(PLAYER_DEATH_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "UNIT_DIED");
    EXPECT_EQ(event->target_id, "Player-11-0E99A7E4");
    EXPECT_EQ(event->target_name, "Lilrawb");
}

TEST(Parser, SpellCastStart_463218)
{
    const auto event = parse_line(SPELL_CAST_START_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "SPELL_CAST_START");
    EXPECT_EQ(event->name, "Hired Muscle");
    EXPECT_EQ(event->npc_id, "210269");
    EXPECT_EQ(event->spell_id, 463218);
}

TEST(Parser, SpellCastSuccess_463218)
{
    const auto event = parse_line(SPELL_CAST_SUCCESS_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "SPELL_CAST_SUCCESS");
    EXPECT_EQ(event->spell_id, 463218);
}

TEST(Parser, SwingMissed_ParryYieldsZeroSpell)
{
    const auto event = parse_line(SWING_MISSED_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "SWING_MISSED");
    EXPECT_EQ(event->npc_id, "210269");
    EXPECT_EQ(event->spell_id, 0);
}

TEST(Parser, SpellInterrupt_RecordsInterruptedSpell)
{
    const auto event = parse_line(SPELL_INTERRUPT_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "SPELL_INTERRUPT");
    EXPECT_EQ(event->spell_id, 57994);
    EXPECT_EQ(event->interrupted_spell_id, 437721);
}

// ==================== Boundary and metadata records ====================

TEST(Parser, ChallengeModeStart_InstanceAndKeystone)
{
    const auto event = parse_line(CHALLENGE_MODE_START_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "CHALLENGE_MODE_START");
    EXPECT_EQ(event->instance_id, 2661);
    EXPECT_EQ(event->keystone_level, 13);
}

TEST(Parser, EncounterStart_EncounterGroupAndInstance)
{
    const auto event = parse_line(ENCOUNTER_START_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "ENCOUNTER_START");
    EXPECT_EQ(event->encounter_id, 2900);
    EXPECT_EQ(event->group_size, 5);
    EXPECT_EQ(event->instance_id, 2661);
}

TEST(Parser, EncounterEnd_EncounterAndGroup)
{
    const auto event = parse_line(ENCOUNTER_END_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "ENCOUNTER_END");
    EXPECT_EQ(event->encounter_id, 2900);
    EXPECT_EQ(event->group_size, 5);
}

TEST(Parser, ZoneChange_InstanceId)
{
    const auto event = parse_line(ZONE_CHANGE_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "ZONE_CHANGE");
    EXPECT_EQ(event->instance_id, 2661);
}

TEST(Parser, CombatLogVersion_AdvancedLoggingEnabled)
{
    const auto event = parse_line(COMBAT_LOG_VERSION_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "COMBAT_LOG_VERSION");
    EXPECT_TRUE(event->advanced_logging);
}

// ==================== COMBATANT_INFO ====================

TEST(Parser, CombatantInfo_AnchorExtractsSpec)
{
    const auto event = parse_line(COMBATANT_INFO_LINE);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->event_type, "COMBATANT_INFO");
    EXPECT_EQ(event->source_id, "Player-3676-0CD71E8D");
    EXPECT_EQ(event->name, "");
    EXPECT_EQ(event->spec_id, 65);
}

TEST(Parser, CombatantInfo_ExtraFieldBeforeTalentArray)
{
    std::string line    = COMBATANT_INFO_LINE;
    const auto spec_pos = line.find(",65,[(");
    ASSERT_NE(spec_pos, std::string::npos);
    line.insert(spec_pos + 1, "999,");

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->spec_id, 65);
}

TEST(Parser, CombatantInfo_FallsBackToField24)
{
    std::string line = "1/15/2025 20:30:45.000+0  COMBATANT_INFO,Player-1-AAA";
    for (int i = 0; i < 22; ++i) {
        line += ",0";
    }
    line += ",73";

    const auto event = parse_line(line);
    ASSERT_TRUE(event.has_value());
    EXPECT_EQ(event->spec_id, 73);
}

// ==================== Timestamps ====================

TEST(Parser, Timestamp_NegativeOffset)
{
    const auto parsed = parse_timestamp("6/14/2025 18:03:38.808-4");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed,
        utc_time(2025, ch::June, 14, ch::hours { 22 }, ch::minutes { 3 }, ch::seconds { 38 },
            ch::milliseconds { 808 }));
}

TEST(Parser, Timestamp_PositiveOffset)
{
    const auto parsed = parse_timestamp("1/15/2025 20:30:45.000+2");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed,
        utc_time(2025, ch::January, 15, ch::hours { 18 }, ch::minutes { 30 }, ch::seconds { 45 }));
}

TEST(Parser, Timestamp_NegativeFiveOffset)
{
    const auto parsed = parse_timestamp("1/15/2025 20:30:45.000-5");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed,
        utc_time(2025, ch::January, 16, ch::hours { 1 }, ch::minutes { 30 }, ch::seconds { 45 }));
}

TEST(Parser, Timestamp_HalfHourOffset)
{
    const auto parsed = parse_timestamp("6/14/2025 18:03:38.808+5.5");
    ASSERT_TRUE(parsed.has_value());
    EXPECT_EQ(*parsed,
        utc_time(2025, ch::June, 14, ch::hours { 12 }, ch::minutes { 33 }, ch::seconds { 38 },
            ch::milliseconds { 808 }));
}

TEST(Parser, Timestamp_OffsetRangeBounds)
{
    const auto minus_twelve = parse_timestamp("6/14/2025 18:03:38.808-12");
    ASSERT_TRUE(minus_twelve.has_value());
    EXPECT_EQ(*minus_twelve,
        utc_time(2025, ch::June, 15, ch::hours { 6 }, ch::minutes { 3 }, ch::seconds { 38 },
            ch::milliseconds { 808 }));

    const auto plus_fourteen = parse_timestamp("6/14/2025 18:03:38.808+14");
    ASSERT_TRUE(plus_fourteen.has_value());
    EXPECT_EQ(*plus_fourteen,
        utc_time(2025, ch::June, 14, ch::hours { 4 }, ch::minutes { 3 }, ch::seconds { 38 },
            ch::milliseconds { 808 }));

    EXPECT_FALSE(parse_timestamp("6/14/2025 18:03:38.808-13").has_value());
    EXPECT_FALSE(parse_timestamp("6/14/2025 18:03:38.808+15").has_value());
}

TEST(Parser, Timestamp_MalformedInput)
{
    EXPECT_FALSE(parse_timestamp("").has_value());
    EXPECT_FALSE(parse_timestamp("not a timestamp").has_value());
    EXPECT_FALSE(parse_timestamp("6/14/2025 18:03:38.808").has_value());
    EXPECT_FALSE(parse_timestamp("6/14/2025 18:03:38+0").has_value());
    EXPECT_FALSE(parse_timestamp("2/30/2025 18:03:38.808+0").has_value());
    EXPECT_FALSE(parse_timestamp("13/14/2025 18:03:38.808+0").has_value());
    EXPECT_FALSE(parse_timestamp("6/14/2025 25:03:38.808+0").has_value());
    EXPECT_FALSE(parse_timestamp("6/14/2025 18:60:38.808+0").has_value());
}
