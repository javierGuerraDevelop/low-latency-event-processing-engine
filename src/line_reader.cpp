#include "line_reader.h"

volatile std::sig_atomic_t stop_requested = 0;

namespace ch = std::chrono;

namespace {

// Strips a trailing carriage return and parses one line, feeding the engine.
// Returns true when an event was handled.
bool handle_log_line(std::string& line, ShotCallEngine& engine)
{
    if (!line.empty() && line.back() == '\r') {
        line.pop_back();
    }
    if (line.empty()) {
        return false;
    }
    if (auto event = parse_line(line)) {
        engine.handle_event(*event);
        return true;
    }
    return false;
}

} // namespace

std::string select_latest_log(const std::string& directory, const std::string& current_path)
{
    namespace fs = std::filesystem;

    std::string latest_file;
    fs::file_time_type latest_time;
    bool found = false;

    if (!fs::exists(directory) || !fs::is_directory(directory)) {
        return { };
    }

    for (const auto& entry : fs::directory_iterator(directory)) {
        if (!entry.is_regular_file()) {
            continue;
        }

        const std::string filename = entry.path().filename().string();
        if (filename.find("WoWCombatLog") == std::string::npos) {
            continue;
        }

        const auto file_time = entry.last_write_time();
        if (!found || file_time > latest_time) {
            latest_time = file_time;
            latest_file = entry.path().string();
            found       = true;
        }
    }

    if (!found) {
        return { };
    }
    if (latest_file == current_path) {
        return current_path;
    }
    return latest_file;
}

std::string get_latest_combat_log(const std::string& logs_directory)
{
    namespace fs = std::filesystem;

    if (!fs::exists(logs_directory) || !fs::is_directory(logs_directory)) {
        std::cerr << "Logs directory does not exist: " << logs_directory << std::endl;
        return "";
    }

    const std::string latest_file = select_latest_log(logs_directory);
    if (latest_file.empty()) {
        std::cerr << "No combat log files found in: " << logs_directory << std::endl;
        return "";
    }

    std::cout << "Found latest combat log: " << latest_file << std::endl;
    return latest_file;
}

std::size_t replay_file(const std::string& filename, ShotCallEngine& engine)
{
    std::ifstream file { filename };
    if (!file.is_open()) {
        std::cerr << "Failed to open combat log for replay: " << filename << std::endl;
        return 0;
    }

    std::size_t events = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (handle_log_line(line, engine)) {
            ++events;
        }
    }
    return events;
}

void monitor_file(const std::string& directory, const std::string& initial_file,
    ShotCallEngine& engine, std::stop_token stop_token, bool replay)
{
    std::ifstream log_file { initial_file };
    if (!log_file.is_open()) {
        std::cerr << "Failed to open combat log: " << initial_file << std::endl;
        return;
    }

    std::string current_file = initial_file;
    if (replay) {
        replay_file(current_file, engine);
    }
    log_file.clear();
    log_file.seekg(0, std::ios::end);
    std::cout << "Monitoring: " << current_file << std::endl;

    std::string line;
    auto last_rotation_check = ch::steady_clock::now();
    while (stop_requested == 0 && !stop_token.stop_requested()
        && !engine.strict_party_violation()) {
        if (std::getline(log_file, line)) {
            handle_log_line(line, engine);
            continue;
        }
        log_file.clear();

        const auto now = ch::steady_clock::now();
        if (now - last_rotation_check >= ch::seconds { 2 }) {
            last_rotation_check      = now;
            const std::string latest = select_latest_log(directory, current_file);
            if (!latest.empty() && latest != current_file) {
                std::ifstream rotated { latest };
                if (rotated.is_open()) {
                    log_file     = std::move(rotated);
                    current_file = latest;
                    std::cout << "Switched to new combat log: " << current_file << std::endl;
                    continue;
                }
            }
        }

        std::this_thread::sleep_for(ch::milliseconds { 250 });
    }
}
