// Entry point. Resolves the combat log, then tail-follows it while the
// scheduler runs on a background thread until a stop signal arrives.

#include <chrono>
#include <csignal>
#include <fstream>
#include <iostream>
#include <string>
#include <thread>

#include "engine.h"
#include "line_reader.h"
#include "line_writer.h"
#include "socket_sender.h"

namespace ch = std::chrono;

namespace {

// Set by the SIGINT/SIGTERM handler and polled by monitor_file.
void handle_stop_signal(int)
{
    stop_requested = 1;
}

void print_usage(const char* program)
{
    std::cerr << "Usage: " << program << " [--replay] [--strict-party-size] <logs_directory>"
              << std::endl;
}

} // namespace

int main(int argc, char* argv[])
{
    std::string logs_directory;
    bool replay            = false;
    bool strict_party_size = false;

    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--replay") {
            replay = true;
        } else if (argument == "--strict-party-size") {
            strict_party_size = true;
        } else if (logs_directory.empty()) {
            logs_directory = argument;
        } else {
            print_usage(argv[0]);
            return 1;
        }
    }

    if (logs_directory.empty()) {
        print_usage(argv[0]);
        return 1;
    }

    // Resolve the log before starting any thread, so a missing file cannot
    // leave a joinable scheduler behind.
    const std::string combat_log_file = get_latest_combat_log(logs_directory);
    if (combat_log_file.empty()) {
        return 1;
    }

    // Open timestamped output file
    std::ofstream output_file;
    std::string output_path = open_output_file(output_file);
    if (!output_file.is_open()) {
        return 1;
    }
    std::cout << "Writing shotcalls to: " << output_path << std::endl;

    // Wire engine callback to write messages to file and send over socket
    auto file_writer   = make_shotcall_writer(output_file);
    auto socket_sender = make_socket_sender();

    ShotCallEngine engine { ch::milliseconds { 2500 }, ch::milliseconds { 1000 },
        ch::milliseconds { 3000 }, ch::milliseconds { 25000 }, strict_party_size };
    engine.set_shotcall_callback([file_writer, socket_sender](const EngineMessage& message) {
        file_writer(message);
        socket_sender(message);
    });

    // Run process_shotcalls on a background thread
    std::jthread shotcall_thread { [&engine](std::stop_token stop_token) {
        engine.process_shotcalls(stop_token);
    } };

    std::signal(SIGINT, handle_stop_signal);
    std::signal(SIGTERM, handle_stop_signal);

    monitor_file(logs_directory, combat_log_file, engine, shotcall_thread.get_stop_token(), replay);

    shotcall_thread.request_stop();
    if (engine.strict_party_violation()) {
        std::cerr << "Fewer than 5 players identified; exiting." << std::endl;
        return 2;
    }
    return 0;
}
