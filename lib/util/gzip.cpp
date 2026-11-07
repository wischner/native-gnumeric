//
// RFC 1951 inflate and deflate, RFC 1952 framing. Written from the
// RFCs; every Huffman code is decoded by canonical first-code tables.
//
// GPL 2.0 or later (see: LICENSE)
// Copyright (C) 2026 tomaz stih
//

#include "util/gzip.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <vector>

namespace util {
namespace {

constexpr int max_bits = 15;
constexpr std::array<int, 29> length_base{
    3,  4,  5,  6,  7,  8,  9,  10, 11,  13,  15,  17,  19,  23, 27,
    31, 35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<int, 29> length_extra{0, 0, 0, 0, 0, 0, 0, 0, 1, 1,
                                           1, 1, 2, 2, 2, 2, 3, 3, 3, 3,
                                           4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<int, 30> distance_base{
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,
    33,  49,  65,  97,  129, 193,  257,  385,  513,  769,
    1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<int, 30> distance_extra{
    0, 0, 0, 0, 1, 1, 2, 2,  3,  3,  4,  4,  5,  5,  6,
    6, 7, 7, 8, 8, 9, 9, 10, 10, 11, 11, 12, 12, 13, 13};

// Return the CRC-32 of a byte range.
std::uint32_t crc32(std::string_view data) {
    static const auto table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t i = 0; i < 256; ++i) {
            std::uint32_t c = i;
            for (int bit = 0; bit < 8; ++bit)
                c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
            values[i] = c;
        }
        return values;
    }();
    std::uint32_t crc = 0xFFFFFFFFu;
    for (const char byte : data)
        crc = table[(crc ^ static_cast<std::uint8_t>(byte)) & 0xFF] ^
              (crc >> 8);
    return crc ^ 0xFFFFFFFFu;
}

// Reads bits least significant first.
class bit_reader {
public:
    explicit bit_reader(std::string_view data) : data_(data) {}

    // Return the next bits, or set failed past the end.
    unsigned bits(int count) {
        while (held_ < count) {
            if (next_ >= data_.size()) {
                failed_ = true;
                return 0;
            }
            buffer_ |= static_cast<std::uint32_t>(
                           static_cast<std::uint8_t>(data_[next_++]))
                       << held_;
            held_ += 8;
        }
        const unsigned result = buffer_ & ((1u << count) - 1u);
        buffer_ >>= count;
        held_ -= count;
        return result;
    }

    // Drop bits up to the next byte boundary.
    void align() {
        buffer_ = 0;
        held_ = 0;
    }

    bool failed() const { return failed_; }

private:
    std::string_view data_;
    std::size_t next_ = 0;
    std::uint32_t buffer_ = 0;
    int held_ = 0;
    bool failed_ = false;
};

// A canonical Huffman decoder.
class huffman {
public:
    huffman(const unsigned char *lengths, int count) {
        counts_.fill(0);
        for (int i = 0; i < count; ++i)
            ++counts_[lengths[i]];
        counts_[0] = 0;
        std::array<int, max_bits + 2> offsets{};
        for (int bit = 1; bit <= max_bits; ++bit)
            offsets[bit + 1] = offsets[bit] + counts_[bit];
        symbols_.resize(count);
        for (int i = 0; i < count; ++i)
            if (lengths[i])
                symbols_[offsets[lengths[i]]++] = i;
    }

    // Decode one symbol; negative when no code matches.
    int decode(bit_reader &in) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int length = 1; length <= max_bits; ++length) {
            code |= static_cast<int>(in.bits(1));
            if (in.failed())
                return -1;
            const int count = counts_[length];
            if (code - count < first)
                return symbols_[index + (code - first)];
            index += count;
            first = (first + count) << 1;
            code <<= 1;
        }
        return -1;
    }

private:
    std::array<int, max_bits + 1> counts_{};
    std::vector<int> symbols_;
};

// Copy the symbols of one compressed block to the output.
bool inflate_codes(bit_reader &in, const huffman &literals,
                   const huffman &distances, std::string &out) {
    while (true) {
        const int symbol = literals.decode(in);
        if (symbol < 0)
            return false;
        if (symbol < 256) {
            out.push_back(static_cast<char>(symbol));
            continue;
        }
        if (symbol == 256)
            return true;
        const int index = symbol - 257;
        if (index >= 29)
            return false;
        const int length = length_base[index] +
                           static_cast<int>(in.bits(length_extra[index]));
        const int code = distances.decode(in);
        if (code < 0 || code >= 30)
            return false;
        const std::size_t distance =
            static_cast<std::size_t>(distance_base[code]) +
            in.bits(distance_extra[code]);
        if (in.failed() || distance > out.size())
            return false;
        for (int i = 0; i < length; ++i)
            out.push_back(out[out.size() - distance]);
    }
}

// Return the fixed literal code lengths of RFC 1951.
std::array<unsigned char, 288> fixed_lengths() {
    std::array<unsigned char, 288> lengths{};
    std::fill(lengths.begin(), lengths.begin() + 144, 8);
    std::fill(lengths.begin() + 144, lengths.begin() + 256, 9);
    std::fill(lengths.begin() + 256, lengths.begin() + 280, 7);
    std::fill(lengths.begin() + 280, lengths.end(), 8);
    return lengths;
}

// Decode a dynamic-table block.
bool inflate_dynamic(bit_reader &in, std::string &out) {
    static constexpr std::array<int, 19> order{
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
    const int literal_count = static_cast<int>(in.bits(5)) + 257;
    const int distance_count = static_cast<int>(in.bits(5)) + 1;
    const int code_count = static_cast<int>(in.bits(4)) + 4;
    if (in.failed() || literal_count > 288 || distance_count > 30)
        return false;
    std::array<unsigned char, 320> lengths{};
    for (int i = 0; i < code_count; ++i)
        lengths[order[i]] = static_cast<unsigned char>(in.bits(3));
    const huffman codes(lengths.data(), 19);
    lengths.fill(0);
    int index = 0;
    while (index < literal_count + distance_count) {
        const int symbol = codes.decode(in);
        if (symbol < 0)
            return false;
        if (symbol < 16) {
            lengths[index++] = static_cast<unsigned char>(symbol);
            continue;
        }
        int repeat = 0;
        unsigned char previous = 0;
        if (symbol == 16) {
            if (index == 0)
                return false;
            previous = lengths[index - 1];
            repeat = 3 + static_cast<int>(in.bits(2));
        } else if (symbol == 17) {
            repeat = 3 + static_cast<int>(in.bits(3));
        } else {
            repeat = 11 + static_cast<int>(in.bits(7));
        }
        if (index + repeat > literal_count + distance_count)
            return false;
        while (repeat-- > 0)
            lengths[index++] = previous;
    }
    return inflate_codes(in, huffman(lengths.data(), literal_count),
                         huffman(lengths.data() + literal_count,
                                 distance_count),
                         out);
}

// Writes bits least significant first.
class bit_writer {
public:
    explicit bit_writer(std::string &out) : out_(out) {}

    // Append the low bits of a value.
    void bits(unsigned value, int count) {
        buffer_ |= static_cast<std::uint64_t>(value) << held_;
        held_ += count;
        while (held_ >= 8) {
            out_.push_back(static_cast<char>(buffer_ & 0xFF));
            buffer_ >>= 8;
            held_ -= 8;
        }
    }

    // Append a Huffman code, most significant bit first.
    void code(unsigned value, int count) {
        unsigned reversed = 0;
        for (int i = 0; i < count; ++i)
            if (value & (1u << i))
                reversed |= 1u << (count - 1 - i);
        bits(reversed, count);
    }

    // Pad to a byte boundary.
    void flush() {
        if (held_ > 0)
            bits(0, 8 - held_);
    }

private:
    std::string &out_;
    std::uint64_t buffer_ = 0;
    int held_ = 0;
};

// Write a literal or length symbol with the fixed code.
void write_symbol(bit_writer &out, int symbol) {
    if (symbol < 144)
        out.code(0x30 + symbol, 8);
    else if (symbol < 256)
        out.code(0x190 + symbol - 144, 9);
    else if (symbol < 280)
        out.code(symbol - 256, 7);
    else
        out.code(0xC0 + symbol - 280, 8);
}

// Write one back reference.
void write_match(bit_writer &out, int length, int distance) {
    int index = 28;
    while (length_base[index] > length)
        --index;
    write_symbol(out, 257 + index);
    out.bits(length - length_base[index], length_extra[index]);
    int code = 29;
    while (distance_base[code] > distance)
        --code;
    out.code(code, 5);
    out.bits(distance - distance_base[code], distance_extra[code]);
}

// Compress with one final fixed-Huffman block.
void deflate(std::string_view data, std::string &output) {
    constexpr int hash_bits = 15;
    constexpr int window = 32768;
    constexpr int max_chain = 32;
    constexpr int max_match = 258;
    std::vector<int> head(1 << hash_bits, -1);
    std::vector<int> previous(data.size(), -1);
    const auto hash = [&](std::size_t i) {
        const auto byte = [&](std::size_t at) {
            return static_cast<std::uint32_t>(
                static_cast<std::uint8_t>(data[at]));
        };
        return ((byte(i) << 10) ^ (byte(i + 1) << 5) ^ byte(i + 2)) &
               ((1u << hash_bits) - 1u);
    };
    const auto insert = [&](std::size_t i) {
        if (i + 2 < data.size()) {
            const auto slot = hash(i);
            previous[i] = head[slot];
            head[slot] = static_cast<int>(i);
        }
    };
    bit_writer out(output);
    out.bits(1, 1);
    out.bits(1, 2);
    std::size_t i = 0;
    while (i < data.size()) {
        int best = 0;
        int best_distance = 0;
        if (i + 2 < data.size()) {
            int candidate = head[hash(i)];
            for (int chain = 0;
                 candidate >= 0 && chain < max_chain &&
                 static_cast<int>(i) - candidate <= window - 1;
                 ++chain, candidate = previous[candidate]) {
                int length = 0;
                while (length < max_match &&
                       i + length < data.size() &&
                       data[candidate + length] == data[i + length])
                    ++length;
                if (length > best) {
                    best = length;
                    best_distance = static_cast<int>(i) - candidate;
                }
            }
        }
        if (best >= 3) {
            write_match(out, best, best_distance);
            for (int step = 0; step < best; ++step)
                insert(i++);
        } else {
            write_symbol(out, static_cast<std::uint8_t>(data[i]));
            insert(i++);
        }
    }
    write_symbol(out, 256);
    out.flush();
}

// Append a little-endian 32-bit field.
void put32(std::string &out, std::uint32_t value) {
    for (int shift = 0; shift < 32; shift += 8)
        out.push_back(static_cast<char>((value >> shift) & 0xFF));
}

} // namespace

bool is_gzip(std::string_view data) {
    return data.size() > 2 && data[0] == '\x1f' && data[1] == '\x8b';
}

std::optional<std::string> gunzip(std::string_view data) {
    if (data.size() < 18 || !is_gzip(data) || data[2] != 8)
        return std::nullopt;
    const auto flags = static_cast<std::uint8_t>(data[3]);
    std::size_t at = 10;
    if (flags & 4) {
        if (at + 2 > data.size())
            return std::nullopt;
        at += 2 + static_cast<std::uint8_t>(data[at]) +
              (static_cast<std::size_t>(
                   static_cast<std::uint8_t>(data[at + 1]))
               << 8);
    }
    for (const int bit : {8, 16}) {
        if (!(flags & bit))
            continue;
        while (at < data.size() && data[at] != 0)
            ++at;
        ++at;
    }
    if (flags & 2)
        at += 2;
    if (at + 8 > data.size())
        return std::nullopt;
    bit_reader in(data.substr(at, data.size() - at - 8));
    const auto literal_table = fixed_lengths();
    const std::array<unsigned char, 30> distance_table = [] {
        std::array<unsigned char, 30> lengths{};
        lengths.fill(5);
        return lengths;
    }();
    std::string out;
    bool last = false;
    while (!last) {
        last = in.bits(1) != 0;
        const unsigned type = in.bits(2);
        if (in.failed())
            return std::nullopt;
        if (type == 0) {
            in.align();
            const unsigned length = in.bits(16);
            const unsigned complement = in.bits(16);
            if (in.failed() || (length ^ complement) != 0xFFFF)
                return std::nullopt;
            for (unsigned i = 0; i < length; ++i)
                out.push_back(static_cast<char>(in.bits(8)));
        } else if (type == 1) {
            if (!inflate_codes(
                    in, huffman(literal_table.data(), 288),
                    huffman(distance_table.data(), 30), out))
                return std::nullopt;
        } else if (type == 2) {
            if (!inflate_dynamic(in, out))
                return std::nullopt;
        } else {
            return std::nullopt;
        }
        if (in.failed())
            return std::nullopt;
    }
    std::uint32_t stored = 0;
    for (int i = 0; i < 4; ++i)
        stored |= static_cast<std::uint32_t>(static_cast<std::uint8_t>(
                      data[data.size() - 8 + i]))
                  << (8 * i);
    if (stored != crc32(out))
        return std::nullopt;
    return out;
}

std::string gzip(std::string_view data) {
    std::string out("\x1f\x8b\x08\x00\x00\x00\x00\x00\x00\xff", 10);
    deflate(data, out);
    put32(out, crc32(data));
    put32(out, static_cast<std::uint32_t>(data.size()));
    return out;
}

} // namespace util
