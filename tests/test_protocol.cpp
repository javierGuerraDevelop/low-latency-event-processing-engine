#include <gtest/gtest.h>

#include <algorithm>
#include <chrono>
#include <string>

#include "protocol.h"

namespace ch = std::chrono;

TEST(Protocol, ShotCallFrameIsOneJsonLine)
{
    EngineMessage message;
    message.type       = MessageType::ShotCall;
    message.text       = "Tank kick DoT soon";
    message.enemy_guid = "Creature-0-4218-2661-9671-210269-0000CDF1A1";
    message.mechanic   = "kick";
    message.spell_id   = 463218;
    message.due        = ch::system_clock::time_point { ch::milliseconds { 1750000000000 } };
    message.call_id    = 42;

    EXPECT_EQ(serialize_message(message),
        "{\"v\":1,\"type\":\"shotcall\",\"text\":\"Tank kick DoT soon\",\"enemy\":"
        "\"Creature-0-4218-2661-9671-210269-0000CDF1A1\",\"spell\":463218,\"mechanic\":"
        "\"kick\",\"due_ms\":1750000000000,\"call_id\":42}\n");
}

TEST(Protocol, PartyStatusFrameIsOneJsonLine)
{
    EngineMessage message;
    message.type = MessageType::PartyStatus;
    message.text = "3/5 players identified. Use your class ability to identify yourself.";

    EXPECT_EQ(serialize_message(message),
        "{\"v\":1,\"type\":\"party_status\",\"text\":\"3/5 players identified. Use your class "
        "ability to identify yourself.\"}\n");
}

TEST(Protocol, EscapesQuotesBackslashesAndControlCharacters)
{
    EngineMessage message;
    message.type = MessageType::ShotCall;
    message.text = "He said \"hi\"\\ then\nleft\t";
    message.text.push_back('\x01');

    const std::string frame = serialize_message(message);
    EXPECT_NE(frame.find("\\\"hi\\\""), std::string::npos);
    EXPECT_NE(frame.find("\\\\ then"), std::string::npos);
    EXPECT_NE(frame.find("\\nleft"), std::string::npos);
    EXPECT_NE(frame.find("\\t"), std::string::npos);
    EXPECT_NE(frame.find("\\u0001"), std::string::npos);

    // Exactly one real newline: the frame terminator.
    EXPECT_EQ(std::count(frame.begin(), frame.end(), '\n'), 1);
    EXPECT_EQ(frame.back(), '\n');
}
