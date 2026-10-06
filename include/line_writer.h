// Writes shotcall output to a timestamped text file under output/.

#ifndef SHOTCALLERCPP_LINE_WRITER_H
#define SHOTCALLERCPP_LINE_WRITER_H

#include <fstream>
#include <functional>
#include <string>

// Creates output/ directory and opens a timestamped file (YYYY-MM-DD_HH-MM-SS.txt).
// The ofstream lifetime is managed by the caller.
std::string open_output_file(std::ofstream& file);

// Returns a callback that writes callout text to file, one line per shotcall.
std::function<void(const std::string&, const std::string&)> make_shotcall_writer(
    std::ofstream& file);

#endif // SHOTCALLERCPP_LINE_WRITER_H
