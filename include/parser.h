// Parses WoW combat log lines into structured CombatEvent data.
// Each line contains a timestamp, event type, source/target GUIDs, flags,
// and spell information. The parser extracts NPC IDs from Creature GUIDs
// for enemy identification.

#ifndef SHOTCALLERCPP_PARSER_H
#define SHOTCALLERCPP_PARSER_H

#include <chrono>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

struct CombatEvent {
    std::chrono::system_clock::time_point time_stamp;
    std::string event_type;
    std::string name;
    std::string source_id;
    std::string target_id;
    std::string source_raid_flag;
    std::string spell_name;
    std::string npc_id;
    int spell_id;
};

CombatEvent parse_line(const std::string& string);

#endif // SHOTCALLERCPP_PARSER_H
