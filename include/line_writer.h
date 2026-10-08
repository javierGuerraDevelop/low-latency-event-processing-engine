// Writes shotcall output to a timestamped text file under output/.

#ifndef SHOTCALLERCPP_LINE_WRITER_H
#define SHOTCALLERCPP_LINE_WRITER_H

#include <fstream>
#include <string>

#include "engine.h"

// Creates output/ directory and opens a timestamped file (YYYY-MM-DD_HH-MM-SS.txt).
// The ofstream lifetime is managed by the caller.
std::string open_output_file(std::ofstream& file);

// Returns a callback that writes one audit line per shotcall, including the
// scheduled cast time and whether the call was a prediction or a resync.
ShotCallCallback make_shotcall_writer(std::ofstream& file);

#endif // SHOTCALLERCPP_LINE_WRITER_H
