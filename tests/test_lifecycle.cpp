#include <gtest/gtest.h>

#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>

#include "line_reader.h"

namespace fs = std::filesystem;

namespace {

// Creates a unique temporary directory that is removed at scope exit.
class TempDirectory {
public:
    TempDirectory()
    {
        path_ = fs::temp_directory_path() / ("shotcaller_test_" + std::to_string(counter_++));
        fs::create_directories(path_);
    }

    ~TempDirectory()
    {
        std::error_code error;
        fs::remove_all(path_, error);
    }

    const fs::path& path() const { return path_; }

private:
    static int counter_;
    fs::path path_;
};

int TempDirectory::counter_ = 0;

void write_file(const fs::path& path, const std::string& contents)
{
    std::ofstream file(path);
    file << contents;
}

} // namespace

TEST(Lifecycle, SelectLatestLog_ChoosesNewestWoWLog)
{
    const TempDirectory directory;
    const fs::path older = directory.path() / "WoWCombatLog.txt";
    const fs::path newer = directory.path() / "WoWCombatLog-2.txt";
    write_file(older, "older\n");
    write_file(newer, "newer\n");

    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(older, now - std::chrono::hours { 2 });
    fs::last_write_time(newer, now - std::chrono::hours { 1 });

    EXPECT_EQ(select_latest_log(directory.path().string()), newer.string());
    // An up-to-date current path is returned unchanged.
    EXPECT_EQ(select_latest_log(directory.path().string(), newer.string()), newer.string());
}

TEST(Lifecycle, SelectLatestLog_IgnoresOtherFiles)
{
    const TempDirectory directory;
    const fs::path unrelated = directory.path() / "notes.txt";
    const fs::path log       = directory.path() / "WoWCombatLog.txt";
    write_file(unrelated, "not a log\n");
    write_file(log, "log\n");

    const auto now = fs::file_time_type::clock::now();
    fs::last_write_time(unrelated, now + std::chrono::hours { 1 });

    EXPECT_EQ(select_latest_log(directory.path().string()), log.string());
}

TEST(Lifecycle, SelectLatestLog_NoLogsReturnsEmpty)
{
    const TempDirectory directory;
    EXPECT_EQ(select_latest_log(directory.path().string()), "");
    EXPECT_EQ(select_latest_log((directory.path() / "missing").string()), "");
}

TEST(Lifecycle, ReplayFile_ParsesEveryLine)
{
    const TempDirectory directory;
    const fs::path log = directory.path() / "sample.txt";
    {
        std::ofstream file(log);
        file << "6/14/2025 18:03:13.665-4  CHALLENGE_MODE_START,\"Cinderbrew Meadery\",2661,506,"
                "13,[9,10,147]\n";
        file << "\n";
        file << "6/14/2025 18:03:38.808-4  UNIT_DIED,0000000000000000,nil,0x80000000,0x80000000,"
                "Creature-0-4218-2661-9671-217126-0001CDF1A1,\"Over-Indulged Patron\",0xa28,0x0,0\n";
        file << "not a combat log line\n";
    }

    ShotCallEngine engine;
    EXPECT_EQ(replay_file(log.string(), engine), 2u);
    EXPECT_EQ(replay_file((directory.path() / "missing.txt").string(), engine), 0u);
}

TEST(Lifecycle, MonitorFile_ReturnsWhenStopFlagSet)
{
    const TempDirectory directory;
    const fs::path log = directory.path() / "WoWCombatLog.txt";
    write_file(log, "");

    ShotCallEngine engine;
    stop_requested = 1;
    monitor_file(directory.path().string(), log.string(), engine, std::stop_token { }, false);
    stop_requested = 0;
}

TEST(Lifecycle, MonitorFile_ReturnsWhenStopTokenRequested)
{
    const TempDirectory directory;
    const fs::path log = directory.path() / "WoWCombatLog.txt";
    write_file(log, "");

    ShotCallEngine engine;
    std::stop_source source;
    source.request_stop();
    monitor_file(directory.path().string(), log.string(), engine, source.get_token(), false);
}
