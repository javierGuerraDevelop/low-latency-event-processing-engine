#include "parser.h"

namespace ch = std::chrono;

// Parses "MM/DD/YYYY HH:MM:SS.mmm+TZ" into a system_clock time_point.
// Returns default time_point on malformed input.
ch::time_point<ch::system_clock> parse_timestamp(const std::string& timestamp)
{
    size_t time_zone_pos = timestamp.find_last_of("+-");
    if (time_zone_pos == std::string::npos) {
        return { };
    }

    std::string main_part     = timestamp.substr(0, time_zone_pos);
    int timezone_offset_hours = std::stoi(timestamp.substr(time_zone_pos));

    size_t millisecond_pos = main_part.find('.');
    if (millisecond_pos == std::string::npos) {
        return { };
    }

    std::string date_time_part = timestamp.substr(0, millisecond_pos);
    int milliseconds           = std::stoi(main_part.substr(millisecond_pos + 1));

    std::tm tm;
    std::stringstream stream { date_time_part };
    stream >> std::get_time(&tm, "%m/%d/%Y %H:%M:%S");

    time_t time     = std::mktime(&tm);
    auto time_point = ch::system_clock::from_time_t(time);
    time_point += ch::milliseconds(milliseconds);
    time_point -= ch::hours(timezone_offset_hours);

    return time_point;
}

void strip_quotes(std::string& str)
{
    str.erase(std::remove(str.begin(), str.end(), '"'), str.end());
}

// Extracts the NPC ID from a Creature GUID (e.g. "Creature-0-...-12345-...").
// Returns empty string for non-Creature GUIDs.
std::string extract_npc_id(const std::string& guid)
{
    if (guid.rfind("Creature", 0) != 0) {
        return { };
    }

    size_t pos = 0;
    for (int i = 0; i < 5; ++i) {
        pos = guid.find('-', pos);
        if (pos == std::string::npos) {
            return { };
        }
        ++pos;
    }

    size_t end = guid.find('-', pos);
    if (end == std::string::npos) {
        return { };
    }

    return guid.substr(pos, end - pos);
}

// Splits a combat log line by commas and extracts relevant fields.
// Lines with fewer than 11 fields are treated as unparseable.
CombatEvent parse_line(const std::string& line)
{
    if (line.empty()) {
        return CombatEvent { };
    }

    std::string part;
    std::vector<std::string> parts;
    std::stringstream stream { line };
    CombatEvent combat_event { };

    while (std::getline(stream, part, ',')) {
        parts.push_back(part);
    }

    if (parts.size() < 11) {
        return { };
    }

    size_t space_pos = parts[0].find("  ");
    if (space_pos != std::string::npos) {
        std::string timestamp_str = parts[0].substr(0, space_pos);
        combat_event.time_stamp   = parse_timestamp(timestamp_str);
        combat_event.event_type   = parts[0].substr(space_pos + 2);
    }

    std::stringstream name_stream { parts[2] };
    std::string player_name;
    std::getline(name_stream, player_name, '-');
    strip_quotes(player_name);

    combat_event.name             = player_name;
    combat_event.source_id        = parts[1];
    combat_event.source_raid_flag = parts[3];
    combat_event.target_id        = parts[5];
    combat_event.spell_name       = parts[10];
    combat_event.npc_id           = extract_npc_id(parts[1]);

    try {
        combat_event.spell_id = std::stoi(parts[9]);
    } catch (...) {
        combat_event.spell_id = 0;
    }

    return combat_event;
}
