// Locates the most recent WoW combat log file and tail-follows it,
// feeding parsed events into the ShotCallEngine.

#ifndef SHOTCALLERCPP_LINE_READER_H
#define SHOTCALLERCPP_LINE_READER_H

#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "engine.h"

// Returns the path of the newest WoWCombatLog* file in the directory, or an
// empty string when none exists. Returns current_path when it is still newest.
std::string select_latest_log(const std::string& directory, const std::string& current_path = "");

// Returns the path to the most recently modified WoWCombatLog file in the directory.
std::string get_latest_combat_log(const std::string& logs_directory);

// Parses every complete line of a log file from the start, feeding events to
// the engine; returns the number of events handled.
std::size_t replay_file(const std::string& filename, ShotCallEngine& engine);

// Tail-follows a combat log, parsing new lines and dispatching events to the engine.
void monitor_file(const std::string& filename, ShotCallEngine& engine);

#endif // SHOTCALLERCPP_LINE_READER_H
