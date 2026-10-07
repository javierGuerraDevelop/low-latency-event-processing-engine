#include "line_reader.h"

namespace ch = std::chrono;

std::string get_latest_combat_log(const std::string& logs_directory)
{
    namespace fs = std::filesystem;

    std::string latest_file;
    fs::file_time_type latest_time;
    bool found = false;

    if (!fs::exists(logs_directory) || !fs::is_directory(logs_directory)) {
        std::cerr << "Logs directory does not exist: " << logs_directory << std::endl;
        return "";
    }

    for (const auto& entry : fs::directory_iterator(logs_directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        std::string filename = entry.path().filename().string();
        if (filename.find("WoWCombatLog") == std::string::npos) {
            continue;
        }

        auto file_time = entry.last_write_time();
        if (!found || file_time > latest_time) {
            latest_time = file_time;
            latest_file = entry.path().string();
            found       = true;
        }
    }

    if (!found) {
        std::cerr << "No combat log files found in: " << logs_directory << std::endl;
        return "";
    }

    std::cout << "Found latest combat log: " << latest_file << std::endl;
    return latest_file;
}

void monitor_file(const std::string& filename, ShotCallEngine& engine)
{
    std::ifstream log_file { filename };
    if (!log_file.is_open()) {
        std::cerr << "Failed to open combat log: " << filename << std::endl;
        return;
    }
    log_file.seekg(0, std::ios::end);
    std::cout << "Monitoring: " << filename << std::endl;

    std::string line;
    while (true) {
        if (std::getline(log_file, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            if (line.empty()) {
                continue;
            }
            if (auto event = parse_line(line)) {
                engine.handle_event(*event);
            }
        } else {
            log_file.clear();
            log_file.seekg(0, std::ios::cur);
            std::this_thread::sleep_for(ch::milliseconds { 250 });
        }
    }
}
