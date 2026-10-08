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

namespace Constants {
struct EnemySpellProfile;
}

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

// Runtime scheduling state for one enemy ability; profile data stays immutable.
struct EnemyAbilityRuntime {
    const Constants::EnemySpellProfile* profile = nullptr;
    ch::time_point<ch::system_clock> next_due { };
    std::uint64_t generation = 0;
};

// How a scheduled call was derived from the enemy's timeline.
enum class CallOrigin { Prediction,
    Resync };

// One predicted cast waiting for its announcement window.
struct ScheduledShotCall {
    std::string enemy_guid;
    const Constants::EnemySpellProfile* profile = nullptr;
    ch::time_point<ch::system_clock> due;
    std::uint64_t generation;
    CallOrigin origin = CallOrigin::Prediction;
};

// The party member and ability chosen to answer a call.
struct Assignment {
    std::string player_guid;
    std::string player_name;
    int spell_id = 0;
    std::string spell_name;
};

// Message kinds emitted by the engine.
enum class MessageType : std::uint8_t { ShotCall,
    PartyStatus };

// One message for the outside world: a shotcall or a party status update.
struct EngineMessage {
    MessageType type = MessageType::ShotCall;
    std::string text;
    std::string enemy_guid;
    std::string mechanic;
    int spell_id = 0;
    ch::system_clock::time_point due;
    std::uint64_t call_id = 0;
    CallOrigin origin     = CallOrigin::Prediction;
    std::optional<Assignment> assignment;
};

// Callback invoked for each message, outside the engine mutex.
using MessageCallback = std::function<void(const EngineMessage&)>;

// One dispatched call with its formatted text and optional assignment.
struct DispatchedCall {
    ScheduledShotCall call;
    std::optional<Assignment> assignment;
    std::string text;
};

// Callback invoked for each dispatched call, outside the engine mutex.
using ShotCallCallback = std::function<void(const DispatchedCall&)>;

// A party member known by GUID. The name may stay empty until an event
// carrying it is seen; the class comes from an action or COMBATANT_INFO.
struct Player {
    std::string guid;
    std::string name;
    std::string class_name;
    int spec_id = 0;
    std::string spec_name;
    std::map<int, AbilityState> interrupts;
    std::map<int, AbilityState> crowd_control;
    bool is_alive = true;
    // Timestamp of the most recent event carrying this player's GUID.
    ch::time_point<ch::system_clock> last_seen { };

    Player(std::string player_guid, std::string player_name, std::string player_class,
        std::map<int, AbilityState> interrupt_abilities,
        std::map<int, AbilityState> crowd_control_abilities)
        : guid { std::move(player_guid) }
        , name { std::move(player_name) }
        , class_name { std::move(player_class) }
        , interrupts { std::move(interrupt_abilities) }
        , crowd_control { std::move(crowd_control_abilities) }
    {
    }

    Player() = default;
};

// A tracked enemy and the abilities generated from its static profile.
struct Enemy {
    std::string guid;
    std::vector<EnemyAbilityRuntime> spells;
    ch::time_point<ch::system_clock> first_seen_time;

    Enemy(std::string enemy_guid, std::vector<EnemyAbilityRuntime> abilities,
        ch::time_point<ch::system_clock> first_seen)
        : guid { std::move(enemy_guid) }
        , spells { std::move(abilities) }
        , first_seen_time { first_seen }
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
    // call_lead is how early a call may fire; late_grace is how late it may
    // fire; reservation_window reserves an assigned ability after dispatch.
    explicit ShotCallEngine(ch::milliseconds call_lead = ch::milliseconds { 2500 },
        ch::milliseconds late_grace                    = ch::milliseconds { 1000 },
        ch::milliseconds reservation_window            = ch::milliseconds { 3000 });
    // Routes incoming combat events to the appropriate handler.
    void handle_event(const CombatEvent& event);
    void set_shotcall_callback(ShotCallCallback callback);
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
    std::vector<DispatchedCall> dispatch_due_locked(
        ch::time_point<ch::system_clock> now);
    // Resyncs the enemy's matching ability from a real cast; unknown spells are ignored.
    void resync_from_cast(Enemy& enemy, const CombatEvent& event);
    // Resyncs the interrupted ability of the event's target enemy.
    void resync_interrupted_enemy(const CombatEvent& event);
    // Reschedules one ability at cast_time + cooldown and queues the call.
    void reschedule_ability(
        Enemy& enemy, EnemyAbilityRuntime& ability, ch::time_point<ch::system_clock> cast_time);
    // Queues the ability's next occurrence and wakes the scheduler thread.
    void enqueue_shotcall_locked(
        const std::string& enemy_guid, const EnemyAbilityRuntime& ability, CallOrigin origin);
    // Moves an ability one cooldown forward and queues the next occurrence.
    void advance_recurrence(Enemy& enemy, EnemyAbilityRuntime& ability);
    // Wakes the scheduler after a schedule or enemy change.
    void wake_scheduler_locked();
    // Earliest time the scheduler should wake, if any call can fire.
    std::optional<ch::time_point<ch::system_clock>> next_actionable_time_locked() const;
    // Auto-detects player class or enemy type from combat event spells.
    void identify_player(const CombatEvent& event);
    void identify_enemy(const CombatEvent& event);
    // Fills a roster player's name if it is still empty.
    void learn_player_name(const std::string& guid, std::string_view name);
    // Returns a living player and the earliest-ready ability for the call, or
    // nullopt if none is available. The player's name may be empty.
    std::optional<Assignment> find_available_interrupter(
        const ch::time_point<ch::system_clock>& call_time);
    std::optional<Assignment> find_available_ccer(
        const ch::time_point<ch::system_clock>& call_time);
    // Extends the assigned ability's cooldown to reserve it for the call.
    void reserve_assignment(const Assignment& assignment, ch::time_point<ch::system_clock> due);

    const ch::milliseconds call_lead_;
    const ch::milliseconds late_grace_;
    const ch::milliseconds reservation_window_;
    ShotCallCallback shotcall_callback_;
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
