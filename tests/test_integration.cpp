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
constexpr const char* SKYFURY_LINE
    = R"(6/14/2025 18:02:53.252-4  SPELL_CAST_SUCCESS,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,Player-11-0E99A7E4,"Lilrawb-Tichondrius-US",0x512,0x0,462854,"Skyfury",0x8,Player-11-0E99A7E4,0000000000000000,10876976,10876976,27212,99534,89207,679,300,0,0,2365840,2500000,25000,2650.96,-4852.19,2335,4.7281,679)";
constexpr const char* COMBATANT_INFO_LINE
    = R"(6/14/2025 18:03:13.666-4  COMBATANT_INFO,Player-3676-0CD71E8D,0,18349,6177,527978,103877,0,0,0,17455,17455,17455,250,0,19997,19997,19997,3163,6188,2821,2821,2821,131649,65,[(81496,102465,1)])";
constexpr const char* HIRED_MUSCLE_CAST_LINE
    = R"(6/14/2025 18:03:36.479-4  SPELL_CAST_START,Creature-0-4218-2661-9671-210269-0000CDF1A1,"Hired Muscle",0x10a48,0x80,0000000000000000,nil,0x80000000,0x80000000,463218,"Volatile Keg",0x4)";
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
            return ch::milliseconds { entry.first_cast_ms };
        }
    }
    return std::nullopt;
}

} // namespace

TEST(Integration, EnemyCast_QueuesCall)
{
    ShotCallEngine engine;
    const auto cast = parse_line(HIRED_MUSCLE_CAST_LINE);
    ASSERT_TRUE(cast.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*cast);

    std::string callout;
    engine.set_shotcall_callback([&callout](const std::string&, const std::string& text) {
        callout = text;
    });

    EXPECT_TRUE(engine.dispatch_next_shotcall(cast->time_stamp + *first_cast));
    EXPECT_FALSE(callout.empty());
}

TEST(Integration, EnemyDeath_PurgesQueuedCalls)
{
    ShotCallEngine engine;
    const auto cast  = parse_line(HIRED_MUSCLE_CAST_LINE);
    const auto death = parse_line(HIRED_MUSCLE_DEATH_LINE);
    ASSERT_TRUE(cast.has_value());
    ASSERT_TRUE(death.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    engine.handle_event(*cast);
    engine.handle_event(*death);

    int callbacks = 0;
    engine.set_shotcall_callback([&callbacks](const std::string&, const std::string&) {
        ++callbacks;
    });

    EXPECT_FALSE(engine.dispatch_next_shotcall(cast->time_stamp + *first_cast));
    EXPECT_EQ(callbacks, 0);
}

TEST(Integration, PlayerDeath_MakesPlayerUnselectable)
{
    ShotCallEngine engine;
    const auto skyfury     = parse_line(SKYFURY_LINE);
    const auto first_cast  = parse_line(HIRED_MUSCLE_CAST_LINE);
    const auto second_cast = parse_line(SECOND_HIRED_MUSCLE_CAST_LINE);
    const auto death       = parse_line(PLAYER_DEATH_LINE);
    ASSERT_TRUE(skyfury.has_value());
    ASSERT_TRUE(first_cast.has_value());
    ASSERT_TRUE(second_cast.has_value());
    ASSERT_TRUE(death.has_value());
    const auto first_cast_delay = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast_delay.has_value());

    engine.handle_event(*skyfury);
    engine.handle_event(*first_cast);

    std::string callout;
    engine.set_shotcall_callback([&callout](const std::string&, const std::string& text) {
        callout = text;
    });

    EXPECT_TRUE(engine.dispatch_next_shotcall(first_cast->time_stamp + *first_cast_delay));
    EXPECT_NE(callout.find("Lilrawb"), std::string::npos);

    engine.handle_event(*death);
    engine.handle_event(*second_cast);

    EXPECT_TRUE(engine.dispatch_next_shotcall(second_cast->time_stamp + *first_cast_delay));
    EXPECT_NE(callout.find("this one is going off"), std::string::npos);
}

TEST(Integration, CombatantInfoAndEnemyCast_ProducesCall)
{
    ShotCallEngine engine;
    const auto combatant_info = parse_line(COMBATANT_INFO_LINE);
    const auto cast           = parse_line(HIRED_MUSCLE_CAST_LINE);
    ASSERT_TRUE(combatant_info.has_value());
    ASSERT_TRUE(cast.has_value());
    const auto first_cast = first_cast_for("210269", 463218);
    ASSERT_TRUE(first_cast.has_value());

    EXPECT_EQ(combatant_info->source_id, "Player-3676-0CD71E8D");
    EXPECT_EQ(combatant_info->spec_id, 65);

    engine.handle_event(*combatant_info);
    engine.handle_event(*cast);

    int callbacks = 0;
    engine.set_shotcall_callback([&callbacks](const std::string&, const std::string&) {
        ++callbacks;
    });

    EXPECT_TRUE(engine.dispatch_next_shotcall(cast->time_stamp + *first_cast));
    EXPECT_EQ(callbacks, 1);
}
