//
// Save failures must preserve both existing documents and unrelated files.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>
#include <unistd.h>

#include "check.h"
#include "util/atomic_file.h"

namespace fs = std::filesystem;

std::string read(const fs::path &path) {
    std::ifstream in(path);
    return {std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>()};
}

int main() {
    std::string pattern = (fs::temp_directory_path() / "isaac-save-XXXXXX").string();
    CHECK(mkdtemp(pattern.data()) != nullptr);
    const fs::path directory(pattern);
    const auto path = directory / "book.gnumeric";
    std::string error;
    CHECK(util::atomic_write(path, "old", error));
    CHECK(error.empty());
    CHECK((fs::status(path).permissions() & fs::perms::all) ==
          (fs::perms::owner_read | fs::perms::owner_write));
    const auto sentinel = directory / "unrelated";
    CHECK(util::atomic_write(sentinel, "untouched", error));
    fs::create_symlink(sentinel, path.string() + ".tmp");
    CHECK(util::atomic_write(path, "new", error));
    CHECK(read(path) == "new");
    CHECK(read(sentinel) == "untouched");
    CHECK(fs::is_symlink(path.string() + ".tmp"));
    fs::permissions(path, fs::perms::owner_read | fs::perms::owner_write |
                          fs::perms::group_read);
    CHECK(util::atomic_write(path, "third", error));
    CHECK((fs::status(path).permissions() & fs::perms::group_read) != fs::perms::none);
    CHECK(!util::atomic_write(directory / "missing" / "book", "x", error));
    CHECK(!error.empty());
    CHECK(!fs::exists(directory / "missing"));
    // Executed as the invoking non-root user by the existing Docker workflow.
    CHECK(geteuid() != 0);
    if (geteuid() != 0) {
        fs::permissions(directory, fs::perms::owner_read | fs::perms::owner_exec);
        CHECK(!util::atomic_write(path, "lost", error));
        CHECK(!error.empty());
        CHECK(read(path) == "third");
        fs::permissions(directory, fs::perms::owner_all);
    }
    const auto target_dir = directory / "directory";
    fs::create_directory(target_dir);
    CHECK(!util::atomic_write(target_dir, "x", error));
    CHECK(fs::is_directory(target_dir));
    int entries = 0;
    for (const auto &entry : fs::directory_iterator(directory)) {
        ++entries;
        CHECK(entry.path().filename().string().find(".tmp-") == std::string::npos);
    }
    CHECK(entries == 4);
    fs::remove_all(directory);
    return testing::summary("atomic_file");
}
