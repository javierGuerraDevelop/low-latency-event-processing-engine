#include <gtest/gtest.h>

#include "parser.h"

namespace ch = std::chrono;

// Helper to build raw combat log lines matching WoW format.
// Format: "MM/DD/YYYY HH:MM:SS.mmm+TZ
// EVENT,sourceGUID,sourceName,sourceFlags,...,targetGUID,...,...,...,spellID,spellName,..."
std::string make_line(const std::string& timestamp, const std::string& event_type,
    const std::string& source_guid, const std::string& source_name,
    const std::string& source_flags, const std::string& target_guid, int spell_id,
    const std::string& spell_name)
{
    // Fields: [0]=timestamp+event, [1]=sourceGUID, [2]=sourceName, [3]=sourceFlags,
    //         [4]=sourceRaidFlags, [5]=targetGUID, [6]=targetName, [7]=targetFlags,
    //         [8]=targetRaidFlags, [9]=spellID, [10]=spellName
    return timestamp + "  " + event_type + "," + source_guid + "," + source_name + "," + source_flags + ",0x0," + target_guid + ",\"Target\",0x0,0x0," + std::to_string(spell_id) + "," + spell_name;
}

TEST(Parser, StandardSpellCastSuccess)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"Healbot\"",
        "0x511", "Creature-0-0-0-0-216293-0", 6673, "\"Battle Shout\"");
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.event_type, "SPELL_CAST_SUCCESS");
    EXPECT_EQ(ev.name, "Healbot");
    EXPECT_EQ(ev.source_id, "Player-1-ABC");
    EXPECT_EQ(ev.target_id, "Creature-0-0-0-0-216293-0");
    EXPECT_EQ(ev.source_raid_flag, "0x511");
    EXPECT_EQ(ev.spell_id, 6673);
    EXPECT_EQ(ev.spell_name, "\"Battle Shout\"");
}

TEST(Parser, CreatureSourceGUID_ExtractsNpcId)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-0",
        "\"Mob\"", "0xa48", "Player-1-ABC", 434793, "\"AoE Barrage\"");
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.npc_id, "216293");
}

TEST(Parser, PlayerSourceGUID_NpcIdEmpty)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"Healbot\"",
        "0x511", "Creature-0-0-0-0-216293-0", 6673, "\"Battle Shout\"");
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.npc_id, "");
}

TEST(Parser, TimestampOrdering)
{
    std::string line1 = make_line("1/15/2025 20:30:45.100+0", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"P\"", "0x511", "Player-1-DEF", 1, "\"S\"");
    std::string line2 = make_line("1/15/2025 20:30:46.200+0", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"P\"", "0x511", "Player-1-DEF", 1, "\"S\"");
    CombatEvent ev1   = parse_line(line1);
    CombatEvent ev2   = parse_line(line2);
    EXPECT_LT(ev1.time_stamp, ev2.time_stamp);
}

TEST(Parser, TimezoneOffset_Positive)
{
    std::string line_plus2 = make_line("1/15/2025 20:30:45.000+2", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"P\"",
        "0x511", "Player-1-DEF", 1, "\"S\"");
    std::string line_plus0 = make_line("1/15/2025 20:30:45.000+0", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"P\"",
        "0x511", "Player-1-DEF", 1, "\"S\"");
    CombatEvent ev_plus2   = parse_line(line_plus2);
    CombatEvent ev_plus0   = parse_line(line_plus0);
    // +2 offset means time is 2 hours ahead, subtracting makes it earlier in UTC
    auto diff = ev_plus0.time_stamp - ev_plus2.time_stamp;
    EXPECT_EQ(ch::duration_cast<ch::hours>(diff), ch::hours { 2 });
}

TEST(Parser, TimezoneOffset_Negative)
{
    std::string line_minus5 = make_line("1/15/2025 20:30:45.000-5", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"P\"",
        "0x511", "Player-1-DEF", 1, "\"S\"");
    std::string line_plus0  = make_line("1/15/2025 20:30:45.000+0", "SPELL_CAST_SUCCESS", "Player-1-ABC", "\"P\"",
        "0x511", "Player-1-DEF", 1, "\"S\"");
    CombatEvent ev_minus5   = parse_line(line_minus5);
    CombatEvent ev_plus0    = parse_line(line_plus0);
    auto diff               = ev_minus5.time_stamp - ev_plus0.time_stamp;
    EXPECT_EQ(ch::duration_cast<ch::hours>(diff), ch::hours { 5 });
}

TEST(Parser, NameWithRealm_StripsRealm)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"Healbot-Proudmoore\"", "0x511", "Player-1-DEF", 1, "\"S\"");
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.name, "Healbot");
}

TEST(Parser, NameWithoutRealm)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"Soloname\"", "0x511", "Player-1-DEF", 1, "\"S\"");
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.name, "Soloname");
}

TEST(Parser, EmptyLine_ReturnsDefault)
{
    CombatEvent ev = parse_line("");
    EXPECT_EQ(ev.event_type, "");
    EXPECT_EQ(ev.spell_id, 0);
}

TEST(Parser, TooFewFields_ReturnsDefault)
{
    CombatEvent ev = parse_line("a,b,c,d,e");
    EXPECT_EQ(ev.event_type, "");
    EXPECT_EQ(ev.spell_id, 0);
}

TEST(Parser, NonNumericSpellId_DefaultsToZero)
{
    std::string line = make_line("1/15/2025 20:30:45.123+2", "SPELL_CAST_SUCCESS", "Player-1-ABC",
        "\"P\"", "0x511", "Player-1-DEF", 0, "\"S\"");
    // Replace the spell_id field with a non-numeric value
    auto pos = line.rfind(",0,");
    line.replace(pos + 1, 1, "notanumber");
    CombatEvent ev = parse_line(line);
    EXPECT_EQ(ev.spell_id, 0);
}

TEST(Parser, UnitDiedEvent)
{
    // UNIT_DIED has different field layout but we still parse what we can
    std::string line = "1/15/2025 20:30:45.123+2  UNIT_DIED,0,nil,0,0,Player-1-ABC,\"Dead\",0x511,0x0,0,nil,0";
    CombatEvent ev   = parse_line(line);
    EXPECT_EQ(ev.event_type, "UNIT_DIED");
    EXPECT_EQ(ev.target_id, "Player-1-ABC");
}
