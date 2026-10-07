// Parses WoW combat log lines into structured CombatEvent data. Handles the
// variable-length records (including 10-field death records), quote-aware
// fields, boundary metadata, and COMBATANT_INFO snapshots.

#ifndef SHOTCALLERCPP_PARSER_H
#define SHOTCALLERCPP_PARSER_H

#include <chrono>
#include <optional>
#include <string>
#include <string_view>

struct CombatEvent {
    std::chrono::system_clock::time_point time_stamp { };
    std::string event_type;
    std::string name;
    std::string target_name;
    std::string source_id;
    std::string target_id;
    std::string source_flags;
    std::string npc_id;
    int spell_id { 0 };
    int interrupted_spell_id { 0 };
    int spec_id { 0 };
    int encounter_id { 0 };
    int group_size { 0 };
    int instance_id { 0 };
    int keystone_level { 0 };
    bool advanced_logging { false };
};

// Parses "M/D/YYYY H:MM:SS.mmm±H[.5]" as a UTC system_clock time point.
// Returns nullopt when the timestamp or its offset is malformed.
std::optional<std::chrono::system_clock::time_point> parse_timestamp(std::string_view timestamp);

// Parses one combat-log line. Returns nullopt for empty or malformed input.
std::optional<CombatEvent> parse_line(std::string_view line);

#endif // SHOTCALLERCPP_PARSER_H
