//
// Gzip, the compression around a .gnumeric file. Reading is a complete
// RFC 1951 inflater, so files from any writer load. Writing uses fixed
// Huffman blocks with a hash-chain match finder: smaller than stored
// data and needing no table in the stream. This replaces zlib and
// libgsf and keeps the toolkit-free libraries free of dependencies.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#ifndef UTIL_GZIP_H
#define UTIL_GZIP_H

#include <optional>
#include <string>
#include <string_view>

namespace util {

// Report whether data begins with the gzip magic number.
bool is_gzip(std::string_view data);

//
// Decompress a gzip file.
//
// Returns:
//      The uncompressed bytes, or nothing when the stream is damaged or
//      its checksum does not match.
//
std::optional<std::string> gunzip(std::string_view data);

// Compress bytes into a gzip file.
std::string gzip(std::string_view data);

} // namespace util

#endif
