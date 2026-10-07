#include "engine.h"

#include <mutex>

#include "constants.h"

namespace ch = std::chrono;

std::map<int, AbilityState> get_crowd_control_m(const std::string& p_class);

bool is_cast_by_party_member(const CombatEvent& event)
{
    // WoW unit flags: 0x1 = MINE, 0x2 = PARTY, 0x400 = TYPE_PLAYER
    // A party member player has TYPE_PLAYER set and either MINE or PARTY affiliation
    unsigned long flag = 0;
    try {
        flag = std::stoul(event.source_flags, nullptr, 16);
    } catch (...) {
        return false;
    }
    bool is_player        = (flag & 0x400) != 0;
    bool is_party_or_mine = (flag & 0x3) != 0;
    return is_player && is_party_or_mine;
}

void ShotCallEngine::handle_event(const CombatEvent& event)
{
    std::lock_guard<std::mutex> lock(mtx_);
    if (event.event_type == "UNIT_DIED" || event.event_type == "UNIT_DESTROYED") {
        handle_death(event);
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
        std::string enemy_id = it->second.id;
        shot_call_queue_.remove_if([&enemy_id](const auto& shotcall) {
            return std::get<1>(shotcall) == enemy_id;
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
    }

    if (Constants::is_ignorable_event(event.event_type)) {
        return;
    }

    if (Constants::is_battle_rez(event.spell_id) && event.event_type == "SPELL_CAST_SUCCESS") {
        if (auto it = roster_.find(event.target_id); it != roster_.end()) {
            it->second.is_alive = true;
        }
    } else if (Constants::is_interrupt(event.spell_id)) {
        auto& player_interrupt             = roster_interrupts_[player_iter->first];
        auto player_interrupt_cd           = player_interrupt.cooldown;
        player_interrupt.on_cooldown_until = event.time_stamp + player_interrupt_cd;
    } else if (Constants::is_crowd_control(event.spell_id)) {
        auto& player_crowd_control_map = roster_crowd_control_[player_iter->first];
        if (auto it = player_crowd_control_map.find(event.spell_id);
            it != player_crowd_control_map.end()) {
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
                std::make_tuple(enemy.spells.at(i).is_interruptable, enemy.id,
                    enemy.spells.at(i).callout, (enemy.combat_start_time + duration))
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
    for (const auto& [player_id, player] : roster_) {
        if (!player.is_alive) {
            continue;
        }

        auto interrupt_iter = roster_interrupts_.find(player_id);
        if (interrupt_iter == roster_interrupts_.end()) {
            continue;
        }

        if (interrupt_iter->second.on_cooldown_until <= call_time) {
            return player.name;
        }
    }
    return "this one is going off";
}

std::string ShotCallEngine::find_available_ccer(const ch::time_point<ch::system_clock>& call_time)
{
    for (const auto& [player_id, player] : roster_) {
        if (!player.is_alive) {
            continue;
        }

        auto cc_iter = roster_crowd_control_.find(player_id);
        if (cc_iter == roster_crowd_control_.end()) {
            continue;
        }

        for (auto& [idx, spell] : cc_iter->second) {
            if (spell.on_cooldown_until <= call_time) {
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
    std::string p_class { Constants::get_class_from_identifying_spells(event.spell_id) };
    if (p_class.empty()) {
        return;
    }

    int interrupt_id               = Constants::get_interrupt_id(p_class);
    ch::seconds interrupt_cooldown = Constants::get_interrupt_cd(p_class);
    AbilityState interrupt { interrupt_id, interrupt_cooldown };
    std::map<int, AbilityState> crowd_control_m = get_crowd_control_m(p_class);
    Player new_player { event.source_id, event.name, p_class, interrupt, crowd_control_m };
    roster_.emplace(event.source_id, new_player);
    roster_interrupts_.emplace(event.source_id, interrupt);
    roster_crowd_control_.emplace(event.source_id, crowd_control_m);
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
    bool is_ccable = Constants::is_enemy_ccable(event.npc_id);
    Enemy new_enemy { event.source_id, spells, event.time_stamp, is_ccable };
    enemy_roster_.emplace(event.source_id, new_enemy);
    ShotCallEngine::generate_shotcalls(new_enemy);
}

std::map<int, AbilityState> get_crowd_control_m(const std::string& p_class)
{
    std::map<int, AbilityState> new_map { };
    for (const auto& [class_name, spell_name, id, cd] : Constants::crowd_control_data) {
        if (class_name == p_class) {
            new_map[id] = AbilityState { id, cd };
        }
    }
    return new_map;
}
