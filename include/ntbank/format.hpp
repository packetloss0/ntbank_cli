#pragma once

#include <cstdint>
#include <array>

namespace ntbank {

    #pragma pack(push, 1)

    struct Header
    {
        std::array<char,5> magic = {'N','T','B','N','K'};
        std::uint16_t version = 1;
        std::uint32_t entry_count = 0;
        std::uint32_t string_table_size = 0; // Size of string table in bytes (0 if stripped)
    };

    struct TocEntry
    {
        std::uint64_t asset_id; // Hash of asset path
        std::uint32_t sample_rate; // eg. 48000
        std::uint8_t channels; // 1 = Mono, 2 = Stereo
        std::uint8_t bits_per_sample; // eg. 16,24,32

        // String Table Mapping
        std::uint32_t filename_offset; // Offset relative to the start of string table
        std::uint16_t filename_length; // Length of the path string

        std::uint64_t preload_offset; // Absolute offset to start of preloaded chunk
        std::uint32_t preload_size; // Byte size of preloaded PCM frames

        std::uint64_t stream_offset; // Absolute offset to start of stream paylaod
        std::uint64_t stream_size; // Byte size of stream chunk
    };

#pragma pack(pop)

}