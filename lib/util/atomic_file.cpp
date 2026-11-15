//
// Exclusive same-directory temporary files make document replacement atomic.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#include "util/atomic_file.h"

#include <algorithm>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <random>
#include <system_error>
#include <vector>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#include <fcntl.h>
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

namespace util {

bool atomic_write(const std::filesystem::path &path, std::string_view bytes,
                  std::string &error) {
    error.clear();
    std::filesystem::path temporary;
    int descriptor = -1;
    auto fail = [&](const char *operation, int code) {
        error = std::string(operation) + " " + path.string() + ": " +
                std::error_code(code, std::generic_category()).message();
        if (!temporary.empty()) {
            std::error_code ignored;
            std::filesystem::remove(temporary, ignored);
        }
        return false;
    };
#ifdef _WIN32
    std::random_device random;
    for (int attempt = 0; attempt < 32; ++attempt) {
        auto candidate = path;
        candidate += ".tmp-" + std::to_string(random());
        descriptor = _wopen(candidate.c_str(), _O_WRONLY | _O_CREAT | _O_EXCL |
                            _O_BINARY, _S_IREAD | _S_IWRITE);
        if (descriptor >= 0) { temporary = candidate; break; }
        if (errno != EEXIST) break;
    }
#else
    std::string pattern = path.string() + ".tmp-XXXXXX";
    std::vector<char> name(pattern.begin(), pattern.end());
    name.push_back('\0');
    descriptor = mkstemp(name.data());
    if (descriptor >= 0) temporary = name.data();
#endif
    if (descriptor < 0) return fail("Cannot create temporary for", errno);

    int failure = 0;
#ifndef _WIN32
    struct stat original{};
    if (stat(path.c_str(), &original) == 0) {
        if (fchmod(descriptor, original.st_mode & 0777) != 0) failure = errno;
    } else if (errno != ENOENT) {
        failure = errno;
    }
#endif
    std::size_t offset = 0;
    while (!failure && offset < bytes.size()) {
        const auto remaining = std::min<std::size_t>(bytes.size() - offset, 1 << 20);
#ifdef _WIN32
        const auto written = _write(descriptor, bytes.data() + offset,
                                    static_cast<unsigned>(remaining));
#else
        const auto written = write(descriptor, bytes.data() + offset, remaining);
#endif
        if (written < 0 && errno == EINTR) continue;
        if (written <= 0) { failure = written == 0 ? EIO : errno; break; }
        offset += static_cast<std::size_t>(written);
    }
#ifdef _WIN32
    if (!failure && _commit(descriptor) != 0) failure = errno;
    if (_close(descriptor) != 0 && !failure) failure = errno;
#else
    if (!failure && fsync(descriptor) != 0) failure = errno;
    if (close(descriptor) != 0 && !failure) failure = errno;
#endif
    if (failure) return fail("Cannot write", failure);
#ifdef _WIN32
    if (!MoveFileExW(temporary.c_str(), path.c_str(),
                     MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
        error = "Cannot replace " + path.string() + ": " +
                std::error_code(GetLastError(), std::system_category()).message();
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        return false;
    }
#else
    if (rename(temporary.c_str(), path.c_str()) != 0)
        return fail("Cannot replace", errno);
#endif
    return true;
}

} // namespace util
