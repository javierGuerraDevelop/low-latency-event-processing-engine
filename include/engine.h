// Core shotcall engine. Tracks party members, enemies, and their ability
// cooldowns. When an enemy is identified, schedules its abilities lazily and
// dispatches callouts from an event-driven loop, assigning available
// interrupters or CC users to each call.

#ifndef SHOTCALLERCPP_ENGINE_H
#define SHOTCALLERCPP_ENGINE_H

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <mutex>
#include <optional>
#include <set>
#include <stop_token>
#include <string>
#include <string_view>
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
    // Runtime scheduling state, advanced after each occurrence.
    ch::time_point<ch::system_clock> next_due { };
    std::uint64_t generation = 0;

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

// One predicted cast waiting for its announcement window.
struct ScheduledShotCall {
    std::string enemy_guid;
    int spell_id;
    std::string callout;
    ch::time_point<ch::system_clock> due;
    std::uint64_t generation;
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
    // Timestamp of the most recent event carrying this player's GUID.
    ch::time_point<ch::system_clock> last_seen { };

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

// Whether a Mythic+ challenge is active; encounter state is tracked separately.
enum class RunState { Idle,
    ChallengeActive };

// Snapshot of party identification state for status reporting.
struct PartyStatus {
    int identified        = 0;
    int expected          = 0;
    bool roster_known     = false;
    bool advanced_logging = false;
    bool in_run           = false;
};

class ShotCallEngine {
public:
    // call_lead is how early a call may fire; late_grace is how late it may fire.
    explicit ShotCallEngine(ch::milliseconds call_lead = ch::milliseconds { 1000 },
        ch::milliseconds late_grace                    = ch::milliseconds { 1000 });
    // Routes incoming combat events to the appropriate handler.
    void handle_event(const CombatEvent& event);
    void set_shotcall_callback(
        std::function<void(const std::string&, const std::string&)> callback);
    // Dispatches every call whose window contains now and returns the number
    // of callbacks invoked. Expired calls are dropped, not announced.
    std::size_t dispatch_due(ch::time_point<ch::system_clock> now);
    // Dispatches calls until stop is requested, waiting for the next due time
    // or a new schedule change.
    void process_shotcalls(std::stop_token stop_token);
    // Returns a snapshot of party identification state.
    PartyStatus party_status() const;

private:
    // Handlers and helpers below assume mtx_ is already held.
    void handle_player_event(const CombatEvent& event);
    void handle_enemy_event(const CombatEvent& event);
    void handle_death(const CombatEvent& event);
    void handle_combatant_info(const CombatEvent& event);
    // Applies challenge/encounter boundaries. Returns true when consumed.
    bool handle_boundary_event(const CombatEvent& event);
    // Starts collecting a COMBATANT_INFO snapshot for the current boundary.
    void begin_roster_snapshot(ch::time_point<ch::system_clock> started_at);
    void reset_roster_snapshot();
    // Records the snapshot once its window closes or another event follows.
    void finalize_roster_snapshot_if_due(ch::time_point<ch::system_clock> now, bool event_followed);
    // True while a challenge or encounter is active.
    bool in_active_run() const;
    // True when the player belongs to the current run's roster.
    bool player_in_current_run(const std::string& guid, const Player& player) const;
    // Drops tracked enemies and their queued calls.
    void clear_enemies_and_calls();
    // Processes due and expired calls, returning the callbacks to invoke.
    std::vector<std::pair<std::string, std::string>> dispatch_due_locked(
        ch::time_point<ch::system_clock> now);
    // Queues the ability's next occurrence and wakes the scheduler thread.
    void enqueue_shotcall_locked(const std::string& enemy_guid, const EnemyAbility& ability);
    // Moves an ability one cooldown forward and queues the next occurrence.
    void advance_recurrence(Enemy& enemy, EnemyAbility& ability);
    // Wakes the scheduler after a schedule or enemy change.
    void wake_scheduler_locked();
    // Earliest time the scheduler should wake, if any call can fire.
    std::optional<ch::time_point<ch::system_clock>> next_actionable_time_locked() const;
    // Auto-detects player class or enemy type from combat event spells.
    void identify_player(const CombatEvent& event);
    void identify_enemy(const CombatEvent& event);
    // Fills a roster player's name if it is still empty.
    void learn_player_name(const std::string& guid, std::string_view name);
    // Returns the name of a living player whose interrupt/CC is off cooldown
    // at call_time, or a fallback message if none available.
    std::string find_available_interrupter(const ch::time_point<ch::system_clock>& call_time);
    std::string find_available_ccer(const ch::time_point<ch::system_clock>& call_time);

    const ch::milliseconds call_lead_;
    const ch::milliseconds late_grace_;
    std::function<void(const std::string&, const std::string&)> shotcall_callback_;
    mutable std::mutex mtx_; // Guards all mutable state below
    std::condition_variable_any wakeup_;
    std::uint64_t queue_revision_ = 0;
    std::map<std::string, Player> roster_;
    std::map<std::string, Enemy> enemy_roster_;
    std::set<std::string> run_roster_;
    // Scheduled calls ordered by their predicted cast time.
    std::multimap<ch::time_point<ch::system_clock>, ScheduledShotCall> shot_call_queue_;
    RunState run_state_           = RunState::Idle;
    bool encounter_active_        = false;
    bool party_size_ok_           = true;
    bool roster_snapshot_known_   = false;
    bool roster_snapshot_pending_ = false;
    ch::time_point<ch::system_clock> roster_snapshot_started_at_ { };
    bool advanced_logging_ = false;
    ch::time_point<ch::system_clock> run_started_at_ { };
    int current_zone_id_ = 0;
};

#endif // SHOTCALLERCPP_ENGINE_H
