// Locates the most recent WoW combat log file and tail-follows it,
// feeding parsed events into the ShotCallEngine.

#ifndef SHOTCALLERCPP_LINE_READER_H
#define SHOTCALLERCPP_LINE_READER_H

#include <chrono>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "engine.h"

// Tail-follows a combat log, parsing new lines and dispatching events to the engine.
void monitor_file(const std::string& filename, ShotCallEngine& engine);
// Returns the path to the most recently modified WoWCombatLog file in the directory.
std::string get_latest_combat_log(const std::string& logs_directory);

#endif // SHOTCALLERCPP_LINE_READER_H
