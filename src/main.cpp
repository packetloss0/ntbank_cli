#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <functional>
#include <filesystem>
#include <algorithm>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ntbank/format.hpp"

namespace fs = std::filesystem;

// Helper utility to create a hash from filename
std::uint64_t hash_asset_name(const std::string& name)
{
    return std::hash<std::string>{}(name);
}

int main(int argc, char* argv[]) {
    if (argc < 2)
    {
        std::cout << "Usage: ntbank_cli [options] <file_or_dir> [file_or_dir2 ...]\n";
        std::cout << "Options:\n";
        std::cout << "  -n, --keep-names   Preserve original filenames and directories.\n";
        return 1;
    }

    bool preserve_names = false;
    std::vector<fs::path> input_paths;

    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];
        if (arg == "-n" || arg == "--keep-names")
        {
            preserve_names = true;
        } else
        {
            input_paths.emplace_back(arg);
        }
    }

    std::vector<std::string> input_files;
    for (const auto& paths : input_paths)
    {
        if (fs::is_directory(paths))
        {
            for (const auto& entry : fs::recursive_directory_iterator(paths))
            {
                if (entry.is_regular_file() && entry.path().extension() == ".wav")
                {
                    input_files.emplace_back(entry.path().string());
                }
            }
        } else if (fs::is_regular_file(paths))
        {
            input_files.emplace_back(paths.string());
        } else
        {
            std::cerr << "Warning! Skipped invalid path: " << paths << "\n";
        }
    }

    if (input_files.empty())
    {
        std::cerr << "Error: No valid WAVs found.\n";
        return 1;
    }

    std::sort(input_files.begin(), input_files.end());

    std::vector<char> string_table;
    std::vector<std::pair<std::uint32_t, std::uint16_t>> string_offsets;

    if (preserve_names)
    {
        for (const auto& path : input_files)
        {
            std::uint32_t offset = static_cast<std::uint32_t>(string_table.size());
            std::uint16_t length = static_cast<std::uint16_t>(path.length());

            string_table.insert(string_table.end(), path.begin(), path.end());
            string_table.emplace_back('\0');
            string_offsets.emplace_back(offset,length);
        }
    }

    std::int64_t current_data_offset = sizeof(ntbank::Header) +
    (sizeof(ntbank::TocEntry) * input_files.size()) +
        string_table.size();

    std::ofstream output_file("output.ntbank", std::ios::binary);
    if (!output_file.is_open())
    {
        std::cerr << "Failed to open output.ntbank for writing.\n";
        return 1;
    }

    ntbank::Header header;
    header.entry_count = static_cast<std::uint32_t>(input_files.size());
    header.string_table_size = static_cast<std::uint32_t>(string_table.size());
    output_file.write(reinterpret_cast<const char*>(&header), sizeof(ntbank::Header));

    std::vector<ntbank::TocEntry> toc_entries;
    std::vector<std::vector<std::uint8_t>> audio_buffers;

    for (size_t i = 0; i < input_files.size(); ++i)
    {
        const auto& filepath = input_files[i];
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
        entry.bits_per_sample = static_cast<std::uint8_t>(wav.bitsPerSample);

        if (preserve_names)
        {
            entry.filename_offset = string_offsets[i].first;
            entry.filename_length = string_offsets[i].second;
        } else
        {
            entry.filename_offset = 0;
            entry.filename_length = 0;
        }

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
            << (int)wav.channels << "ch, " << (int)wav.bitsPerSample << "-bit]\n";
    }

    // Write toc block
    for (const auto& entry : toc_entries)
    {
        output_file.write(reinterpret_cast<const char*>(&entry), sizeof(ntbank::TocEntry));
    }

    // Write string table block
    if (!string_table.empty())
    {
        output_file.write(string_table.data(), string_table.size());
    }

    // Write audio payloads
    for (size_t i = 0; i < audio_buffers.size(); ++i)
    {
        output_file.write(reinterpret_cast<const char*>(audio_buffers[i].data()), audio_buffers[i].size());
    }

    output_file.close();
    std::cout << "\n Sucessfully created output.ntbank file! String table: "
        << header.string_table_size << " bytes.\n";
    return 0;
}
