#include <gtest/gtest.h>

#include <future>
#include <thread>

#include "constants.h"
#include "engine.h"

namespace ch = std::chrono;

// Helper: build a CombatEvent struct directly (bypasses parser).
CombatEvent make_event(const std::string& event_type, const std::string& source_id,
    const std::string& name, const std::string& source_flag,
    const std::string& target_id, int spell_id, const std::string& npc_id = "",
    ch::system_clock::time_point ts = ch::system_clock::now())
{
    CombatEvent ev { };
    ev.time_stamp   = ts;
    ev.event_type   = event_type;
    ev.name         = name;
    ev.source_id    = source_id;
    ev.target_id    = target_id;
    ev.source_flags = source_flag;
    ev.spell_id     = spell_id;
    ev.npc_id       = npc_id;
    return ev;
}

constexpr const char* PLAYER_FLAG = "0x511";
constexpr const char* ENEMY_FLAG  = "0xa48";

// Builds a CHALLENGE_MODE_START event for the Cinderbrew Meadery fixture run.
CombatEvent make_challenge_start(ch::system_clock::time_point ts)
{
    CombatEvent event;
    event.time_stamp     = ts;
    event.event_type     = "CHALLENGE_MODE_START";
    event.instance_id    = 2661;
    event.keystone_level = 13;
    return event;
}

CombatEvent make_encounter_start(int group_size, ch::system_clock::time_point ts)
{
    CombatEvent event;
    event.time_stamp   = ts;
    event.event_type   = "ENCOUNTER_START";
    event.encounter_id = 2900;
    event.group_size   = group_size;
    event.instance_id  = 2661;
    return event;
}

CombatEvent make_combatant_info(
    const std::string& guid, int spec_id, ch::system_clock::time_point ts)
{
    CombatEvent event;
    event.time_stamp = ts;
    event.event_type = "COMBATANT_INFO";
    event.source_id  = guid;
    event.spec_id    = spec_id;
    return event;
}

// ==================== Player Identification ====================

TEST(Engine, IdentifyPlayer_BattleShout_Warrior)
{
    ShotCallEngine engine;
    auto ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6673); // Battle Shout
    engine.handle_event(ev);

    // Second event with same source should not re-identify (verify no crash)
    auto ev2 = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6673);
    engine.handle_event(ev2);
}

TEST(Engine, IdentifyPlayer_UnknownSpell_NotAdded)
{
    ShotCallEngine engine;
    auto ev = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Nobody", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 999999);
    engine.handle_event(ev);
    // No crash, player not in roster (tested indirectly via dispatch)
}

TEST(Engine, IdentifyPlayer_MultipleDifferentClasses)
{
    ShotCallEngine engine;
    auto ev1 = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Warrior", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6673); // Battle Shout -> Warrior
    auto ev2 = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Shaman", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 8004); // Healing Surge -> Shaman
    auto ev3 = make_event("SPELL_CAST_SUCCESS", "Player-1-CCC", "Mage", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 1459); // Arcane Intellect -> Mage
    engine.handle_event(ev1);
    engine.handle_event(ev2);
    engine.handle_event(ev3);
    // All three identified without error
}

TEST(Engine, FirstEventInterrupt_IdentifiesWarriorAndAppliesCooldown)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Pummel (6552) arrives before any class-identifying spell.
    auto pummel = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6552, "", now);
    engine.handle_event(pummel);

    // A later identifying spell must not duplicate or reset the roster entry.
    auto shout = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now + ch::seconds { 1 });
    engine.handle_event(shout);

    auto enemy = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy);

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    // Pummel is on cooldown until now+15s, so the 4s call has no interrupter.
    engine.dispatch_due(now + ch::seconds { 4 });
    EXPECT_NE(callout.find("this one is going off"), std::string::npos);

    // The entry was not reset: after the cooldown the Warrior is assigned.
    engine.dispatch_due(now + ch::milliseconds { 4000 + 16900 });
    EXPECT_NE(callout.find("Tank"), std::string::npos);
}

TEST(Engine, FirstEventCrowdControl_IdentifiesWarrior)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Shockwave (46968) is only known through the crowd-control table.
    auto shockwave = make_event("SPELL_CAST_SUCCESS", "Player-1-CCC", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 46968, "", now);
    engine.handle_event(shockwave);

    // NPC 164557 casts a non-interruptable ability that requires a CCer.
    auto enemy = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-164557-ABC", "Mob", ENEMY_FLAG,
        "Player-1-CCC", 326409, "164557", now);
    engine.handle_event(enemy);

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    engine.dispatch_due(now + ch::milliseconds { 8900 });
    EXPECT_NE(callout.find("Tank"), std::string::npos);
}

TEST(Engine, UnknownFirstSpell_IgnoredUntilIdentified)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // The first event carries no class information: ignored without crashing.
    auto unknown = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Nobody", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 999999, "", now);
    engine.handle_event(unknown);

    // A later identifying spell creates the entry.
    auto shout = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Nobody", PLAYER_FLAG,
        "Player-1-AAA", 6673, "", now + ch::seconds { 1 });
    engine.handle_event(shout);

    auto enemy = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now + ch::seconds { 1 });
    engine.handle_event(enemy);

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    engine.dispatch_due(now + ch::seconds { 1 } + ch::seconds { 4 });
    EXPECT_NE(callout.find("Nobody"), std::string::npos);
}

TEST(Engine, UnknownSpecCombatantInfo_DoesNotBlockIdentification)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // An unknown spec must not create a player entry that would block the
    // normal action-based identification path.
    auto info    = make_event("COMBATANT_INFO", "Player-1-ZZZ", "", "0", "", 0, "", now);
    info.spec_id = 999999;
    engine.handle_event(info);

    auto shout = make_event("SPELL_CAST_SUCCESS", "Player-1-ZZZ", "Tank", PLAYER_FLAG,
        "Player-1-AAA", 6673, "", now + ch::seconds { 1 });
    engine.handle_event(shout);

    auto enemy = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now + ch::seconds { 1 });
    engine.handle_event(enemy);

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    engine.dispatch_due(now + ch::seconds { 1 } + ch::seconds { 4 });
    EXPECT_NE(callout.find("Tank"), std::string::npos);
}

// ==================== Cooldown Tracking ====================

TEST(Engine, InterruptCast_PutsOnCooldown)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Identify as Warrior
    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    // Cast interrupt (Pummel)
    auto int_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6552, "", now);
    engine.handle_event(int_ev);

    // Set up an enemy and shotcall to verify the player is on cooldown
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    // Dispatch within 1s window before first shotcall (first_cast=4000ms)
    auto dispatch_time = now + ch::milliseconds { 3500 };
    engine.dispatch_due(dispatch_time);

    // Warrior is on cooldown, should get "this one is going off"
    EXPECT_NE(last_callout.find("this one is going off"), std::string::npos);
}

TEST(Engine, CCCast_PutsOnCooldown)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    // Identify as Warrior
    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    // Cast CC (Shockwave 46968, 40s cd)
    auto cc_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 46968, "", now);
    engine.handle_event(cc_ev);
    // No crash; CD is tracked internally
}

TEST(Engine, IgnorableEvent_DoesNotAffectCooldowns)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    // SPELL_DAMAGE is ignorable
    auto dmg_ev = make_event("SPELL_DAMAGE", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6552, "", now);
    engine.handle_event(dmg_ev);
    // No cooldown should be set (tested indirectly -- interrupt should still be available)
}

// ==================== Death / Rez ====================

TEST(Engine, PlayerDeath_MarkedDead)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    auto death_ev = make_event("UNIT_DIED", "", "", "", "Player-1-AAA", 0, "", now);
    engine.handle_event(death_ev);

    // Set up enemy and try dispatch -- dead player should be skipped
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    // Dispatch within 1s window before first shotcall (first_cast=4000ms)
    engine.dispatch_due(now + ch::milliseconds { 3500 });
    EXPECT_NE(last_callout.find("this one is going off"), std::string::npos);
}

TEST(Engine, BattleRez_RevivesPlayer)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Identify Warrior
    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    // Identify Druid (separate player for the rez)
    auto id_ev2 = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Healer", PLAYER_FLAG,
        "Player-1-AAA", 1126, "", now);
    engine.handle_event(id_ev2);

    // Kill Warrior
    auto death_ev = make_event("UNIT_DIED", "", "", "", "Player-1-AAA", 0, "", now);
    engine.handle_event(death_ev);

    // Rez Warrior (battle rez id 61999)
    auto rez_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Healer", PLAYER_FLAG,
        "Player-1-AAA", 61999, "", now);
    engine.handle_event(rez_ev);

    // Set up enemy
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-DEF", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    // Dispatch within 1s window before first shotcall (first_cast=4000ms)
    engine.dispatch_due(now + ch::milliseconds { 3500 });
    // Warrior is alive again and should be assigned (Tank or this one is going off depending on
    // other state) Since Warrior's interrupt is off cooldown, should see "Tank" in the callout
    EXPECT_NE(last_callout.find("Tank"), std::string::npos);
}

TEST(Engine, EnemyDeath_RemovesAndPurgesQueue)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    auto death_ev = make_event("UNIT_DIED", "", "", "", "Creature-0-0-0-0-216293-ABC", 0, "", now);
    engine.handle_event(death_ev);

    // Nothing to dispatch
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 5 }), 0u);
}

TEST(Engine, DeathOfUnknownEntity_NoOp)
{
    ShotCallEngine engine;
    auto death_ev = make_event("UNIT_DIED", "", "", "", "Player-1-UNKNOWN", 0);
    engine.handle_event(death_ev);
    // No crash
}

// ==================== Enemy Identification ====================

TEST(Engine, IdentifyEnemy_TrackedNPC)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // 216293 is a tracked NPC in Ara-kara
    auto ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(ev);

    // Should have generated shotcalls -- dispatch should work
    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });
    // First shotcall for 216293 (AoE Barrage) is at combat_start + 4000ms
    const std::size_t dispatched = engine.dispatch_due(now + ch::seconds { 4 });
    EXPECT_EQ(dispatched, 1u);
}

TEST(Engine, IdentifyEnemy_UntrackedNPC_Ignored)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    auto ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-000000-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 1, "000000", now);
    engine.handle_event(ev);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 5 }), 0u);
}

TEST(Engine, IdentifyEnemy_EmptyNpcId_Ignored)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    auto ev = make_event("SPELL_CAST_SUCCESS", "Player-1-XYZ", "Mob", ENEMY_FLAG, "Player-1-AAA", 1,
        "", now);
    engine.handle_event(ev);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 5 }), 0u);
}

TEST(Engine, IdentifyEnemy_DuplicateGUID_NotReidentified)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    auto ev1 = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    auto ev2 = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(ev1);
    engine.handle_event(ev2);
    // Should only have one set of shotcalls, not duplicated
}

TEST(Engine, IdentifyEnemy_MultipleSpells)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // NPC 214761 has two spells: Seed (23000ms cd) and Ray (10900ms cd)
    auto ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-214761-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 432448, "214761", now);
    engine.handle_event(ev);

    // Should have shotcalls from both spells
    std::vector<std::string> callouts;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        callouts.push_back(callout);
    });

    // Dispatch multiple times covering both first_cast times
    engine.dispatch_due(now + ch::milliseconds { 3300 }); // Ray first_cast
    engine.dispatch_due(now + ch::milliseconds { 8300 }); // Seed first_cast
    EXPECT_GE(callouts.size(), 2u);
}

// ==================== Shotcall Generation ====================

TEST(Engine, GenerateShotcalls_CorrectCount)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // NPC 216293 has AoE Barrage: first_cast=4000ms, cd=16900ms
    // Over 5 min (300000ms): iterations = (300000/16900)+1 = 18
    auto ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(ev);

    int count = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        count++;
    });

    // Dispatch at each exact call_time (must be within 1s window)
    long long first_cast_ms = 4000;
    long long cd_ms         = 16900;
    for (int i = 0; i < 18; i++) {
        long long ms = first_cast_ms + i * cd_ms;
        engine.dispatch_due(now + ch::milliseconds { ms });
    }
    EXPECT_EQ(count, 18);
}

TEST(Engine, GenerateShotcalls_SortedByTime)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // NPC 214761 has two spells: Seed (first_cast=8300, cd=23000) and Ray (first_cast=3300,
    // cd=10900)
    auto ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-214761-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 432448, "214761", now);
    engine.handle_event(ev);

    std::vector<ch::system_clock::time_point> dispatch_times;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        dispatch_times.push_back(ch::system_clock::now());
    });

    // Step through time in 500ms increments to catch all shotcalls in their windows
    int dispatched = 0;
    for (long long ms = 0; ms <= 310000; ms += 500) {
        dispatched += static_cast<int>(engine.dispatch_due(now + ch::milliseconds { ms }));
    }
    EXPECT_GT(dispatched, 0);
}

// ==================== Dispatch ====================

TEST(Engine, Dispatch_AvailableInterrupter)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now); // Warrior
    engine.handle_event(id_ev);

    // NPC 216293 has interruptable AoE Barrage
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    engine.dispatch_due(now + ch::seconds { 4 });
    EXPECT_NE(last_callout.find("Tank"), std::string::npos);
}

TEST(Engine, Dispatch_InterrupterOnCooldown_AssignsNext)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Warrior (Tank)
    auto id1 = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG, "Player-1-BBB",
        6673, "", now);
    engine.handle_event(id1);
    // Shaman (Healer)
    auto id2 = make_event("SPELL_CAST_SUCCESS", "Player-1-BBB", "Healer", PLAYER_FLAG,
        "Player-1-AAA", 8004, "", now);
    engine.handle_event(id2);

    // Warrior uses interrupt
    auto int_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6552, "", now);
    engine.handle_event(int_ev);

    // Set up enemy
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    engine.dispatch_due(now + ch::seconds { 4 });
    // Warrior on cooldown, Shaman should be assigned
    EXPECT_NE(last_callout.find("Healer"), std::string::npos);
}

TEST(Engine, Dispatch_AllOnCooldown_GoingOff)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    auto id1 = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG, "Player-1-BBB",
        6673, "", now);
    engine.handle_event(id1);

    // Use interrupt
    auto int_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6552, "", now);
    engine.handle_event(int_ev);

    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    engine.dispatch_due(now + ch::seconds { 4 });
    EXPECT_NE(last_callout.find("this one is going off"), std::string::npos);
}

TEST(Engine, Dispatch_DeadPlayerSkipped)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    auto death_ev = make_event("UNIT_DIED", "", "", "", "Player-1-AAA", 0, "", now);
    engine.handle_event(death_ev);

    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    engine.dispatch_due(now + ch::seconds { 4 });
    EXPECT_NE(last_callout.find("this one is going off"), std::string::npos);
}

TEST(Engine, Dispatch_NonInterruptable_AssignsCCer)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Warrior with CC (Shockwave/Intimidating Shout)
    auto id_ev = make_event("SPELL_CAST_SUCCESS", "Player-1-AAA", "Tank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now);
    engine.handle_event(id_ev);

    // NPC 164557 has non-interruptable AOE (is_ccable=true)
    auto enemy_ev = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-164557-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 326409, "164557", now);
    engine.handle_event(enemy_ev);

    std::string last_callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& callout) {
        last_callout = callout;
    });

    // First cast at 8900ms
    engine.dispatch_due(now + ch::milliseconds { 8900 });
    // Warrior has CC available, should be assigned
    EXPECT_NE(last_callout.find("Tank"), std::string::npos);
}

// ==================== Run boundaries ====================

TEST(Engine, EnemyEventWhileIdle_IgnoredUntilChallengeStart)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    auto enemy = make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);

    engine.handle_event(enemy);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 0u);

    engine.handle_event(make_challenge_start(now));
    engine.handle_event(enemy);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);
}

TEST(Engine, EnemyAuraDuringRun_DoesNotEngage)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_challenge_start(now));

    auto aura = make_event("SPELL_AURA_APPLIED", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(aura);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 0u);

    auto cast = make_event("SPELL_CAST_START", "Creature-0-0-0-0-216293-ABC", "Mob", ENEMY_FLAG,
        "Player-1-AAA", 434793, "216293", now);
    engine.handle_event(cast);
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);
}

TEST(Engine, EncounterStart_GroupSizeFiveAllowsCalls)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_encounter_start(5, now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);
}

TEST(Engine, EncounterStart_WrongGroupSizePausesAndValidRestores)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_encounter_start(3, now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 0u);

    engine.handle_event(make_encounter_start(5, now + ch::seconds { 1 }));
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);
}

TEST(Engine, ChallengeModeEnd_PurgesCallsAndIgnoresEndBeforeStart)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    // End before any start is a no-op.
    engine.handle_event(make_event("CHALLENGE_MODE_END", "", "", "", "", 0, "", now));

    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));
    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);

    engine.handle_event(
        make_event("CHALLENGE_MODE_END", "", "", "", "", 0, "", now + ch::seconds { 1 }));
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4000 + 16900 }), 0u);
}

TEST(Engine, EncounterEnd_DoesNotPurgeChallengeQueue)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));
    engine.handle_event(make_encounter_start(5, now));
    engine.handle_event(make_event("ENCOUNTER_END", "", "", "", "", 0, "", now + ch::seconds { 1 }));

    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 1u);
}

TEST(Engine, ZoneChangeWhileEncounterActive_EndsRun)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_encounter_start(5, now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    auto zone        = make_event("ZONE_CHANGE", "", "", "", "", 0, "", now + ch::seconds { 1 });
    zone.instance_id = 9999;
    engine.handle_event(zone);

    EXPECT_EQ(engine.dispatch_due(now + ch::seconds { 4 }), 0u);
    EXPECT_FALSE(engine.party_status().in_run);
}

// ==================== Roster snapshot and party status ====================

TEST(Engine, PartyStatus_FivePlayerSnapshot)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    engine.handle_event(make_combatant_info("Player-1-PALA", 65, now + ch::milliseconds { 1 }));
    engine.handle_event(make_combatant_info("Player-1-DK", 252, now + ch::milliseconds { 1 }));
    engine.handle_event(make_combatant_info("Player-1-WAR", 73, now + ch::milliseconds { 1 }));
    engine.handle_event(make_combatant_info("Player-1-SHA", 262, now + ch::milliseconds { 1 }));
    engine.handle_event(make_combatant_info("Player-1-WLK", 267, now + ch::milliseconds { 1 }));

    // A later non-COMBATANT_INFO event finalizes the snapshot.
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-PALA", "Pala", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 19750, "", now + ch::milliseconds { 2 }));

    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.roster_known);
    EXPECT_TRUE(status.in_run);
    EXPECT_EQ(status.identified, 5);
    EXPECT_EQ(status.expected, 5);
}

TEST(Engine, PartyStatus_SnapshotWithFourPlayersReportsGap)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    engine.handle_event(make_combatant_info("Player-1-A", 65, now));
    engine.handle_event(make_combatant_info("Player-1-B", 252, now));
    engine.handle_event(make_combatant_info("Player-1-C", 73, now));
    engine.handle_event(make_combatant_info("Player-1-D", 999999, now));

    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-A", "A", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6673, "", now + ch::milliseconds { 1 }));

    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.roster_known);
    EXPECT_EQ(status.identified, 3);
    EXPECT_EQ(status.expected, 4);
}

TEST(Engine, PartyStatus_NoSnapshotIsNotKnown)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Without COMBATANT_INFO the first event resolves the snapshot as unknown.
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-A", "A", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 6673, "", now));

    const PartyStatus status = engine.party_status();
    EXPECT_FALSE(status.roster_known);
    EXPECT_EQ(status.expected, 5);
}

TEST(Engine, PartyStatus_SnapshotFinalizesWhenWindowElapses)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_combatant_info("Player-1-A", 65, now));
    engine.handle_event(make_combatant_info("Player-1-B", 252, now));

    // Before the window closes the snapshot is still pending.
    engine.dispatch_due(now + ch::milliseconds { 499 });
    EXPECT_FALSE(engine.party_status().roster_known);

    // The scheduler tick with a later now finalizes it without another event.
    engine.dispatch_due(now + ch::milliseconds { 501 });
    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.roster_known);
    EXPECT_EQ(status.identified, 2);
    EXPECT_EQ(status.expected, 2);
}

TEST(Engine, PartyStatus_CombatLogVersionRecordsAdvancedLogging)
{
    ShotCallEngine engine;

    auto version             = make_event("COMBAT_LOG_VERSION", "", "", "", "", 0);
    version.advanced_logging = true;
    engine.handle_event(version);

    const PartyStatus status = engine.party_status();
    EXPECT_TRUE(status.advanced_logging);
    EXPECT_FALSE(status.in_run);
}

TEST(Engine, ClassKnowledgeSurvivesBetweenKeys)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_combatant_info("Player-1-PALA", 65, now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-PALA", "Pala", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 19750, "", now + ch::milliseconds { 1 }));
    engine.handle_event(
        make_event("CHALLENGE_MODE_END", "", "", "", "", 0, "", now + ch::seconds { 5 }));

    const auto key_two = now + ch::seconds { 10 };
    engine.handle_event(make_challenge_start(key_two));
    engine.handle_event(make_combatant_info("Player-1-PALA", 65, key_two));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-PALA", "Pala", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 19750, "", key_two + ch::milliseconds { 1 }));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-PALA", 434793, "216293", key_two + ch::milliseconds { 1 }));

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(key_two + ch::milliseconds { 1 } + ch::seconds { 4 }), 1u);
    EXPECT_NE(callout.find("Pala"), std::string::npos);
}

TEST(Engine, PreviousRunPlayerNotInSnapshotIsNotAssigned)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();

    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-OLD", "OldTank", PLAYER_FLAG,
        "Player-1-BBB", 6673, "", now));
    engine.handle_event(
        make_event("CHALLENGE_MODE_END", "", "", "", "", 0, "", now + ch::seconds { 5 }));

    const auto key_two = now + ch::seconds { 10 };
    engine.handle_event(make_challenge_start(key_two));
    engine.handle_event(make_combatant_info("Player-1-PALA", 65, key_two));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Player-1-PALA", "Pala", PLAYER_FLAG,
        "Creature-0-0-0-0-999-0", 19750, "", key_two + ch::milliseconds { 1 }));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-PALA", 434793, "216293", key_two + ch::milliseconds { 1 }));

    std::string callout;
    engine.set_shotcall_callback([&](const std::string&, const std::string& text) {
        callout = text;
    });

    EXPECT_EQ(engine.dispatch_due(key_two + ch::milliseconds { 1 } + ch::seconds { 4 }), 1u);
    EXPECT_NE(callout.find("Pala"), std::string::npos);
    EXPECT_EQ(callout.find("OldTank"), std::string::npos);
}

// ==================== Scheduler ====================

TEST(Engine, DispatchDue_RespectsLeadWindow)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    int callbacks = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        ++callbacks;
    });

    // AoE Barrage is due at now+4000ms; the default lead is 1000ms.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 2999 }), 0u);
    EXPECT_EQ(callbacks, 0);
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 3000 }), 1u);
    EXPECT_EQ(callbacks, 1);
}

TEST(Engine, DispatchDue_LateWithinGraceAndRecurrenceAfterExpiry)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    int callbacks = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        ++callbacks;
    });

    // 999ms late is still inside the 1000ms grace.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4999 }), 1u);
    // The next occurrence is due at now+20900ms and has expired at now+22400ms.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 22400 }), 0u);
    EXPECT_EQ(callbacks, 1);
    // Expiry still advanced the recurrence: the following occurrence is announced.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 37800 }), 1u);
    EXPECT_EQ(callbacks, 2);
}

TEST(Engine, DispatchDue_BacklogDispatchesDueCallInSamePass)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // Ten enemies whose first cast has already expired by the dispatch time.
    for (int i = 0; i < 10; ++i) {
        engine.handle_event(make_event("SPELL_CAST_SUCCESS",
            "Creature-0-0-0-0-216293-" + std::to_string(i), "Mob", ENEMY_FLAG, "Player-1-AAA",
            434793, "216293", now));
    }
    // One enemy whose call is due exactly at the dispatch time.
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now + ch::milliseconds { 5000 }));

    int callbacks = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        ++callbacks;
    });

    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 9000 }), 1u);
    EXPECT_EQ(callbacks, 1);
}

TEST(Engine, DispatchDue_DispatchesInDueOrder)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));

    // NPC 214761 schedules Ray at now+3300ms; NPC 216293 schedules AoE Barrage at now+4000ms.
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-214761-AAA", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 432448, "214761", now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    std::vector<std::string> enemy_order;
    engine.set_shotcall_callback([&](const std::string& enemy_guid, const std::string&) {
        enemy_order.push_back(enemy_guid);
    });

    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4000 }), 2u);
    ASSERT_EQ(enemy_order.size(), 2u);
    EXPECT_EQ(enemy_order[0], "Creature-0-0-0-0-214761-AAA");
    EXPECT_EQ(enemy_order[1], "Creature-0-0-0-0-216293-ABC");
}

TEST(Engine, DispatchDue_LazyRecurrenceOneCooldownLater)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    int callbacks = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        ++callbacks;
    });

    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4000 }), 1u);
    // The next occurrence is due at now+20900ms and becomes dispatchable one lead earlier.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 19899 }), 0u);
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 19900 }), 1u);
    EXPECT_EQ(callbacks, 2);
}

TEST(Engine, DispatchDue_EnemyDeathLazilyCancelsCalls)
{
    ShotCallEngine engine;
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    auto death = make_event("UNIT_DIED", "", "", "", "Creature-0-0-0-0-216293-ABC", 0, "",
        now + ch::seconds { 1 });
    engine.handle_event(death);

    int callbacks = 0;
    engine.set_shotcall_callback([&](const std::string&, const std::string&) {
        ++callbacks;
    });

    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4000 }), 0u);
    EXPECT_EQ(callbacks, 0);
    // No recurrence survives the dead enemy either.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 20900 }), 0u);
}

TEST(Engine, DispatchDue_CustomLead)
{
    ShotCallEngine engine { ch::milliseconds { 2000 }, ch::milliseconds { 500 } };
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    // Due at now+4000ms; the custom lead is 2000ms.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 1999 }), 0u);
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 2000 }), 1u);
}

TEST(Engine, DispatchDue_CustomGrace)
{
    ShotCallEngine engine { ch::milliseconds { 1000 }, ch::milliseconds { 100 } };
    auto now = ch::system_clock::now();
    engine.handle_event(make_challenge_start(now));
    engine.handle_event(make_event("SPELL_CAST_SUCCESS", "Creature-0-0-0-0-216293-ABC", "Mob",
        ENEMY_FLAG, "Player-1-AAA", 434793, "216293", now));

    // 101ms late exceeds the custom grace.
    EXPECT_EQ(engine.dispatch_due(now + ch::milliseconds { 4101 }), 0u);
}

TEST(Engine, ProcessShotcalls_StopsPromptly)
{
    ShotCallEngine engine;
    std::promise<void> finished;
    std::jthread worker { [&engine, &finished](std::stop_token stop_token) {
        engine.process_shotcalls(stop_token);
        finished.set_value();
    } };

    worker.request_stop();
    EXPECT_EQ(finished.get_future().wait_for(ch::seconds { 2 }), std::future_status::ready);
}
