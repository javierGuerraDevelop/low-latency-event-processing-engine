#include "protocol.h"

#include <chrono>
#include <string>
#include <string_view>

namespace ch = std::chrono;

namespace {

// Escapes a string for use inside a JSON double-quoted value.
std::string escape_json(std::string_view text)
{
    constexpr char hex_digits[] = "0123456789abcdef";

    std::string escaped;
    escaped.reserve(text.size());
    for (const char character : text) {
        switch (character) {
        case '"':
            escaped += "\\\"";
            break;
        case '\\':
            escaped += "\\\\";
            break;
        case '\b':
            escaped += "\\b";
            break;
        case '\f':
            escaped += "\\f";
            break;
        case '\n':
            escaped += "\\n";
            break;
        case '\r':
            escaped += "\\r";
            break;
        case '\t':
            escaped += "\\t";
            break;
        default:
            if (static_cast<unsigned char>(character) < 0x20) {
                const auto value = static_cast<unsigned int>(static_cast<unsigned char>(character));
                escaped += "\\u00";
                escaped += hex_digits[(value >> 4) & 0x0F];
                escaped += hex_digits[value & 0x0F];
            } else {
                escaped += character;
            }
        }
    }
    return escaped;
}

} // namespace

std::string serialize_message(const EngineMessage& message)
{
    if (message.type == MessageType::PartyStatus) {
        return "{\"v\":1,\"type\":\"party_status\",\"text\":\"" + escape_json(message.text)
            + "\"}\n";
    }

    const auto due_ms
        = ch::duration_cast<ch::milliseconds>(message.due.time_since_epoch()).count();
    return "{\"v\":1,\"type\":\"shotcall\",\"text\":\"" + escape_json(message.text)
        + "\",\"enemy\":\"" + escape_json(message.enemy_guid) + "\",\"spell\":"
        + std::to_string(message.spell_id) + ",\"mechanic\":\"" + escape_json(message.mechanic)
        + "\",\"due_ms\":" + std::to_string(due_ms) + ",\"call_id\":"
        + std::to_string(message.call_id) + "}\n";
}
