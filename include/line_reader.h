// Locates the most recent WoW combat log file and tail-follows it,
// feeding parsed events into the ShotCallEngine.

#ifndef SHOTCALLERCPP_LINE_READER_H
#define SHOTCALLERCPP_LINE_READER_H

#include <chrono>
#include <csignal>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stop_token>
#include <string>
#include <thread>

#include "engine.h"

// Set by the SIGINT/SIGTERM handler; monitor_file returns when it is nonzero.
extern volatile std::sig_atomic_t stop_requested;

// Returns the path of the newest WoWCombatLog* file in the directory, or an
// empty string when none exists. Returns current_path when it is still newest.
std::string select_latest_log(const std::string& directory, const std::string& current_path = "");

// Returns the path to the most recently modified WoWCombatLog file in the directory.
std::string get_latest_combat_log(const std::string& logs_directory);

// Parses every complete line of a log file from the start, feeding events to
// the engine; returns the number of events handled.
std::size_t replay_file(const std::string& filename, ShotCallEngine& engine);

// Tail-follows the newest combat log until stop_requested or stop_token fires,
// switching to newer log files as they appear. When replay is true the initial
// file is read from the beginning before tailing.
void monitor_file(const std::string& directory, const std::string& initial_file,
    ShotCallEngine& engine, std::stop_token stop_token, bool replay);

#endif // SHOTCALLERCPP_LINE_READER_H
