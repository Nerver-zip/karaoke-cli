#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>
#include <cstdio>
#include <filesystem>
#include <sys/ioctl.h>
#include <unistd.h>
#include <csignal>
#include "karaoke.h"
#include "utils.h"

int main() {
    atexit(restoreCursor);
    atexit(restoreFont);
    std::signal(SIGINT, handleSignal);
    std::signal(SIGTERM, handleSignal);
    std::signal(SIGWINCH, handleResize);

    std::cout << "\033[?25l" << std::flush;

    auto cfg = loadConfig("../config/config.ini");

    // === BLOCO [display] ===
    int refresh_ms   = cfg.count("display.refresh_ms") ? std::stoi(cfg.at("display.refresh_ms")) : 100;
    int titlePadTop  = cfg.count("display.title_padding_top") ? std::stoi(cfg.at("display.title_padding_top")) : 1;

    // === BLOCO [cover] ===
    int coverW      = cfg.count("cover.cover_width") ? std::stoi(cfg.at("cover.cover_width")) : 40;
    int coverH      = cfg.count("cover.cover_height") ? std::stoi(cfg.at("cover.cover_height")) : 20;
    int coverX      = cfg.count("cover.cover_x") ? std::stoi(cfg.at("cover.cover_x")) : 110;
    int coverY      = cfg.count("cover.cover_y") ? std::stoi(cfg.at("cover.cover_y")) : 1;
    bool autoPos    = cfg.count("cover.auto_position") && cfg.at("cover.auto_position") == "true";
    int coverOffset = cfg.count("cover.cover_offset_x") ? std::stoi(cfg.at("cover.cover_offset_x")) : 5;
    bool stretch    = cfg.count("cover.stretch") && cfg.at("cover.stretch") == "true";

    // === BLOCO [font] ===
    std::string fontScale = cfg.count("display.font_scale") ? cfg.at("display.font_scale") : "";
    bool fontResetOnExit = cfg.count("display.font_reset_on_exit") && cfg.at("display.font_reset_on_exit") == "true";

    if (!fontScale.empty()) {
        std::string cmd = "kitty @ set-font-size " + fontScale + " 2>/dev/null";
        system(cmd.c_str());
    }

    if (fontResetOnExit) {
        std::signal(SIGINT, handleSignal);
        std::signal(SIGTERM, handleSignal);
        std::atexit(restoreFont);
    }

    std::string title, artist, lrcFile, coverFile;
    std::vector<LyricLine> lyrics;
    size_t currentLine = 0;

    double startPos = 0.0;
    double lastPosition = -1.0;
    auto startTime = std::chrono::steady_clock::now();
    auto lastUpdate = startTime;

    while (true) {
        std::string currentTitle = exec("playerctl --player=spotify metadata title");
        std::string currentArtist = exec("playerctl --player=spotify metadata artist");

        if (currentTitle.empty() || currentTitle.find("No player could handle") != std::string::npos) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // ===== NOVA MÚSICA DETECTADA =====
        if (currentTitle != title || currentArtist != artist) {
            title = currentTitle;
            artist = currentArtist;

            clearScreen();
            std::cout << "\033[H";

            // === TÍTULO E ARTISTA NO TOPO ===
            int titleY = coverY + titlePadTop;
            std::string metaTop =
                "\033[" + std::to_string(titleY) + ";2H" +
                "\033[1;36m" + title + "\033[0m\n" +
                "\033[" + std::to_string(titleY + 1) + ";2H" +
                "\033[2m" + artist + "\033[0m\n";
            std::cout << metaTop << std::flush;

            // === CAPA DO ÁLBUM ===
            std::string coverUrl = exec("playerctl --player=spotify metadata mpris:artUrl");
            if (!coverUrl.empty()) {
                std::filesystem::create_directories("../local/icons");

                if (coverUrl.rfind("file://", 0) == 0)
                    coverUrl = coverUrl.substr(7);

                std::string hash = hashString(title + artist);
                coverFile = "../local/icons/" + hash + "_cover.jpg";

                if (!std::filesystem::exists(coverFile) || std::filesystem::file_size(coverFile) == 0) {
                    std::string cmd;
                    if (system("command -v wget >/dev/null 2>&1") == 0)
                        cmd = "wget -q -O \"" + coverFile + "\" \"" + coverUrl + "\"";
                    else
                        cmd = "curl -s -L -o \"" + coverFile + "\" \"" + coverUrl + "\"";
                    system(cmd.c_str());
                }

                if (autoPos) {
                    struct winsize w;
                    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
                    int termWidth = w.ws_col;
                    coverX = termWidth - coverW - coverOffset;
                }

                if (std::filesystem::exists(coverFile)) {
                    std::string alignFlags = " --align right";
                    std::string aspectFlag = stretch ? " --no-preserve-aspect-ratio" : "";

                    std::string icatCmd =
                        "kitty +kitten icat --transfer-mode=memory" + alignFlags + aspectFlag +
                        " --place " + std::to_string(coverW) + "x" + std::to_string(coverH) + "@" +
                        std::to_string(coverX) + "x" + std::to_string(coverY) +
                        " \"" + coverFile + "\"";
                    system(icatCmd.c_str());
                }
            }

            // === LETRAS ===
            lrcFile = findLRC(title, artist);
            if (lrcFile.empty()) {
                lyrics.clear();
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            lyrics = readLRC(lrcFile);
            if (lyrics.empty()) {
                std::cout << "❌ LRC file is empty or malformed: " << lrcFile << "\n";
                std::this_thread::sleep_for(std::chrono::seconds(2));
                continue;
            }

            std::string posStr = exec("playerctl --player=spotify position");
            startPos = posStr.empty() ? 0.0 : std::stod(posStr);
            startTime = std::chrono::steady_clock::now();
            lastPosition = startPos;

            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < startPos)
                currentLine++;

            clearLyricsArea(title, artist, cfg);
            printLyrics(lyrics, currentLine, title, artist, cfg);
        }

        if (lyrics.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        // ===== MONITORA POSIÇÃO =====
        std::string posStr = exec("playerctl --player=spotify position");
        if (posStr.empty()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(500));
            continue;
        }

        double position = std::stod(posStr);
        auto now = std::chrono::steady_clock::now();

        // Detecta salto ou retrocesso
        if (lastPosition >= 0.0 && std::abs(position - lastPosition) > 3.0) {
            clearLyricsArea(title, artist, cfg);
            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < position)
                currentLine++;
            printLyrics(lyrics, currentLine, title, artist, cfg);
        }

        if (lastPosition >= 0.0 && position < lastPosition - 0.5) {
            clearLyricsArea(title, artist, cfg);
            currentLine = 0;
            while (currentLine < lyrics.size() && lyrics[currentLine].time < position)
                currentLine++;
            printLyrics(lyrics, currentLine, title, artist, cfg);
        }

        if (currentLine < lyrics.size() && position >= lyrics[currentLine].time) {
            printLyrics(lyrics, currentLine, title, artist, cfg);
            currentLine++;
        }
        
        // ===== ATUALIZAÇÃO DA BARRA DE PROGRESSO =====
        std::string durationStr = exec("playerctl --player=spotify metadata mpris:length");
        double duration = durationStr.empty() ? 0.0 : std::stod(durationStr) / 1e6;
        drawProgressBar(position, duration, cfg);

        // ===== TRATA REDIMENSIONAMENTO DE TERMINAL =====
        if (resized) {
            resized = false;

            struct winsize w;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
            int termWidth = w.ws_col;

            if (autoPos)
                coverX = termWidth - coverW - coverOffset;

            system("kitty +kitten icat --clear 2>/dev/null");

            if (!coverFile.empty() && std::filesystem::exists(coverFile)) {
                std::string alignFlags = " --align right";
                std::string aspectFlag = stretch ? " --no-preserve-aspect-ratio" : "";
                std::string icatCmd =
                    "kitty +kitten icat --transfer-mode=memory" + alignFlags + aspectFlag +
                    " --place " + std::to_string(coverW) + "x" + std::to_string(coverH) + "@" +
                    std::to_string(coverX) + "x" + std::to_string(coverY) +
                    " \"" + coverFile + "\"";
                system(icatCmd.c_str());
            }

            clearLyricsArea(title, artist, cfg);
            printLyrics(lyrics, currentLine, title, artist, cfg);

            bool resizable = cfg.count("progress.resizable") && cfg.at("progress.resizable") == "true";
            if (resizable) {
                std::string durationStr = exec("playerctl --player=spotify metadata mpris:length");
                double duration = durationStr.empty() ? 0.0 : std::stod(durationStr) / 1e6;
                drawProgressBar(position, duration, cfg);
            }
        }

        lastPosition = position;
        lastUpdate = now;
        std::this_thread::sleep_for(std::chrono::milliseconds(refresh_ms));
    }

    return 0;
}
