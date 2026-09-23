#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <iomanip>

#define DR_WAV_IMPLEMENTATION
#include "dr_wav.h"

#include "ntbank/format.hpp"

bool export_wav(const std::string& filename, const std::vector<std::uint8_t>& pcm_data,
    std::uint32_t sample_rate, std::uint8_t channels, std::uint8_t bits_per_sample)
{
    drwav_data_format format;
    format.container = drwav_container_riff;
    format.format = DR_WAVE_FORMAT_PCM; // TODO: 32bit floats should use DR_WAVE_FORMAT_IEEE_FLOAT
    format.channels = channels;
    format.sampleRate = sample_rate;
    format.bitsPerSample = bits_per_sample;

    drwav wav;
    if (!drwav_init_file_write(&wav, filename.c_str(), &format, nullptr))
    {
        return false;
    }
    std::uint64_t total_frames = pcm_data.size() / (channels * (bits_per_sample / 8));
    drwav_write_pcm_frames(&wav, total_frames, pcm_data.data());
    drwav_uninit(&wav);
    return true;
}

int main (int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cout << "Usage: ntbank_unpack <archive.ntbank>\n";
        return 1;
    }

    std::string bank_path = argv[1];
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

    std::cout << "================ \n";
    std::cout << "NTBANK INSPECTOR \n";
    std::cout << "================ \n";
    std::cout << "Format Version: " << header.version << "\n";
    std::cout << "Asset Count: " << header.entry_count << "\n";
    std::cout << "================ \n";

    // Read table of contents
    std::vector<ntbank::TocEntry> toc_entries(header.entry_count);
    in_file.read(reinterpret_cast<char*>(toc_entries.data()),
        sizeof(ntbank::TocEntry) * header.entry_count);

    for (size_t i = 0; i < toc_entries.size(); ++i)
    {
        const auto& entry = toc_entries[i];
        std::cout << "Asset [" << i << "]: ID 0x" << std::hex << entry.asset_id << std::dec << "\n";
        std::cout << "  |- Sample Rate: " << entry.sample_rate << " Hz\n";
        std::cout << "  |- Channels: " << (int)entry.channels << " (" << (entry.channels == 1 ? "Mono" : "Stereo") << ")\n";
        std::cout << "  |- Preload Offset / Size: " << entry.preload_offset << " / " << entry.preload_size << " bytes\n";
        std::cout << "  |_ Stream Offset / Size: " << entry.stream_offset << " / " << entry.stream_size << " bytes\n\n";
    }

    std::cout << "Extracting files...\n";
    for (size_t i = 0; i < toc_entries.size(); ++i)
    {
        const auto& entry = toc_entries[i];
        std::uint64_t total_size = entry.preload_size + entry.stream_size;

        std::vector<std::uint8_t> full_pcm_buffer(total_size);

        in_file.seekg(entry.preload_offset, std::ios::beg);
        in_file.read(reinterpret_cast<char*>(full_pcm_buffer.data()),entry.preload_size);

        in_file.seekg(entry.stream_offset, std::ios::beg);
        in_file.read(reinterpret_cast<char*>(full_pcm_buffer.data()) + entry.preload_size, entry.stream_size);

        std::string output_filename = "extracted_" + std::to_string(i) + ".wav";
        if (export_wav(output_filename, full_pcm_buffer, entry.sample_rate, entry.channels, entry.bits_per_sample))
        {
            std::cout << "Sucessfully extracted WAV file: " << output_filename << "\n";
        } else
        {
            std::cout << "Failed to write WAV file: " << output_filename << "\n";
        }
    }

    std::cout << "\n Unpacking complete!\n";
    return 0;
}