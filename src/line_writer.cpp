#include "line_writer.h"

#include <chrono>
#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace ch = std::chrono;

namespace {

// Formats the scheduled cast time as a UTC timestamp with milliseconds.
std::string format_scheduled_time(ch::system_clock::time_point due)
{
    const auto millis         = ch::duration_cast<ch::milliseconds>(due.time_since_epoch()).count() % 1000;
    const std::time_t seconds = ch::system_clock::to_time_t(due);
    std::tm utc { };
    gmtime_r(&seconds, &utc);

    std::ostringstream formatted;
    formatted << std::put_time(&utc, "%Y-%m-%d %H:%M:%S") << "." << std::setw(3)
              << std::setfill('0') << millis;
    return formatted.str();
}

const char* origin_name(CallOrigin origin)
{
    return origin == CallOrigin::Resync ? "resync" : "prediction";
}

} // namespace

std::string open_output_file(std::ofstream& file)
{
    namespace fs = std::filesystem;
    fs::create_directories("output");

    std::time_t now = std::time(nullptr);
    std::tm tm      = *std::localtime(&now);
    std::ostringstream oss;
    oss << "output/" << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S") << ".txt";
    std::string path = oss.str();

    file.open(path);
    if (!file.is_open()) {
        std::cerr << "Failed to open output file: " << path << std::endl;
    }

    return path;
}

ShotCallCallback make_shotcall_writer(std::ofstream& file)
{
    return [&file](const ScheduledShotCall& call, const std::string& text) {
        file << "[" << format_scheduled_time(call.due) << " UTC] [" << origin_name(call.origin)
             << "] " << text << "\n";
        file.flush();
    };
}
