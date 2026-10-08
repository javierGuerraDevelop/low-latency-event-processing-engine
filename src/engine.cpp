#include "engine.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <iostream>
#include <map>
#include <mutex>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "constants.h"

namespace ch = std::chrono;

namespace {

// How long a boundary keeps collecting COMBATANT_INFO records before the
// snapshot is finalized.
constexpr ch::milliseconds roster_snapshot_window { 500 };

// WoW unit flags: 0x1 = MINE, 0x2 = PARTY, 0x400 = TYPE_PLAYER. A party member
// has TYPE_PLAYER set and either MINE or PARTY affiliation.
bool is_cast_by_party_member(const CombatEvent& event)
{
    unsigned long flag = 0;
    try {
        flag = std::stoul(event.source_flags, nullptr, 16);
    } catch (...) {
        return false;
    }
    const bool is_player        = (flag & 0x400) != 0;
    const bool is_party_or_mine = (flag & 0x3) != 0;
    return is_player && is_party_or_mine;
}

// Enemy events that can start a timeline; auras and idle casts do not engage.
bool is_hostile_action(std::string_view event_type)
{
    constexpr std::array<std::string_view, 9> hostile_actions = {
        "SPELL_CAST_START",
        "SPELL_CAST_SUCCESS",
        "SPELL_DAMAGE",
        "SPELL_MISSED",
        "SPELL_PERIODIC_DAMAGE",
        "SWING_DAMAGE",
        "SWING_MISSED",
        "RANGE_DAMAGE",
        "RANGE_MISSED",
    };
    for (const auto& action : hostile_actions) {
        if (action == event_type) {
            return true;
        }
    }
    return false;
}

// Finds the scheduled ability matching a call; stale generations do not match.
EnemyAbilityRuntime* find_ability(
    Enemy& enemy, const Constants::EnemySpellProfile* profile, std::uint64_t generation)
{
    for (auto& ability : enemy.spells) {
        if (ability.profile == profile && ability.generation == generation) {
            return &ability;
        }
    }
    return nullptr;
}

// Builds the class's interrupt abilities with zeroed cooldowns.
std::map<int, AbilityState> build_interrupt_map(const std::string& class_name)
{
    std::map<int, AbilityState> abilities;
    for (const auto& [interrupt_class, spell_name, spell_id, cooldown] : Constants::interrupt_data) {
        if (interrupt_class == class_name) {
            abilities[spell_id] = AbilityState { spell_id, cooldown };
        }
    }
    return abilities;
}

// Builds the class's crowd-control abilities with zeroed cooldowns.
std::map<int, AbilityState> build_crowd_control_map(const std::string& class_name)
{
    std::map<int, AbilityState> abilities;
    for (const auto& [cc_class, spell_name, spell_id, cooldown] : Constants::crowd_control_data) {
        if (cc_class == class_name) {
            abilities[spell_id] = AbilityState { spell_id, cooldown };
        }
    }
    return abilities;
}

// Resets a player's interrupt and crowd-control state from the class tables.
void initialize_player_abilities(Player& player, const std::string& class_name)
{
    player.interrupts    = build_interrupt_map(class_name);
    player.crowd_control = build_crowd_control_map(class_name);
}

// Builds the spoken text for one dispatched call; all templates live here.
std::string format_spoken_call(
    const Constants::EnemySpellProfile& profile, const std::optional<Assignment>& assignment)
{
    const std::string callout { profile.callout };

    if (profile.mechanic == Constants::Mechanic::Kick) {
        if (!assignment) {
            return "this one is going off — " + callout + " soon";
        }
        const std::string prefix
            = assignment->player_name.empty() ? std::string { } : assignment->player_name + " ";
        return prefix + "kick " + callout + " soon";
    }
    if (profile.mechanic == Constants::Mechanic::Stun) {
        if (!assignment) {
            return "this one is going off — " + callout + " soon";
        }
        const std::string prefix
            = assignment->player_name.empty() ? std::string { } : assignment->player_name + " ";
        return prefix + "stop " + callout + " soon";
    }
    if (profile.mechanic == Constants::Mechanic::Dispel) {
        return "Dispel " + callout + " soon";
    }
    if (profile.mechanic == Constants::Mechanic::TankHit) {
        return "Tank " + callout + " soon";
    }
    return callout + " soon"; // Movement and Awareness only announce the cast.
}

} // namespace

ShotCallEngine::ShotCallEngine(ch::milliseconds call_lead, ch::milliseconds late_grace,
    ch::milliseconds reservation_window)
    : call_lead_ { call_lead }
    , late_grace_ { late_grace }
    , reservation_window_ { reservation_window }
{
}

void ShotCallEngine::handle_event(const CombatEvent& event)
{
    std::lock_guard<std::mutex> lock { mtx_ };
    learn_player_name(event.source_id, event.name);
    learn_player_name(event.target_id, event.target_name);

    if (event.event_type != "COMBATANT_INFO") {
        finalize_roster_snapshot_if_due(event.time_stamp, true);
    }

    if (event.event_type == "UNIT_DIED" || event.event_type == "UNIT_DESTROYED") {
        handle_death(event);
        return;
    }

    if (handle_boundary_event(event)) {
        wake_scheduler_locked();
        return;
    }

    if (event.event_type == "COMBATANT_INFO") {
        handle_combatant_info(event);
        return;
    }

    if (event.event_type == "SPELL_INTERRUPT") {
        resync_interrupted_enemy(event);
    }

    if (is_cast_by_party_member(event)) {
        handle_player_event(event);
    } else {
        handle_enemy_event(event);
    }
}

bool ShotCallEngine::handle_boundary_event(const CombatEvent& event)
{
    if (event.event_type == "CHALLENGE_MODE_START") {
        clear_enemies_and_calls();
        run_state_      = RunState::ChallengeActive;
        run_started_at_ = event.time_stamp;
        party_size_ok_  = true;
        begin_roster_snapshot(event.time_stamp);
        return true;
    }

    if (event.event_type == "CHALLENGE_MODE_END") {
        // The fixture contains an end-before-start pair; ignore it when idle.
        if (run_state_ == RunState::Idle) {
            return true;
        }
        clear_enemies_and_calls();
        run_state_        = RunState::Idle;
        encounter_active_ = false;
        party_size_ok_    = true;
        reset_roster_snapshot();
        return true;
    }

    if (event.event_type == "ENCOUNTER_START") {
        encounter_active_ = true;
        begin_roster_snapshot(event.time_stamp);
        party_size_ok_ = event.group_size == 5;
        if (!party_size_ok_) {
            std::cerr << "Encounter started with group size " << event.group_size
                      << " (expected 5); pausing shotcalls." << std::endl;
        }
        return true;
    }

    if (event.event_type == "ENCOUNTER_END") {
        encounter_active_ = false;
        return true;
    }

    if (event.event_type == "ZONE_CHANGE") {
        const bool zone_changed = event.instance_id != current_zone_id_;
        const bool in_challenge = run_state_ == RunState::ChallengeActive;
        if (zone_changed && encounter_active_ && !in_challenge) {
            clear_enemies_and_calls();
            encounter_active_ = false;
            party_size_ok_    = true;
            reset_roster_snapshot();
        }
        current_zone_id_ = event.instance_id;
        return true;
    }

    if (event.event_type == "COMBAT_LOG_VERSION") {
        advanced_logging_ = event.advanced_logging;
        return true;
    }

    return false;
}

void ShotCallEngine::begin_roster_snapshot(ch::time_point<ch::system_clock> started_at)
{
    run_roster_.clear();
    roster_snapshot_known_      = false;
    roster_snapshot_pending_    = true;
    roster_snapshot_started_at_ = started_at;
}

void ShotCallEngine::reset_roster_snapshot()
{
    run_roster_.clear();
    roster_snapshot_known_   = false;
    roster_snapshot_pending_ = false;
}

void ShotCallEngine::finalize_roster_snapshot_if_due(
    ch::time_point<ch::system_clock> now, bool event_followed)
{
    if (!roster_snapshot_pending_) {
        return;
    }
    if (!event_followed && now - roster_snapshot_started_at_ < roster_snapshot_window) {
        return;
    }

    roster_snapshot_pending_ = false;
    roster_snapshot_known_   = !run_roster_.empty();
}

bool ShotCallEngine::in_active_run() const
{
    return run_state_ == RunState::ChallengeActive || encounter_active_;
}

bool ShotCallEngine::player_in_current_run(const std::string& guid, const Player& player) const
{
    if (roster_snapshot_known_) {
        return run_roster_.count(guid) > 0;
    }
    return player.last_seen >= run_started_at_;
}

void ShotCallEngine::clear_enemies_and_calls()
{
    enemy_roster_.clear();
    shot_call_queue_.clear();
}

void ShotCallEngine::handle_death(const CombatEvent& event)
{
    if (auto it = roster_.find(event.target_id); it != roster_.end()) {
        it->second.is_alive = false;
        return;
    }

    if (auto it = enemy_roster_.find(event.target_id); it != enemy_roster_.end()) {
        // Queued calls are discarded lazily when their turn comes up.
        enemy_roster_.erase(it);
        wake_scheduler_locked();
    }
}

void ShotCallEngine::handle_player_event(const CombatEvent& event)
{
    auto player_iter = roster_.find(event.source_id);
    if (player_iter == roster_.end()) {
        identify_player(event);
        player_iter = roster_.find(event.source_id);
        if (player_iter == roster_.end()) {
            return;
        }
    }

    Player& player   = player_iter->second;
    player.last_seen = event.time_stamp;

    if (Constants::is_ignorable_event(event.event_type)) {
        return;
    }

    if (Constants::is_battle_rez(event.spell_id) && event.event_type == "SPELL_CAST_SUCCESS") {
        if (auto it = roster_.find(event.target_id); it != roster_.end()) {
            it->second.is_alive = true;
        }
    } else if (auto interrupt_it = player.interrupts.find(event.spell_id);
        interrupt_it != player.interrupts.end()) {
        interrupt_it->second.on_cooldown_until = event.time_stamp + interrupt_it->second.cooldown;
    } else if (auto cc_it = player.crowd_control.find(event.spell_id);
        cc_it != player.crowd_control.end()) {
        cc_it->second.on_cooldown_until = event.time_stamp + cc_it->second.cooldown;
    }
}

void ShotCallEngine::handle_enemy_event(const CombatEvent& event)
{
    if (!in_active_run() || !is_hostile_action(event.event_type)) {
        return;
    }

    auto enemy_iter = enemy_roster_.find(event.source_id);
    if (enemy_iter == enemy_roster_.end()) {
        identify_enemy(event);
        return;
    }

    resync_from_cast(enemy_iter->second, event);
}

void ShotCallEngine::handle_combatant_info(const CombatEvent& event)
{
    if (event.source_id.empty()) {
        return;
    }

    if (in_active_run()) {
        run_roster_.insert(event.source_id);
        if (roster_snapshot_pending_) {
            roster_snapshot_started_at_ = event.time_stamp;
        }
    }

    const std::string class_name { Constants::get_class_from_spec(event.spec_id) };
    if (class_name.empty()) {
        return;
    }

    auto [player_iter, inserted] = roster_.try_emplace(event.source_id);
    Player& player               = player_iter->second;
    if (inserted) {
        player.guid = event.source_id;
    }
    if (inserted || player.class_name != class_name) {
        initialize_player_abilities(player, class_name);
    }
    player.class_name = class_name;
    player.spec_id    = event.spec_id;
    player.spec_name  = std::string { Constants::get_spec_name(event.spec_id) };
    player.last_seen  = event.time_stamp;
}

void ShotCallEngine::learn_player_name(const std::string& guid, std::string_view name)
{
    if (guid.empty() || name.empty()) {
        return;
    }

    const auto player_iter = roster_.find(guid);
    if (player_iter != roster_.end() && player_iter->second.name.empty()) {
        player_iter->second.name = std::string { name };
    }
}

void ShotCallEngine::enqueue_shotcall_locked(
    const std::string& enemy_guid, const EnemyAbilityRuntime& ability, CallOrigin origin)
{
    shot_call_queue_.emplace(ability.next_due,
        ScheduledShotCall { enemy_guid, ability.profile, ability.next_due, ability.generation,
            origin });
    wake_scheduler_locked();
}

void ShotCallEngine::advance_recurrence(Enemy& enemy, EnemyAbilityRuntime& ability)
{
    ability.next_due += ability.profile->cooldown;
    ++ability.generation;
    enqueue_shotcall_locked(enemy.guid, ability, CallOrigin::Prediction);
}

void ShotCallEngine::resync_from_cast(Enemy& enemy, const CombatEvent& event)
{
    if (event.event_type != "SPELL_CAST_START" && event.event_type != "SPELL_CAST_SUCCESS") {
        return;
    }

    for (auto& ability : enemy.spells) {
        if (ability.profile->spell_id == event.spell_id) {
            reschedule_ability(enemy, ability, event.time_stamp);
            return;
        }
    }
}

void ShotCallEngine::resync_interrupted_enemy(const CombatEvent& event)
{
    auto enemy_iter = enemy_roster_.find(event.target_id);
    if (enemy_iter == enemy_roster_.end()) {
        return;
    }

    for (auto& ability : enemy_iter->second.spells) {
        if (ability.profile->spell_id == event.interrupted_spell_id) {
            reschedule_ability(enemy_iter->second, ability, event.time_stamp);
            return;
        }
    }
}

void ShotCallEngine::reschedule_ability(
    Enemy& enemy, EnemyAbilityRuntime& ability, ch::time_point<ch::system_clock> cast_time)
{
    ability.next_due = cast_time + ability.profile->cooldown;
    ++ability.generation;
    enqueue_shotcall_locked(enemy.guid, ability, CallOrigin::Resync);
}

void ShotCallEngine::wake_scheduler_locked()
{
    ++queue_revision_;
    wakeup_.notify_all();
}

std::optional<ch::time_point<ch::system_clock>> ShotCallEngine::next_actionable_time_locked() const
{
    if (!party_size_ok_ || shot_call_queue_.empty()) {
        return std::nullopt;
    }
    return shot_call_queue_.begin()->first - call_lead_;
}

std::optional<Assignment> ShotCallEngine::find_available_interrupter(
    const ch::time_point<ch::system_clock>& call_time)
{
    std::optional<Assignment> best;
    ch::time_point<ch::system_clock> best_ready { };
    for (const auto& entry : roster_) {
        const Player& player = entry.second;
        if (!player.is_alive || !player_in_current_run(entry.first, player)) {
            continue;
        }

        for (const auto& ability : player.interrupts) {
            if (ability.second.on_cooldown_until > call_time) {
                continue;
            }
            if (!best || ability.second.on_cooldown_until < best_ready) {
                best       = Assignment { entry.first, player.name, ability.first,
                    std::string { Constants::get_interrupt_name(ability.first) } };
                best_ready = ability.second.on_cooldown_until;
            }
        }
    }
    return best;
}

std::optional<Assignment> ShotCallEngine::find_available_ccer(
    const ch::time_point<ch::system_clock>& call_time)
{
    std::optional<Assignment> best;
    ch::time_point<ch::system_clock> best_ready { };
    for (const auto& entry : roster_) {
        const Player& player = entry.second;
        if (!player.is_alive || !player_in_current_run(entry.first, player)) {
            continue;
        }

        for (const auto& ability : player.crowd_control) {
            if (ability.second.on_cooldown_until > call_time) {
                continue;
            }
            if (!best || ability.second.on_cooldown_until < best_ready) {
                best       = Assignment { entry.first, player.name, ability.first,
                    std::string { Constants::get_crowd_control_name(ability.first) } };
                best_ready = ability.second.on_cooldown_until;
            }
        }
    }
    return best;
}

void ShotCallEngine::reserve_assignment(
    const Assignment& assignment, ch::time_point<ch::system_clock> due)
{
    auto player_iter = roster_.find(assignment.player_guid);
    if (player_iter == roster_.end()) {
        return;
    }

    Player& player        = player_iter->second;
    const auto reserve_in = [&](std::map<int, AbilityState>& abilities) {
        const auto ability_iter = abilities.find(assignment.spell_id);
        if (ability_iter == abilities.end()) {
            return false;
        }
        const auto reservation = std::min(
            reservation_window_, ch::duration_cast<ch::milliseconds>(ability_iter->second.cooldown));
        ability_iter->second.on_cooldown_until
            = std::max(ability_iter->second.on_cooldown_until, due + reservation);
        return true;
    };
    if (!reserve_in(player.interrupts)) {
        reserve_in(player.crowd_control);
    }
}

void ShotCallEngine::set_shotcall_callback(ShotCallCallback callback)
{
    shotcall_callback_ = std::move(callback);
}

PartyStatus ShotCallEngine::party_status() const
{
    std::lock_guard<std::mutex> lock { mtx_ };

    PartyStatus status;
    status.roster_known     = roster_snapshot_known_;
    status.advanced_logging = advanced_logging_;
    status.in_run           = in_active_run();
    status.expected         = roster_snapshot_known_ ? static_cast<int>(run_roster_.size()) : 5;

    int identified = 0;
    for (const auto& entry : roster_) {
        if (entry.second.class_name.empty() || !player_in_current_run(entry.first, entry.second)) {
            continue;
        }
        ++identified;
    }
    status.identified = identified;
    return status;
}

std::vector<DispatchedCall> ShotCallEngine::dispatch_due_locked(
    ch::time_point<ch::system_clock> now)
{
    finalize_roster_snapshot_if_due(now, false);
    if (!party_size_ok_) {
        return { };
    }

    std::vector<DispatchedCall> calls;
    auto call_iter = shot_call_queue_.begin();
    while (call_iter != shot_call_queue_.end()) {
        const ScheduledShotCall call = call_iter->second;
        auto enemy_iter              = enemy_roster_.find(call.enemy_guid);
        if (enemy_iter == enemy_roster_.end()) {
            call_iter = shot_call_queue_.erase(call_iter);
            continue;
        }

        EnemyAbilityRuntime* ability
            = find_ability(enemy_iter->second, call.profile, call.generation);
        if (ability == nullptr) {
            call_iter = shot_call_queue_.erase(call_iter);
            continue;
        }

        if (now > call.due + late_grace_) {
            advance_recurrence(enemy_iter->second, *ability);
            call_iter = shot_call_queue_.erase(call_iter);
            continue;
        }
        if (call.due - call_lead_ > now) {
            break;
        }

        std::optional<Assignment> assignment;
        switch (call.profile->mechanic) {
        case Constants::Mechanic::Kick:
            assignment = find_available_interrupter(call.due);
            break;
        case Constants::Mechanic::Stun:
            assignment = find_available_ccer(call.due);
            break;
        default:
            break;
        }
        if (assignment) {
            reserve_assignment(*assignment, call.due);
        }
        advance_recurrence(enemy_iter->second, *ability);
        call_iter = shot_call_queue_.erase(call_iter);
        calls.push_back(
            DispatchedCall { call, assignment, format_spoken_call(*call.profile, assignment) });
    }
    return calls;
}

std::size_t ShotCallEngine::dispatch_due(ch::time_point<ch::system_clock> now)
{
    std::vector<DispatchedCall> calls;
    {
        std::lock_guard<std::mutex> lock { mtx_ };
        calls = dispatch_due_locked(now);
    }

    for (const auto& dispatched : calls) {
        if (shotcall_callback_) {
            shotcall_callback_(dispatched);
        }
    }
    return calls.size();
}

void ShotCallEngine::process_shotcalls(std::stop_token stop_token)
{
    while (!stop_token.stop_requested()) {
        std::vector<DispatchedCall> calls;
        {
            std::unique_lock<std::mutex> lock { mtx_ };
            calls = dispatch_due_locked(ch::system_clock::now());

            if (calls.empty()) {
                const std::uint64_t revision = queue_revision_;
                const auto changed           = [this, revision] { return queue_revision_ != revision; };
                if (const auto deadline = next_actionable_time_locked(); deadline) {
                    wakeup_.wait_until(lock, stop_token, *deadline, changed);
                } else {
                    wakeup_.wait(lock, stop_token, changed);
                }
            }
        }

        for (const auto& dispatched : calls) {
            if (shotcall_callback_) {
                shotcall_callback_(dispatched);
            }
        }
    }
}

void ShotCallEngine::identify_player(const CombatEvent& event)
{
    std::string class_name { Constants::get_class_from_identifying_spells(event.spell_id) };
    if (class_name.empty()) {
        class_name = std::string { Constants::get_class_from_interrupt_spell(event.spell_id) };
    }
    if (class_name.empty()) {
        class_name = std::string { Constants::get_class_from_cc_spell(event.spell_id) };
    }
    if (class_name.empty()) {
        return;
    }

    Player new_player { event.source_id, event.name, class_name, build_interrupt_map(class_name),
        build_crowd_control_map(class_name) };
    new_player.last_seen = event.time_stamp;
    roster_.emplace(event.source_id, std::move(new_player));
}

void ShotCallEngine::identify_enemy(const CombatEvent& event)
{
    if (event.npc_id.empty() || !Constants::is_tracked_enemy(event.npc_id)) {
        return;
    }

    std::vector<EnemyAbilityRuntime> spells;
    for (const auto& profile : Constants::enemy_data) {
        if (profile.enemy_id == event.npc_id) {
            spells.push_back(EnemyAbilityRuntime {
                &profile, event.time_stamp + profile.first_cast, 1 });
        }
    }

    auto [enemy_iter, inserted] = enemy_roster_.emplace(event.source_id,
        Enemy { event.source_id, std::move(spells), event.time_stamp });
    if (inserted) {
        Enemy& enemy = enemy_iter->second;
        for (auto& ability : enemy.spells) {
            enqueue_shotcall_locked(enemy.guid, ability, CallOrigin::Prediction);
        }
    }
}
