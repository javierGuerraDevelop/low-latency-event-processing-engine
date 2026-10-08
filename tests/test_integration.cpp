#include <gtest/gtest.h>

#include <optional>
#include <string>
#include <string_view>

#include "constants.h"
#include "engine.h"
#include "parser.h"

namespace ch = std::chrono;

namespace {

// Literal records from text_files/combat_log_large.txt (Cinderbrew Meadery).
constexpr const char* CHALLENGE_MODE_START_LINE
    = R"(6/14/2025 18:03:13.665-4  CHALLENGE_MODE_START,"Cinderbrew Meadery",2661,506,13,[9,10,147])";
constexpr const char* SKYFURY_LINE
    = R"(6/14/2025 18:02:53.252-4  SPELL_CAST_SUCCESS,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,462854,"Skyfury",0x8,Player-11-0E99A7E4,0000000000000000,10876976,10876976,27212,99534,89207,679,300,0,0,2365840,2500000,25000,2650.96,-4852.19,2335,4.7281,679)";
constexpr const char* COMBATANT_INFO_PALADIN_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-3676-0CD71E8D,0,18349,6177,527978,103877,0,0,0,17455,17455,17455,250,0,19997,19997,19997,3163,6188,2821,2821,2821,131649,65,[(81496,102465,1)])";
constexpr const char* COMBATANT_INFO_DK_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-1427-0860A3DB,0,78465,14644,556659,9528,0,0,0,4029,4029,4029,0,2040,26272,26272,26272,545,22003,4688,4688,4688,74678,252,[(76044,96172,1)])";
constexpr const char* COMBATANT_INFO_WARLOCK_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-76-0BED3564,1,6704,13590,523669,97197,0,0,0,11793,11793,11793,0,0,18991,18991,18991,1635,13693,5141,5141,5141,39210,267,[(71939,91427,1)])";
constexpr const char* COMBATANT_INFO_WARRIOR_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-1427-0E1E8D11,0,80740,12173,770240,11999,0,15255,0,15255,15255,15255,783,3348,22174,22174,22174,1090,3500,9001,9001,9001,194691,73,[(90450,112110,1)])";
constexpr const char* COMBATANT_INFO_SHAMAN_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-11-0E99A7E4,1,7590,17645,494408,99534,0,0,0,6172,6172,6172,0,0,17353,17353,17353,1635,20821,5300,5300,5300,89207,262,[(80981,101849,1)])";
constexpr const char* HIRED_MUSCLE_CAST_LINE
    = R"(6/14/2025 18:03:36.479-4  SPELL_CAST_START,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0x10a48,0x80,0000000000000000,nil,0x80000000,0x80000000,463218,"Volatile Keg",0x4)";
constexpr const char* HIRED_MUSCLE_DAMAGE_LINE
    = R"(6/14/2025 18:03:38.492-4  SPELL_DAMAGE,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0x10a48,0x80,Player-3676-0CD71E8D,"Bigchalupa-Area52-US",0x511,0x0,463218,"Volatile Keg",0x4,Player-3676-0CD71E8D,0000000000000000,7384075,10559560,108032,103877,131649,934,581,0,0,2416820,2500000,0,2620.45,-4892.46,2335,4.9466,680,3175485,3899633,-1,4,0,0,0,nil,nil,nil,AOE)";
constexpr const char* SECOND_HIRED_MUSCLE_CAST_LINE
    = R"(6/14/2025 18:03:42.577-4  SPELL_CAST_START,Creature-0-4218-2661-9671-210269-00014DF1A1,"Hired Muscle",0xa48,0x0,0000000000000000,nil,0x80000000,0x80000000,463218,"Volatile Keg",0x4)";
constexpr const char* HIRED_MUSCLE_DEATH_LINE
    = R"(6/14/2025 18:03:53.151-4  UNIT_DIED,0000000000000000,nil,0x80000000,0x80000000,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0xa48,0x80,0)";
constexpr const char* PLAYER_DEATH_LINE
    = R"(6/14/2025 18:23:23.553-4  UNIT_DIED,0000000000000000,nil,0x80000000,0x80000000,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,0)";

// Looks up the first cast delay for a tracked ability so tests do not depend
// on hard-coded enemy timings.
std::optional<ch::milliseconds> first_cast_for(std::string_view enemy_id, int spell_id)
{
    for (const auto& entry : Constants::enemy_data) {
        if (entry.enemy_id == enemy_id && entry.spell_id == spell_id) {
            return entry.first_cast;
        }
    }
    return std::nullopt;
}

} // namespace

TEST(Integration, EnemyCast_QueuesCall)
{
    ShotCallEngine engine;
    const auto challenge = parse_line(CHALLENGE_MODE_START_LINE);
    const auto cast      = parse_line(HIRED_MUSCLE_CAST_LINE);
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(cast.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*challenge);
    engine.handle_event(*cast);

    std::string callout;
    engine.set_shotcall_callback([&callout](const ScheduledShotCall&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(cast->time_stamp + *first_cast), 1u);
    EXPECT_FALSE(callout.empty());
}

TEST(Integration, EnemyDeath_PurgesQueuedCalls)
{
    ShotCallEngine engine;
    const auto challenge = parse_line(CHALLENGE_MODE_START_LINE);
    const auto cast      = parse_line(HIRED_MUSCLE_CAST_LINE);
    const auto death     = parse_line(HIRED_MUSCLE_DEATH_LINE);
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(cast.has_value());
    ASSERT_TRUE(death.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*challenge);
    engine.handle_event(*cast);
    engine.handle_event(*death);

    int callbacks = 0;
    engine.set_shotcall_callback([&callbacks](const ScheduledShotCall&, const std::string&) {
        ++callbacks;
    });

    EXPECT_EQ(engine.dispatch_due(cast->time_stamp + *first_cast), 0u);
    EXPECT_EQ(callbacks, 0);
}

TEST(Integration, PlayerDeath_MakesPlayerUnselectable)
{
    ShotCallEngine engine;
    const auto skyfury     = parse_line(SKYFURY_LINE);
    const auto challenge   = parse_line(CHALLENGE_MODE_START_LINE);
    const auto combatant   = parse_line(COMBATANT_INFO_SHAMAN_LINE);
    const auto first_cast  = parse_line(HIRED_MUSCLE_CAST_LINE);
    const auto second_cast = parse_line(SECOND_HIRED_MUSCLE_CAST_LINE);
    const auto death       = parse_line(PLAYER_DEATH_LINE);
    ASSERT_TRUE(skyfury.has_value());
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(combatant.has_value());
    ASSERT_TRUE(first_cast.has_value());
    ASSERT_TRUE(second_cast.has_value());
    ASSERT_TRUE(death.has_value());
    const auto first_cast_delay = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast_delay.has_value());

    engine.handle_event(*skyfury);
    engine.handle_event(*challenge);
    engine.handle_event(*combatant);
    engine.handle_event(*first_cast);

    std::string callout;
    engine.set_shotcall_callback([&callout](const ScheduledShotCall&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(first_cast->time_stamp + *first_cast_delay), 1u);
    EXPECT_NE(callout.find("Lilrawb"), std::string::npos);

    engine.handle_event(*death);
    engine.handle_event(*second_cast);

    EXPECT_EQ(engine.dispatch_due(second_cast->time_stamp + *first_cast_delay), 1u);
    EXPECT_NE(callout.find("this one is going off"), std::string::npos);
}

TEST(Integration, CombatantInfoMakesPlayerAssignableWithoutAction)
{
    ShotCallEngine engine;
    const auto challenge      = parse_line(CHALLENGE_MODE_START_LINE);
    const auto combatant_info = parse_line(COMBATANT_INFO_PALADIN_LINE);
    const auto cast           = parse_line(HIRED_MUSCLE_CAST_LINE);
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(combatant_info.has_value());
    ASSERT_TRUE(cast.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    EXPECT_EQ(combatant_info->source_id, "Player-3676-0CD71E8D");
    EXPECT_EQ(combatant_info->spec_id, 65);

    engine.handle_event(*challenge);
    engine.handle_event(*combatant_info);
    engine.handle_event(*cast);

    std::string callout;
    engine.set_shotcall_callback([&callout](const ScheduledShotCall&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(cast->time_stamp + *first_cast), 1u);
    // The Paladin is a valid assignee, so the fallback phrase must not appear.
    // The name is still unknown at this point and must not be invented.
    EXPECT_EQ(callout.find("this one is going off"), std::string::npos);
    EXPECT_EQ(callout.find("Bigchalupa"), std::string::npos);

    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.roster_known);
    EXPECT_EQ(status.identified, 1);
    EXPECT_EQ(status.expected, 1);
}

TEST(Integration, NameLearnedFromEnemyEventTargetingPlayer)
{
    ShotCallEngine engine;
    const auto challenge      = parse_line(CHALLENGE_MODE_START_LINE);
    const auto combatant_info = parse_line(COMBATANT_INFO_PALADIN_LINE);
    const auto damage         = parse_line(HIRED_MUSCLE_DAMAGE_LINE);
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(combatant_info.has_value());
    ASSERT_TRUE(damage.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*challenge);
    engine.handle_event(*combatant_info);
    engine.handle_event(*damage);

    std::string callout;
    engine.set_shotcall_callback([&callout](const ScheduledShotCall&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(damage->time_stamp + *first_cast), 1u);
    EXPECT_NE(callout.find("Bigchalupa"), std::string::npos);
}

TEST(Integration, ChallengeRosterSnapshotCompletes)
{
    ShotCallEngine engine;
    const auto challenge    = parse_line(CHALLENGE_MODE_START_LINE);
    const auto paladin      = parse_line(COMBATANT_INFO_PALADIN_LINE);
    const auto death_knight = parse_line(COMBATANT_INFO_DK_LINE);
    const auto warlock      = parse_line(COMBATANT_INFO_WARLOCK_LINE);
    const auto warrior      = parse_line(COMBATANT_INFO_WARRIOR_LINE);
    const auto shaman       = parse_line(COMBATANT_INFO_SHAMAN_LINE);
    const auto cast         = parse_line(HIRED_MUSCLE_CAST_LINE);
    ASSERT_TRUE(challenge.has_value());
    ASSERT_TRUE(paladin.has_value());
    ASSERT_TRUE(death_knight.has_value());
    ASSERT_TRUE(warlock.has_value());
    ASSERT_TRUE(warrior.has_value());
    ASSERT_TRUE(shaman.has_value());
    ASSERT_TRUE(cast.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*challenge);
    engine.handle_event(*paladin);
    engine.handle_event(*death_knight);
    engine.handle_event(*warlock);
    engine.handle_event(*warrior);
    engine.handle_event(*shaman);
    engine.handle_event(*cast);

    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.roster_known);
    EXPECT_TRUE(status.in_run);
    EXPECT_EQ(status.identified, 5);
    EXPECT_EQ(status.expected, 5);

    EXPECT_EQ(engine.dispatch_due(cast->time_stamp + *first_cast), 1u);
}
