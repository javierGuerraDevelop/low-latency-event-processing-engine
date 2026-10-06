#include "line_writer.h"

#include <ctime>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

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

std::function<void(const std::string&, const std::string&)> make_shotcall_writer(
    std::ofstream& file)
{
    return [&file](const std::string& /*enemy_id*/, const std::string& callout) {
        file << callout << "\n";
        file.flush();
    };
}
