#include "engine.h"

#include <chrono>
#include <map>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "constants.h"

namespace ch = std::chrono;

namespace {

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
    player.interrupt     = AbilityState { Constants::get_interrupt_id(class_name),
        Constants::get_interrupt_cd(class_name) };
    player.crowd_control = build_crowd_control_map(class_name);
}

} // namespace

void ShotCallEngine::handle_event(const CombatEvent& event)
{
    std::lock_guard<std::mutex> lock(mtx_);
    learn_player_name(event.source_id, event.name);
    learn_player_name(event.target_id, event.target_name);

    if (event.event_type == "UNIT_DIED" || event.event_type == "UNIT_DESTROYED") {
        handle_death(event);
        return;
    }

    if (event.event_type == "COMBATANT_INFO") {
        handle_combatant_info(event);
        return;
    }

    if (is_cast_by_party_member(event)) {
        handle_player_event(event);
    } else {
        handle_enemy_event(event);
    }
}

void ShotCallEngine::handle_death(const CombatEvent& event)
{
    if (auto it = roster_.find(event.target_id); it != roster_.end()) {
        it->second.is_alive = false;
        return;
    }

    if (auto it = enemy_roster_.find(event.target_id); it != enemy_roster_.end()) {
        const std::string enemy_guid = it->second.guid;
        shot_call_queue_.remove_if([&enemy_guid](const auto& shotcall) {
            return std::get<1>(shotcall) == enemy_guid;
        });
        enemy_roster_.erase(it);
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

    if (Constants::is_ignorable_event(event.event_type)) {
        return;
    }

    Player& player = player_iter->second;
    if (Constants::is_battle_rez(event.spell_id) && event.event_type == "SPELL_CAST_SUCCESS") {
        if (auto it = roster_.find(event.target_id); it != roster_.end()) {
            it->second.is_alive = true;
        }
    } else if (Constants::is_interrupt(event.spell_id)) {
        player.interrupt.on_cooldown_until = event.time_stamp + player.interrupt.cooldown;
    } else if (Constants::is_crowd_control(event.spell_id)) {
        if (auto it = player.crowd_control.find(event.spell_id); it != player.crowd_control.end()) {
            it->second.on_cooldown_until = event.time_stamp + it->second.cooldown;
        }
    }
}

void ShotCallEngine::handle_enemy_event(const CombatEvent& event)
{
    if (auto it = enemy_roster_.find(event.source_id); it == enemy_roster_.end()) {
        identify_enemy(event);
    }
}

void ShotCallEngine::handle_combatant_info(const CombatEvent& event)
{
    if (event.source_id.empty()) {
        return;
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
    player.spec_name  = std::string(Constants::get_spec_name(event.spec_id));
}

void ShotCallEngine::learn_player_name(const std::string& guid, std::string_view name)
{
    if (guid.empty() || name.empty()) {
        return;
    }

    const auto player_iter = roster_.find(guid);
    if (player_iter != roster_.end() && player_iter->second.name.empty()) {
        player_iter->second.name = std::string(name);
    }
}

void ShotCallEngine::generate_shotcalls(Enemy& enemy)
{
    for (size_t i = 0; i < enemy.spells.size(); ++i) {
        long long cd_ms = enemy.spells.at(i).cooldown.count();
        if (cd_ms <= 0) {
            continue;
        }

        long long five_minutes_ms = 300000;
        auto iterations           = (five_minutes_ms / cd_ms) + 1;
        for (long long j = 0; j < iterations; ++j) {
            ch::milliseconds duration { };
            if (j == 0) {
                duration = enemy.spells.at(i).first_cast;
            } else {
                duration = enemy.spells.at(i).first_cast + ch::milliseconds(j * cd_ms);
            }

            std::tuple<bool, std::string, std::string, ch::time_point<ch::system_clock>> shotcall {
                std::make_tuple(enemy.spells.at(i).is_interruptable, enemy.guid,
                    enemy.spells.at(i).callout, (enemy.first_seen_time + duration))
            };
            shot_call_queue_.push_back(shotcall);
        }
    }
    shot_call_queue_.sort([](const auto& a, const auto& b) {
        return std::get<3>(a) < std::get<3>(b);
    });
}

std::string ShotCallEngine::find_available_interrupter(
    const ch::time_point<ch::system_clock>& call_time)
{
    for (const auto& entry : roster_) {
        const Player& player = entry.second;
        if (!player.is_alive || player.interrupt.id == 0) {
            continue;
        }
        if (player.interrupt.on_cooldown_until <= call_time) {
            return player.name;
        }
    }
    return "this one is going off";
}

std::string ShotCallEngine::find_available_ccer(const ch::time_point<ch::system_clock>& call_time)
{
    for (const auto& entry : roster_) {
        const Player& player = entry.second;
        if (!player.is_alive) {
            continue;
        }

        for (const auto& ability : player.crowd_control) {
            if (ability.second.on_cooldown_until <= call_time) {
                return player.name;
            }
        }
    }
    return "this one is going off";
}

void ShotCallEngine::set_shotcall_callback(
    std::function<void(const std::string&, const std::string&)> callback)
{
    shotcall_callback_ = callback;
}

bool ShotCallEngine::dispatch_next_shotcall(ch::time_point<ch::system_clock> now)
{
    std::unique_lock<std::mutex> lock(mtx_);
    if (shot_call_queue_.empty()) {
        return false;
    }

    const bool interruptable   = std::get<0>(shot_call_queue_.front());
    const std::string enemy_id = std::get<1>(shot_call_queue_.front());
    const std::string callout  = std::get<2>(shot_call_queue_.front());
    const auto call_time       = std::get<3>(shot_call_queue_.front());
    auto time_until_call       = ch::duration_cast<ch::milliseconds>(call_time - now);
    if (time_until_call.count() < 0) {
        shot_call_queue_.pop_front();
        return false;
    }
    if (time_until_call.count() > 1000) {
        return false;
    }

    auto enemy_it = enemy_roster_.find(enemy_id);
    if (enemy_it == enemy_roster_.end()) {
        shot_call_queue_.pop_front();
        return false;
    }

    std::string available_player;
    if (interruptable) {
        available_player = find_available_interrupter(call_time);
    } else {
        if (!enemy_it->second.is_ccable) {
            shot_call_queue_.pop_front();
            return false;
        }
        available_player = find_available_ccer(call_time);
    }
    shot_call_queue_.pop_front();
    lock.unlock();
    if (shotcall_callback_) {
        std::string full_callout = callout;
        if (!available_player.empty()) {
            full_callout = available_player + " " + callout;
        }
        shotcall_callback_(enemy_id, full_callout);
    }

    return true;
}

void ShotCallEngine::process_shotcalls()
{
    while (true) {
        auto now = ch::system_clock::now();
        if (!dispatch_next_shotcall(now)) {
            std::this_thread::sleep_for(ch::milliseconds(250));
        } else {
            std::this_thread::sleep_for(ch::milliseconds(100));
        }
    }
}

void ShotCallEngine::identify_player(const CombatEvent& event)
{
    std::string class_name { Constants::get_class_from_identifying_spells(event.spell_id) };
    if (class_name.empty()) {
        class_name = std::string(Constants::get_class_from_interrupt_spell(event.spell_id));
    }
    if (class_name.empty()) {
        class_name = std::string(Constants::get_class_from_cc_spell(event.spell_id));
    }
    if (class_name.empty()) {
        return;
    }

    AbilityState interrupt { Constants::get_interrupt_id(class_name),
        Constants::get_interrupt_cd(class_name) };
    Player new_player { event.source_id, event.name, class_name, interrupt,
        build_crowd_control_map(class_name) };
    roster_.emplace(event.source_id, std::move(new_player));
}

void ShotCallEngine::identify_enemy(const CombatEvent& event)
{
    if (event.npc_id.empty() || !Constants::is_tracked_enemy(event.npc_id)) {
        return;
    }

    std::vector<EnemyAbility> spells;
    for (const auto& entry : Constants::enemy_data) {
        if (entry.enemy_id == event.npc_id) {
            spells.emplace_back(entry.spell_id, ch::milliseconds(entry.first_cast_ms),
                ch::milliseconds(entry.cooldown_ms), std::string(entry.callout),
                entry.is_interruptable);
        }
    }

    auto [enemy_iter, inserted] = enemy_roster_.emplace(event.source_id,
        Enemy { event.source_id, std::move(spells), event.time_stamp,
            Constants::is_enemy_ccable(event.npc_id) });
    if (inserted) {
        generate_shotcalls(enemy_iter->second);
    }
}
