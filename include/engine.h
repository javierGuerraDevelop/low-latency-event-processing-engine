// Core shotcall engine. Tracks party members, enemies, and their ability
// cooldowns. When an enemy is identified, generates a time-sorted queue of
// upcoming shotcalls. A background thread dispatches callouts at the right
// time, assigning available interrupters or CC users to each call.

#ifndef SHOTCALLERCPP_ENGINE_H
#define SHOTCALLERCPP_ENGINE_H

#include <chrono>
#include <functional>
#include <list>
#include <map>
#include <mutex>
#include <string>
#include <string_view>
#include <tuple>
#include <utility>
#include <vector>

#include "parser.h"

namespace ch = std::chrono;

// Tracks a player ability's cooldown state.
struct AbilityState {
    int id;
    ch::seconds cooldown { };
    ch::time_point<ch::system_clock> on_cooldown_until { };

    AbilityState(int ability_id, ch::seconds cooldown_duration)
        : id { ability_id }
        , cooldown { cooldown_duration }
    {
    }
    AbilityState()
        : id { 0 }
        , cooldown { ch::seconds { 0 } }
    {
    }
};

// Describes an enemy's ability with its cast timing and callout text.
struct EnemyAbility {
    int id;
    ch::milliseconds first_cast;
    ch::milliseconds cooldown;
    std::string callout;
    bool is_interruptable;

    EnemyAbility(int ability_id, ch::milliseconds first_cast_delay, ch::milliseconds cooldown_duration,
        std::string callout_text, bool interruptable)
        : id { ability_id }
        , first_cast { first_cast_delay }
        , cooldown { cooldown_duration }
        , callout { std::move(callout_text) }
        , is_interruptable { interruptable }
    {
    }
};

// A party member known by GUID. The name may stay empty until an event
// carrying it is seen; the class comes from an action or COMBATANT_INFO.
struct Player {
    std::string guid;
    std::string name;
    std::string class_name;
    int spec_id = 0;
    std::string spec_name;
    AbilityState interrupt;
    std::map<int, AbilityState> crowd_control;
    bool is_alive = true;

    Player(std::string player_guid, std::string player_name, std::string player_class,
        AbilityState interrupt_ability, std::map<int, AbilityState> crowd_control_abilities)
        : guid { std::move(player_guid) }
        , name { std::move(player_name) }
        , class_name { std::move(player_class) }
        , interrupt { interrupt_ability }
        , crowd_control { std::move(crowd_control_abilities) }
    {
    }

    Player() = default;
};

// A tracked enemy and the abilities generated from its static profile.
struct Enemy {
    std::string guid;
    std::vector<EnemyAbility> spells;
    ch::time_point<ch::system_clock> first_seen_time;
    bool is_ccable = false;

    Enemy(std::string enemy_guid, std::vector<EnemyAbility> abilities,
        ch::time_point<ch::system_clock> first_seen, bool ccable)
        : guid { std::move(enemy_guid) }
        , spells { std::move(abilities) }
        , first_seen_time { first_seen }
        , is_ccable { ccable }
    {
    }
};

class ShotCallEngine {
public:
    // Routes incoming combat events to the appropriate handler.
    void handle_event(const CombatEvent& event);
    void set_shotcall_callback(
        std::function<void(const std::string&, const std::string&)> callback);
    // Runs on a background thread; dispatches queued shotcalls at their scheduled time.
    void process_shotcalls();
    // Acquires mtx_ itself. Dispatches the next shotcall if one is due and
    // returns true when a callback was invoked.
    bool dispatch_next_shotcall(ch::time_point<ch::system_clock> now);

private:
    // Handlers and helpers below assume mtx_ is already held.
    void handle_player_event(const CombatEvent& event);
    void handle_enemy_event(const CombatEvent& event);
    void handle_death(const CombatEvent& event);
    void handle_combatant_info(const CombatEvent& event);
    // Auto-detects player class or enemy type from combat event spells.
    void identify_player(const CombatEvent& event);
    void identify_enemy(const CombatEvent& event);
    // Fills a roster player's name if it is still empty.
    void learn_player_name(const std::string& guid, std::string_view name);
    // Pre-computes all shotcalls for an enemy over a 5-minute window.
    void generate_shotcalls(Enemy& enemy);
    // Returns the name of a living player whose interrupt/CC is off cooldown
    // at call_time, or a fallback message if none available.
    std::string find_available_interrupter(const ch::time_point<ch::system_clock>& call_time);
    std::string find_available_ccer(const ch::time_point<ch::system_clock>& call_time);

    std::function<void(const std::string&, const std::string&)> shotcall_callback_;
    std::mutex mtx_; // Guards all mutable state below
    std::map<std::string, Player> roster_;
    std::map<std::string, Enemy> enemy_roster_;
    // Queue entries: (is_interruptable, enemy_guid, callout_text, scheduled_time)
    std::list<std::tuple<bool, std::string, std::string, ch::time_point<ch::system_clock>>>
        shot_call_queue_;
};

#endif // SHOTCALLERCPP_ENGINE_H
