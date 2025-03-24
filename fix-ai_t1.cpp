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

// Funkcja do załadowania pliku WAV z rozszerzoną kontrolą błędów
std::vector<int16_t> WczytajPlikWAV(const std::string& nazwaPliku) {
    SF_INFO infoPliku;
    SNDFILE* plikDzwieku = sf_open(nazwaPliku.c_str(), SFM_READ, &infoPliku);
    if (!plikDzwieku) {
        throw std::runtime_error("Błąd otwarcia pliku WAV: " + nazwaPliku + " (" + std::strerror(errno) + ")");
    }

    // Walidacja formatu audio
    if (infoPliku.format != (SF_FORMAT_WAV | SF_FORMAT_PCM_16)) {
        sf_close(plikDzwieku);
        throw std::runtime_error("Nieobsługiwany format WAV: " + nazwaPliku);
    }

    std::vector<int16_t> dane(infoPliku.frames * infoPliku.channels);
    if (sf_read_short(plikDzwieku, dane.data(), dane.size()) != static_cast<sf_count_t>(dane.size())) {
        sf_close(plikDzwieku);
        throw std::runtime_error("Błąd odczytu próbek z: " + nazwaPliku);
    }
    sf_close(plikDzwieku);

    return dane;
}

// Generowanie nazwy presetu z czyszczeniem znaków niedozwolonych
std::string PobierzNazwePresetu(const std::string& nazwaPliku) {
    fs::path sciezka(nazwaPliku);
    std::string nazwa = sciezka.stem().string();
    
    // Zastępowanie niedozwolonych znaków podkreśleniami
    std::replace_if(nazwa.begin(), nazwa.end(), 
        [](char c) { return !std::isalnum(c) && c != '_' && c != '-'; }, '_');
    
    return nazwa.substr(0, 128); // Ograniczenie długości do 128 znaków
}

int main(int argc, char * argv[]) {
    if (argc < 2) {
        std::cerr << "Użycie: " << argv[0<< " <ścieżka_do_katalogu_WAV>
"
                  << "Obsługiwane formaty: 16-bit PCM WAV
"
                  << "Wyjście: output.sf2" << std::endl;
        return 1;
    }

    try {
        // Walidacja katalogu wejściowego
        if (!fs::exists(argv[1]) || !fs::is_directory(argv[1])) {
            throw std::invalid_argument("Nieprawidłowy katalog: " + std::string(argv[1]));
        }

        // Zbieranie plików WAV
        std::vector<std::string> plikiWAV;
        for (const auto& wpis : fs::directory_iterator(argv[1])) {
            if (wpis.is_regular_file() && 
                wpis.path().extension() == ".wav" &&
                wpis.file_size() > 44) { // Filtracja pustych plików
                plikiWAV.push_back(wpis.path().string());
            }
        }

        if (plikiWAV.empty()) {
            throw std::runtime_error("Brak prawidłowych plików WAV w katalogu");
        }

        // Tworzenie obiektu SoundFont
        auto sf2 = std::make_unique<SoundFont>();
        sf2->set_sound_engine("EMU8000");
        sf2->set_bank_name("Chipsound");
        sf2->set_rom_name("ROM");
        sf2->set_version(2, 0x0104); // Ustawienie wersji SF2

        const uint16_t numerBanku = 0;
        const uint16_t maksymalnePresety = 128;
        
        for (size_t i = 0; i < plikiWAV.size() && i < maksymalnePresety; ++i) {
            const auto& sciezkaWAV = plikiWAV[i];
            
            try {
                // Ładowanie i walidacja danych audio
                auto daneDzwieku = WczytajPlikWAV(sciezkaWAV);
                if (daneDzwieku.empty()) {
                    throw std::runtime_error("Pusty pladź dźwiękowy w: " + sciezkaWAV);
                }

                // Tworzenie obiektu próbki
                auto próbka = std::make_shared<SFSample>(
                    PobierzNazwePresetu(sciezkaWAV).c_str(),
                    daneDzwieku.data(),
                    0,
                    static_cast<uint32_t>(daneDzwieku.size()),
                    44100,
                    60,   // Podstawowa wysokość MIDI
                    -1    // Brak korekcji głośności
                );

                // Konfiguracja generatorów dźwiękowych
                SFInstrumentZone strefaInstrumentu(próbka);
                strefaInstrumentu.SetGenerator(
                    SFGeneratorItem(SFGenerator::kSampleModes, 
                        static_cast<uint16_t>(SampleMode::kLoopContinuously))
                );
                
                // Ustawienie pętli dźwiękowej
                strefaInstrumentu.SetGenerator(
                    SFGeneratorItem(SFGenerator::kStartLoop, 0)
                );
                strefaInstrumentu.SetGenerator(
                    SFGeneratorItem(SFGenerator::kEndLoop, static_cast<uint32_t>(daneDzwieku.size()))
                );

                // Tworzenie instrumentu i dodawanie stref
                auto instrument = std::make_shared<SFInstrument>(PobierzNazwePresetu(sciezkaWAV).c_str());
                instrument->AddZone(std::move(strefaInstrumentu));

                // Tworzenie presetu
                SFPreset preset(
                    PobierzNazwePresetu(sciezkaWAV).c_str(),
                    numerBanku,
                    static_cast<uint16_t>(i)
                );
                
                preset.AddZone(std::move(instrument));

                // Dodawanie do kontenera SF2
                sf2->AddSample(próbka);
                sf2->AddPreset(preset);

            } catch (const std::exception& e) {
                std::cerr << "Pomijanie " << sciezkaWAV << ": " << e.what() << std::endl;
                continue;
            }
        }

        // Zapis pliku SF2
        std::ofstream plikWyjsciowy("output.sf2", std::ios::binary);
        if (!plikWyjsciowy) {
            throw std::runtime_error("Błąd tworzenia pliku wyjściowego");
        }

        sf2->Write(plikWyjsciowy);
        std::cout << "Pomyślnie utworzono plik SF2 z " 
                  << sf2->presets().size() << " presetami" << std::endl;

    } catch (const std::exception& e) {
        std::cerr << "Krytyczny błąd: " << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
