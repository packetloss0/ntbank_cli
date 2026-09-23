#pragma once

#include <cstdint>
#include <array>

namespace ntbank {

    #pragma pack(push, 1)

    struct Header
    {
        std::array<char,6> magic = {'N','T','B','A','N','K'};
        std::uint16_t version = 1;
        std::uint32_t entry_count = 0;
    };

    struct TocEntry
    {
        std::uint64_t asset_id; // Hash of asset path
        std::uint32_t sample_rate; // eg. 48000
        std::uint8_t channels; // 1 = Mono, 2 = Stereo

        std::uint64_t preload_offset; // Absolute offset to start of preloaded chunk
        std::uint32_t preload_size; // Byte size of preloaded PCM frames

        std::uint64_t stream_offset; // Absolute offset to start of stream paylaod
        std::uint64_t stream_size; // Byte size of stream chunk
    };

#pragma pack(pop)

}