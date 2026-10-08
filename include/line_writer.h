// Writes engine message audit output to a timestamped text file under output/.

#ifndef SHOTCALLERCPP_LINE_WRITER_H
#define SHOTCALLERCPP_LINE_WRITER_H

#include <fstream>
#include <string>

#include "engine.h"

// Creates output/ directory and opens a timestamped file (YYYY-MM-DD_HH-MM-SS.txt).
// The ofstream lifetime is managed by the caller.
std::string open_output_file(std::ofstream& file);

// Returns a callback that writes one audit line per message; status lines are
// prefixed with STATUS so they are easy to filter.
std::function<void(const EngineMessage&)> make_shotcall_writer(std::ofstream& file);

#endif // SHOTCALLERCPP_LINE_WRITER_H
