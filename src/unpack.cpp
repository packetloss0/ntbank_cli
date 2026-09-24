#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <map>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ntbank/format.hpp"

namespace fs = std::filesystem;

// Formats byte size into kb, mb or gb with 1 decimal place.
std::string format_file_size(std::uint64_t bytes)
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(1);

    double double_bytes = static_cast<double>(bytes);
    if (bytes >= 1024*1024*1024)
    {
        ss << (double_bytes / (1024.0 * 1024.0 * 1024.0)) << "gb";
    } else if (bytes >= 1024*1024)
    {
        ss << (double_bytes / (1024.0 * 1024.0)) << "mb";
    } else
    {
        ss << (double_bytes / 1024.0) << "kb";
    }
    return ss.str();
}

bool export_wav(const fs::path& filepath, const std::vector<std::uint8_t>& pcm_data,
    std::uint32_t sample_rate, std::uint8_t channels, std::uint8_t bits_per_sample)
{
    // Create a directory tree if its missing
    if (filepath.has_parent_path())
    {
        fs::create_directories(filepath.parent_path());
    }

    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM; // TODO: 32bit floats should use DR_WAVE_FORMAT_IEEE_FLOAT
    format.channels = channels;
    format.sampleRate = sample_rate;
    format.bitsPerSample = bits_per_sample;

    drwav wav;
    if (!drwav_init_file_write(&wav, filepath.c_str(), &format, nullptr))
    {
        return false;
    }

    std::uint64_t bytes_per_frame = channels * (bits_per_sample / 8);
    std::uint64_t total_frames = pcm_data.size() / bytes_per_frame;

    drwav_write_pcm_frames(&wav, total_frames, pcm_data.data());
    drwav_uninit(&wav);
    return true;
}

struct FileReport
{
    std::string filename;
    std::uint64_t bytes;
    std::uint32_t sample_rate;
    std::uint8_t channels;
    std::uint8_t bits;
};

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: ntbank_unpack <archive.ntbank> [options]\n";
        std::cout << "Options:\n";
        std::cout << "  -o, --output-dir <dir>   Specify directory to extract files into (Default: ./extracted)\n";
        std::cout << "  -n, --keep-names         Use perserved filenames/directories stored in bank if present.\n";
        return 1;
    }

    std::string bank_path;
    fs::path output_dir = "extracted";
    bool use_stored_names = false;

    for (int i = 1; i < argc; ++i)
    {
        std::string arg = argv[i];
        if ((arg == "-o" || arg == "-output-dir") && i + 1 < argc)
        {
            output_dir = argv[++i];
        } else if (arg == "-n" || arg == "-keep-names")
        {
            use_stored_names = true;
        } else if (bank_path.empty())
        {
            bank_path = arg;
        }
    }

    std::ifstream in_file(bank_path, std::ios::binary);
    if (!in_file.is_open())
    {
        std::cerr << "Failed to open " << bank_path << " for reading.\n";
        return 1;
    }

    ntbank::Header header;
    in_file.read(reinterpret_cast<char*>(&header), sizeof(ntbank::Header));

    std::string magic_str(header.magic.data(), header.magic.size());
    if (magic_str != "NTBNK")
    {
        std::cerr << "Error: Invalid file format! Magic string is '" << magic_str << "', expected 'NTBNK'\n ";
        return 1;
    }

    // Read table of contents
    std::vector<ntbank::TocEntry> toc_entries(header.entry_count);
    in_file.read(reinterpret_cast<char*>(toc_entries.data()),
        sizeof(ntbank::TocEntry) * header.entry_count);

    // Read string table block
    std::vector<char> string_table(header.string_table_size);
    if (header.string_table_size > 0)
    {
        in_file.read(string_table.data(), header.string_table_size);
    }

    std::uint64_t total_unpacked_bytes = 0;
    std::map<std::string, std::vector<FileReport>> tree_groups;

    std::cout << "Extracting files to: " << output_dir << "\n";
    for (size_t i = 0; i < toc_entries.size(); ++i)
    {
        const auto& entry = toc_entries[i];

        fs::path relative_path;
        if (use_stored_names && entry.filename_length > 0 && !string_table.empty())
        {
            relative_path = std::string(&string_table[entry.filename_offset], entry.filename_length);
        } else
        {
            // target_file_path = output_dir / ("extracted_" + std::to_string(i) + ".wav");
            relative_path = "extracted_" + std::to_string(i) + ".wav";
        }

        fs::path target_file_path = output_dir / relative_path;
        std::uint64_t total_size = entry.preload_size + entry.stream_size;
        total_unpacked_bytes += total_size;

        std::vector<std::uint8_t> full_pcm_buffer(total_size);

        // Read Preload PCM Data
        in_file.seekg(entry.preload_offset, std::ios::beg);
        in_file.read(reinterpret_cast<char*>(full_pcm_buffer.data()),entry.preload_size);

        // Read Stream PCM Data
        in_file.seekg(entry.stream_offset, std::ios::beg);
        in_file.read(reinterpret_cast<char*>(full_pcm_buffer.data()) + entry.preload_size, entry.stream_size);

        if (export_wav(target_file_path, full_pcm_buffer, entry.sample_rate, entry.channels, entry.bits_per_sample))
        {
            std::cout << "Extracting file: " << target_file_path.string() << "\n";
        } else
        {
            std::cerr << "Failed to extract: " << target_file_path.string() << "\n";
        }

        std::string parent_dir = relative_path.has_parent_path() ? relative_path.parent_path().string() : ".";
        std::string file_name = relative_path.filename().string();

        tree_groups[parent_dir].push_back({
        file_name,
        total_size,
        entry.sample_rate,
        entry.channels,
        entry.bits_per_sample});

        /*std::cout << "Asset [" << i << "]: ID 0x" << std::hex << entry.asset_id << std::dec << "\n";
        std::cout << "  |- Sample  Rate: " << entry.sample_rate << " Hz\n";
        std::cout << "  |- Channels: " << (int)entry.channels << " (" << (entry.channels == 1 ? "Mono" : "Stereo") << ")\n";
        std::cout << "  |- Bitrate: " << (int)entry.bits_per_sample << "bit\n";
        std::cout << "  |- Preload Offset / Size: " << entry.preload_offset << " / " << entry.preload_size << " bytes\n";
        std::cout << "  |_ Stream Offset / Size: " << entry.stream_offset << " / " << entry.stream_size << " bytes\n\n";
        */
    }

    std::cout << "===============================\n";
    std::cout << "NTBANK ARCHIVE INSPECTOR \n";
    std::cout << "=============================== \n";
    std::cout << "Format Version    : " << header.version << "\n";
    std::cout << "Asset Count       : " << header.entry_count << "\n";
    std::cout << "String Table Size : " << header.string_table_size << " bytes\n";
    std::cout << "Total Unpacked Size: " << format_file_size(total_unpacked_bytes) << "\n";
    std::cout << "===============================\n\n";

    for (const auto& [dir_path,files] : tree_groups)
    {
        if (dir_path != ".")
        {
            std::cout << dir_path << "\n";
        }

        for (size_t i = 0; i < files.size(); ++i)
        {
            const auto& file = files[i];
            bool is_last = (i==files.size() - 1);
            std::string prefix = (dir_path == ".") ? "" : (is_last ? "   |_  " : "   |-  ");

            std::string chan_str = (file.channels == 1 ? "Mono" :
                    (file.channels == 2 ? "Stereo" : std::to_string(file.channels) + "ch"));

            std::cout << prefix << file.filename
                << " (" << format_file_size(file.bytes) << ")"
                << " [" << file.sample_rate << "Hz, " << chan_str << ", " << (int)file.bits << "-bit]\n";
        }
        std::cout << "\n";
    }

    std::cout << "\n Unpacking complete!\n";
    return 0;
}