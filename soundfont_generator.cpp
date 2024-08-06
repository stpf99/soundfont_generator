#include <algorithm>
#include <memory>
#include <vector>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <filesystem>
#include <sndfile.h>
#include <sf2cute.hpp>

using namespace sf2cute;
namespace fs = std::filesystem;

std::vector<int16_t> LoadWavFile(const std::string& filename) {
    SF_INFO sf_info;
    SNDFILE* snd_file = sf_open(filename.c_str(), SFM_READ, &sf_info);
    if (!snd_file) {
        throw std::runtime_error("Failed to open WAV file: " + filename);
    }

    std::vector<int16_t> data(sf_info.frames * sf_info.channels);
    sf_read_short(snd_file, data.data(), data.size());
    sf_close(snd_file);

    return data;
}

std::string GetPresetName(const std::string& filename) {
    fs::path path(filename);
    return path.stem().string(); // Use the filename without extension
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <path_to_wav_directory>" << std::endl;
        return 1;
    }

    std::string dir_path = argv[1];
    std::vector<std::string> wav_files;

    for (const auto& entry : fs::directory_iterator(dir_path)) {
        if (entry.is_regular_file() && entry.path().extension() == ".wav") {
            wav_files.push_back(entry.path().string());
        }
    }
    std::sort(wav_files.begin(), wav_files.end());

    auto sf2 = std::make_unique<SoundFont>();
    sf2->set_sound_engine("EMU8000");
    sf2->set_bank_name("Chipsound");
    sf2->set_rom_name("ROM");

    try {
        size_t presets_per_bank = 128;
        for (size_t i = 0; i < wav_files.size(); ++i) {
            const auto& wav_file = wav_files[i];
            std::string preset_name = GetPresetName(wav_file);
            std::vector<int16_t> data = LoadWavFile(wav_file);

            auto sample = std::make_shared<SFSample>(
                preset_name, data, 0, uint32_t(data.size()), 44100, 60, 0);

            SFInstrumentZone inst_zone(sample);
            inst_zone.SetGenerator(SFGeneratorItem(SFGenerator::kSampleModes, uint16_t(SampleMode::kLoopContinuously)));
            inst_zone.SetGenerator(SFGeneratorItem(SFGenerator::kReverbEffectsSend, 618));

            auto instrument = std::make_shared<SFInstrument>(preset_name);
            instrument->AddZone(std::move(inst_zone));

            SFPresetZone preset_zone(instrument);

            uint16_t bank_num = 0;  // All presets in bank 0
            uint16_t preset_num = static_cast<uint16_t>(i % 128); // Preset number within the bank

            auto preset = std::make_shared<SFPreset>(preset_name, bank_num, preset_num);
            preset->AddZone(std::move(preset_zone));

            sf2->AddSample(sample);
            sf2->AddInstrument(instrument);
            sf2->AddPreset(preset);
        }

        std::ofstream ofs("output.sf2", std::ios::binary);
        sf2->Write(ofs);

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}
