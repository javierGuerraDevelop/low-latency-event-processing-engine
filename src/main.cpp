// Entry point. Sets up output file and socket callbacks, starts the shotcall
// processing thread, then tail-follows the latest WoW combat log.

#include <fstream>
#include <iostream>
#include <thread>

#include "engine.h"
#include "line_reader.h"
#include "line_writer.h"
#include "socket_sender.h"

int main(int argc, char* argv[])
{
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <logs_directory>" << std::endl;
        return 1;
    }

    // Open timestamped output file
    std::ofstream output_file;
    std::string output_path = open_output_file(output_file);
    if (!output_file.is_open()) {
        return 1;
    }
    std::cout << "Writing shotcalls to: " << output_path << std::endl;

    // Wire engine callback to write shotcalls to file and send over socket
    auto file_writer   = make_shotcall_writer(output_file);
    auto socket_sender = make_socket_sender();

    ShotCallEngine engine { };
    engine.set_shotcall_callback(
        [file_writer, socket_sender](const std::string& enemy_id, const std::string& callout) {
            file_writer(enemy_id, callout);
            socket_sender(enemy_id, callout);
        });

    // Run process_shotcalls on a background thread
    std::jthread shotcall_thread { [&engine](std::stop_token stop_token) {
        engine.process_shotcalls(stop_token);
    } };

    // Find and monitor the log file
    std::string combat_log_file = get_latest_combat_log(argv[1]);
    if (combat_log_file.empty()) {
        std::cerr << "Failed to find a combat log file. Exiting." << std::endl;
        return 1;
    }

    monitor_file(combat_log_file, engine);

    shotcall_thread.request_stop();
    return 0;
}
