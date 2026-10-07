#include "parser.h"

#include <charconv>
#include <cstddef>
#include <system_error>
#include <vector>

namespace ch = std::chrono;

namespace {

// Parses a non-negative decimal integer, rejecting signs, trailing characters,
// and overflow.
std::optional<int> parse_uint(std::string_view text)
{
    if (text.empty()) {
        return std::nullopt;
    }

    int value               = 0;
    const char* first       = text.data();
    const char* last        = text.data() + text.size();
    const auto [ptr, error] = std::from_chars(first, last, value);
    if (error != std::errc { } || ptr != last) {
        return std::nullopt;
    }
    return value;
}

// Splits a line on commas. Double quotes group a field, "" is an escaped quote
// inside a quoted field, and surrounding quotes are removed.
std::vector<std::string> split_fields(std::string_view line)
{
    std::vector<std::string> fields;
    std::string current;
    bool in_quotes = false;

    for (std::size_t i = 0; i < line.size(); ++i) {
        const char character = line[i];
        if (in_quotes) {
            if (character == '"') {
                if (i + 1 < line.size() && line[i + 1] == '"') {
                    current.push_back('"');
                    ++i;
                } else {
                    in_quotes = false;
                }
            } else {
                current.push_back(character);
            }
        } else if (character == '"') {
            in_quotes = true;
        } else if (character == ',') {
            fields.push_back(std::move(current));
            current.clear();
        } else {
            current.push_back(character);
        }
    }
    fields.push_back(std::move(current));
    return fields;
}

std::string_view field_at(const std::vector<std::string>& fields, std::size_t index)
{
    return index < fields.size() ? std::string_view { fields[index] } : std::string_view { };
}

// Extracts the NPC ID from a Creature GUID (fifth dash-delimited segment).
// Returns an empty string for non-Creature GUIDs.
std::string extract_npc_id(std::string_view guid)
{
    if (guid.rfind("Creature", 0) != 0) {
        return { };
    }

    std::size_t pos = 0;
    for (int i = 0; i < 5; ++i) {
        pos = guid.find('-', pos);
        if (pos == std::string_view::npos) {
            return { };
        }
        ++pos;
    }

    const std::size_t end = guid.find('-', pos);
    if (end == std::string_view::npos) {
        return { };
    }

    return std::string(guid.substr(pos, end - pos));
}

// Returns the character name without its "-Realm" suffix.
std::string strip_realm(std::string_view name)
{
    return std::string(name.substr(0, name.find('-')));
}

// Returns the signed timezone offset in minutes, supporting integer and
// half-hour forms such as "+2", "-5", and "+5.5".
std::optional<int> parse_timezone_offset(std::string_view text)
{
    if (text.size() < 2 || (text.front() != '+' && text.front() != '-')) {
        return std::nullopt;
    }

    const bool negative         = text.front() == '-';
    std::string_view hours_text = text.substr(1);
    int half_hour_minutes       = 0;
    if (hours_text.size() > 2 && hours_text.substr(hours_text.size() - 2) == ".5") {
        half_hour_minutes = 30;
        hours_text        = hours_text.substr(0, hours_text.size() - 2);
    }

    const auto hours = parse_uint(hours_text);
    if (!hours) {
        return std::nullopt;
    }

    int offset_minutes = *hours * 60 + half_hour_minutes;
    if (negative) {
        offset_minutes = -offset_minutes;
    }
    if (offset_minutes < -12 * 60 || offset_minutes > 14 * 60) {
        return std::nullopt;
    }
    return offset_minutes;
}

// The spec ID is the numeric token before the talent-tree array; the anchor
// survives the field inserted in 12.0+, where fields[24] would shift.
int parse_spec_id(std::string_view line, const std::vector<std::string>& fields)
{
    const std::size_t anchor = line.find("[(");
    if (anchor != std::string_view::npos) {
        std::size_t end = anchor;
        if (end > 0 && line[end - 1] == ',') {
            --end;
        }

        std::size_t begin = end;
        while (begin > 0 && line[begin - 1] >= '0' && line[begin - 1] <= '9') {
            --begin;
        }

        if (begin < end) {
            if (const auto spec = parse_uint(line.substr(begin, end - begin))) {
                return *spec;
            }
        }
    }

    return parse_uint(field_at(fields, 24)).value_or(0);
}

} // namespace

std::optional<ch::system_clock::time_point> parse_timestamp(std::string_view timestamp)
{
    constexpr std::size_t npos = std::string_view::npos;

    const std::size_t offset_pos = timestamp.find_last_of("+-");
    if (offset_pos == npos || offset_pos == 0) {
        return std::nullopt;
    }

    const auto offset_minutes = parse_timezone_offset(timestamp.substr(offset_pos));
    if (!offset_minutes) {
        return std::nullopt;
    }

    const std::string_view main = timestamp.substr(0, offset_pos);
    const std::size_t dot_pos   = main.find('.');
    const std::size_t space_pos = main.find(' ');
    if (dot_pos == npos || space_pos == npos || space_pos > dot_pos) {
        return std::nullopt;
    }

    const std::string_view date = main.substr(0, space_pos);
    const std::string_view time = main.substr(space_pos + 1, dot_pos - space_pos - 1);
    const auto milliseconds     = parse_uint(main.substr(dot_pos + 1));
    if (!milliseconds || *milliseconds > 999) {
        return std::nullopt;
    }

    const std::size_t first_slash = date.find('/');
    const std::size_t second_slash
        = first_slash == npos ? npos : date.find('/', first_slash + 1);
    const std::size_t first_colon = time.find(':');
    const std::size_t second_colon
        = first_colon == npos ? npos : time.find(':', first_colon + 1);
    if (first_slash == npos || second_slash == npos || first_colon == npos || second_colon == npos) {
        return std::nullopt;
    }

    const auto month  = parse_uint(date.substr(0, first_slash));
    const auto day    = parse_uint(date.substr(first_slash + 1, second_slash - first_slash - 1));
    const auto year   = parse_uint(date.substr(second_slash + 1));
    const auto hour   = parse_uint(time.substr(0, first_colon));
    const auto minute = parse_uint(time.substr(first_colon + 1, second_colon - first_colon - 1));
    const auto second = parse_uint(time.substr(second_colon + 1));
    if (!month || !day || !year || !hour || !minute || !second) {
        return std::nullopt;
    }
    if (*hour > 23 || *minute > 59 || *second > 59) {
        return std::nullopt;
    }

    const ch::year_month_day date_parts { ch::year { *year },
        ch::month { static_cast<unsigned>(*month) }, ch::day { static_cast<unsigned>(*day) } };
    if (!date_parts.ok()) {
        return std::nullopt;
    }

    const auto local_time = ch::system_clock::time_point { ch::sys_days { date_parts } }
        + ch::hours { *hour } + ch::minutes { *minute } + ch::seconds { *second }
        + ch::milliseconds { *milliseconds };
    return local_time - ch::minutes { *offset_minutes };
}

std::optional<CombatEvent> parse_line(std::string_view line)
{
    if (line.empty()) {
        return std::nullopt;
    }

    const std::vector<std::string> fields = split_fields(line);
    const std::string_view head           = field_at(fields, 0);
    const std::size_t separator           = head.find("  ");
    if (separator == std::string_view::npos) {
        return std::nullopt;
    }

    const auto timestamp = parse_timestamp(head.substr(0, separator));
    if (!timestamp) {
        return std::nullopt;
    }

    const std::string_view event_type = head.substr(separator + 2);
    if (event_type.empty()) {
        return std::nullopt;
    }

    CombatEvent event { };
    event.time_stamp   = *timestamp;
    event.event_type   = std::string(event_type);
    event.source_id    = std::string(field_at(fields, 1));
    event.name         = strip_realm(field_at(fields, 2));
    event.source_flags = std::string(field_at(fields, 3));
    event.target_id    = std::string(field_at(fields, 5));
    event.npc_id       = extract_npc_id(field_at(fields, 1));

    // Spell payload columns only exist on spell events; a non-numeric value
    // such as PARRY in SWING_MISSED yields 0 without rejecting the record.
    if (fields.size() > 9) {
        event.spell_id = parse_uint(field_at(fields, 9)).value_or(0);
    }
    // The interrupted spell ID follows the spell school column in real
    // SPELL_INTERRUPT records.
    if (event.event_type == "SPELL_INTERRUPT" && fields.size() > 12) {
        event.interrupted_spell_id = parse_uint(field_at(fields, 12)).value_or(0);
    }

    const auto field_value = [&fields](std::size_t index) {
        return parse_uint(field_at(fields, index)).value_or(0);
    };

    if (event.event_type == "ENCOUNTER_START") {
        event.encounter_id = field_value(1);
        event.group_size   = field_value(4);
        event.instance_id  = field_value(5);
    } else if (event.event_type == "ENCOUNTER_END") {
        event.encounter_id = field_value(1);
        event.group_size   = field_value(4);
    } else if (event.event_type == "CHALLENGE_MODE_START") {
        event.instance_id    = field_value(2);
        event.keystone_level = field_value(4);
    } else if (event.event_type == "CHALLENGE_MODE_END") {
        event.instance_id = field_value(1);
    } else if (event.event_type == "ZONE_CHANGE") {
        event.instance_id = field_value(1);
    } else if (event.event_type == "COMBAT_LOG_VERSION") {
        event.advanced_logging = field_at(fields, 3) == "1";
    } else if (event.event_type == "COMBATANT_INFO") {
        event.spec_id = parse_spec_id(line, fields);
        event.name.clear();
    }

    return event;
}
