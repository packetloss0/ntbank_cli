#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ntbank/format.hpp"

// Helper utility to create a hash from filename
std::uint64_t hash_asset_name(const std::string& name)
{
    return std::hash<std::string>{}(name);
}

int main(int argc, char* argv[]) {
    if (argc < 2)
    {
        std::cout << "Usage: ntbank_cli <file1.wav> [file2.wav ...]\n";
        return 1;
    }

    std::vector<std::string> input_files;
    for (int i = 1; i < argc; i++)
    {
        input_files.emplace_back(argv[i]);
    }

    std::ofstream output_file("output.ntbank", std::ios::binary);
    if (!output_file.is_open())
    {
        std::cerr << "Failed to open output.ntbank for writing.\n";
        return 1;
    }

    ntbank::Header header;
    header.entry_count = static_cast<std::uint32_t>(input_files.size());
    output_file.write(reinterpret_cast<const char*>(&header), sizeof(ntbank::Header));

    std::int64_t current_data_offset = sizeof(ntbank::Header) + (sizeof(ntbank::TocEntry) * input_files.size());

    std::vector<ntbank::TocEntry> toc_entries;
    std::vector<std::vector<std::uint8_t>> audio_buffers;

    for (const auto& filepath : input_files)
    {
        drwav wav;
        if (!drwav_init_file(&wav, filepath.c_str(), NULL))
        {
            std::cerr << "Failed to parse WAV file: " << filepath << "\n";
            return 1;
        }

        std::uint64_t total_pcm_bytes = wav.totalPCMFrameCount * wav.channels * (wav.bitsPerSample / 8);
        std::vector<std::uint8_t> buffer(total_pcm_bytes);

        drwav_read_pcm_frames(&wav, wav.totalPCMFrameCount, buffer.data());
        drwav_uninit(&wav);

        ntbank::TocEntry entry{};
        entry.asset_id = hash_asset_name(filepath);
        entry.sample_rate = wav.sampleRate;
        entry.channels = static_cast<std::uint8_t>(wav.channels);

        std::uint32_t bytes_per_frame = wav.channels * (wav.bitsPerSample / 8);
        std::uint32_t preload_frame_count = wav.sampleRate / 10; // 100ms
        std::uint32_t preload_bytes = std::min(static_cast<std::uint32_t>(total_pcm_bytes),
            preload_frame_count * bytes_per_frame);

        entry.preload_offset = current_data_offset;
        entry.preload_size = preload_bytes;
        entry.stream_offset = current_data_offset + preload_bytes;
        entry.stream_size = total_pcm_bytes - preload_bytes;

        current_data_offset += total_pcm_bytes;

        toc_entries.emplace_back(entry);
        audio_buffers.emplace_back(std::move(buffer));

        std::cout << "Packed: " << filepath << " [" << wav.sampleRate << "Hz, "
            << (int)wav.channels << "ch] -> Preload: " << entry.preload_size << " bytes\n";
    }

    for (const auto& entry : toc_entries)
    {
        output_file.write(reinterpret_cast<const char*>(&entry), sizeof(ntbank::TocEntry));
    }

    for (size_t i = 0; i < audio_buffers.size(); ++i)
    {
        output_file.write(reinterpret_cast<const char*>(audio_buffers[i].data()), audio_buffers[i].size());
    }

    output_file.close();
    std::cout << "\n Sucessfully created output.ntbank file!\n";
    return 0;
}
