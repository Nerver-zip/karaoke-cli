#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdio>
#include "karaoke.h"

int main() {
    auto cfg = loadConfig("../config/config.ini");

    int context_lines = std::stoi(cfg["context_lines"]);
    int refresh_ms = std::stoi(cfg["refresh_ms"]);

    std::string highlight_color = cfg["highlight_color"];
    std::string past_color = cfg["past_color"];
    std::string next_color = cfg["next_color"];
    
    std::string title, artist, lrcFile;
    std::vector<LyricLine> lyrics;
    size_t currentLine = 0;

    double startPos = 0.0;
    auto startTime = std::chrono::steady_clock::now();

    double lastPosition = -1.0;
    auto lastUpdate = std::chrono::steady_clock::now();

    while (true) {
        std::string currentTitle = exec("playerctl --player=spotify metadata title");
        std::string currentArtist = exec("playerctl --player=spotify metadata artist");

        if (currentTitle.empty() || currentTitle.find("No player could handle") != std::string::npos) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // --- Detecta nova música ---
        if (currentTitle != title || currentArtist != artist) {
            title = currentTitle;
            artist = currentArtist;
            std::cout << "\n🎵 New song: " << title << " - " << artist << "\n";

            lrcFile = findLRC(title, artist);
            if (lrcFile.empty()) {
                lyrics.clear();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            lyrics = readLRC(lrcFile);
            if (lyrics.empty()) {
                std::cout << "❌ .lrc is empty or corrupted: " << lrcFile << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            // Pega posição inicial da música
            std::string posStr = exec("playerctl --player=spotify position");
            startPos = posStr.empty() ? 0.0 : std::stod(posStr);
            startTime = std::chrono::steady_clock::now();
            lastPosition = startPos;

            //Move currentLine para o ponto certo
            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < startPos) {
                currentLine++;
            }
            clearScreen();
            printLyrics(lyrics, currentLine, cfg);
        }

        if (lyrics.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // --- Posição atual ---
        std::string posStr = exec("playerctl --player=spotify position");
        if (posStr.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        double position = std::stod(posStr);
        auto now = std::chrono::steady_clock::now();
        double deltaTime = std::chrono::duration<double>(now - lastUpdate).count();

        // --- Detecta SEEK (avanço ou retrocesso) ---
        if (lastPosition >= 0.0 && std::abs(position - lastPosition) > 3.0) {
            // Grande salto detectado
            clearScreen();
            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < position) {
                currentLine++;
            }
            printLyrics(lyrics, currentLine, cfg);
        }

        // --- Atualiza quando tempo de reprodução diminui (usuário voltou) ---
        if (lastPosition >= 0.0 && position < lastPosition - 0.5) {
            clearScreen();
            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < position) {
                currentLine++;
            }
            printLyrics(lyrics, currentLine, cfg);
        }

        // --- Mostra letra ---
        if (currentLine < lyrics.size() && position >= lyrics[currentLine].time) {
            printLyrics(lyrics, currentLine, cfg);
            currentLine++;
        }

        lastPosition = position;
        lastUpdate = now;

        std::this_thread::sleep_for(std::chrono::milliseconds(
            cfg.count("refresh_ms") ? std::stoi(cfg.at("refresh_ms")) : 100
        ));
    }

    return 0;
}
