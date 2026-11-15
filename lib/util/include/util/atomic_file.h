//
// Safe replacement of user-selected documents without shared temporary names.
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//
#ifndef UTIL_ATOMIC_FILE_H
#define UTIL_ATOMIC_FILE_H

#include <filesystem>
#include <string>
#include <string_view>

namespace util {

// Write a private, exclusive temporary beside the destination, then replace it.
// Existing permissions are retained. Failure leaves the old file intact and
// reports the operation and path; parent directories are never invented.
bool atomic_write(const std::filesystem::path &path, std::string_view bytes,
                  std::string &error);

} // namespace util
#endif
