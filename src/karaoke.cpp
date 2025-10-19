#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <regex>
#include <filesystem>
#include <unordered_map>

struct LyricLine {
    double time;
    std::string text;
};

// Executa comando e captura stdout, ignorando stderr
std::string exec(const char* cmd) {
    std::string result;
    char buffer[128];
    std::string command = std::string(cmd) + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe)) {
        result += buffer;
    }
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

std::unordered_map<std::string, std::string> loadConfig(const std::string& path) {
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream file(path);
    if (!file) return cfg;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#' || line[0] == '[') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;
        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);
        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);
        cfg[key] = val;
    }
    return cfg;
}

// Lê arquivo .lrc com suporte a milissegundos de 3 dígitos
std::vector<LyricLine> readLRC(const std::string& filename) {
    std::vector<LyricLine> lyrics;
    std::ifstream file(filename);
    if (!file) return lyrics;

    std::string line;
    std::regex pattern(R"(\[(\d+):(\d+)\.(\d+)\](.*))"); // [mm:ss.xxx]texto
    std::smatch match;

    while (std::getline(file, line)) {
        if (std::regex_match(line, match, pattern)) {
            int min = std::stoi(match[1]);
            int sec = std::stoi(match[2]);
            int msec = std::stoi(match[3]);
            std::string text = match[4];
            double timeSec = min * 60 + sec + msec / 1000.0;
            lyrics.push_back({timeSec, text});
        }
    }
    return lyrics;
}

// Procura arquivo LRC em várias combinações de título e artista
std::string findLRC(const std::string& title, const std::string& artist) {
    std::vector<std::string> candidates = {
        "../local/songs/" + title + " - " + artist + ".lrc",
        "../local/songs/" + artist + " - " + title + ".lrc",
        "../local/songs/" + title + ".lrc",
        "../local/songs/" + artist + ".lrc"
    };

    for (const auto& f : candidates) {
        if (std::filesystem::exists(f)) {
            return f;
        }
    }

    // Nenhum arquivo encontrado — tenta baixar via Python
    std::cout << "⚙️  Song not found in local files. Fetching from LRCLib...\n";

    std::string cmd = "python3 ../src/fetchLyrics.py \"" + title + "\" \"" + artist + "\"";
    int result = system(cmd.c_str());

    // Recheca após o download
    for (const auto& f : candidates) {
        if (std::filesystem::exists(f)) {
            return f;
        }
    }

    std::cout << "🚫 Download failed.\n";
    return "";
}

void clearScreen() {
    std::cout << "\033[2J\033[H";
}

// Mostra a linha atual e algumas linhas de contexto
void printLyrics(
    const std::vector<LyricLine>& lyrics,
    size_t currentLine,
    const std::unordered_map<std::string, std::string>& cfg
) {
    clearScreen();

    int context = cfg.count("context_lines") ? std::stoi(cfg.at("context_lines")) : 3;
    bool fade_enabled = cfg.count("fade_enabled") ? (cfg.at("fade_enabled") == "true") : true;
    int fade_depth = cfg.count("fade_depth") ? std::stoi(cfg.at("fade_depth")) : 3;
    std::string fade_style = cfg.count("fade_style") ? cfg.at("fade_style") : "blend";

    auto color = [&](const std::string& name, int intensity = 0) -> std::string {
        // intensidade: 0 = foco, 1 = perto, 2 = longe
        std::string prefix;
        if (fade_enabled) {
            if (fade_style == "dim") {
                if (intensity >= 1) prefix = "\033[2m";
            } 
            else if (fade_style == "gray") {
                if (intensity == 1) prefix = "\033[37m";
                else if (intensity >= 2) prefix = "\033[90m";
            } 
            else if (fade_style == "blend") {
                // Corrigido: agora mais próximo = gray (claro), distante = dim (mais fraco)
                if (intensity == 1) prefix = "\033[37m";
                else if (intensity >= 2) prefix = "\033[2m";
            }
        }

        if (name == "yellow") return prefix + "\033[1;33m";
        if (name == "green")  return prefix + "\033[1;32m";
        if (name == "red")    return prefix + "\033[1;31m";
        if (name == "blue")   return prefix + "\033[1;34m";
        if (name == "cyan")   return prefix + "\033[1;36m";
        if (name == "bright_black") return prefix + "\033[90m";
        if (name == "white")  return prefix + "\033[1;37m";
        return prefix + "\033[0m";
    };

    std::string highlight_color = cfg.count("highlight_color") ? cfg.at("highlight_color") : "cyan";
    std::string past_color = cfg.count("past_color") ? cfg.at("past_color") : "bright_black";
    std::string next_color = cfg.count("next_color") ? cfg.at("next_color") : "cyan";

    size_t start = (currentLine >= context) ? currentLine - context : 0;
    size_t end = std::min(currentLine + context + 1, lyrics.size());

    for (size_t i = start; i < end; ++i) {
        int distance = std::abs((int)i - (int)currentLine);
        int intensity = std::min(distance, fade_depth);

        if (i == currentLine)
            std::cout << color(highlight_color, 0) << lyrics[i].text << "\033[0m\n";
        else if (i < currentLine)
            std::cout << color(past_color, intensity) << lyrics[i].text << "\033[0m\n";
        else
            std::cout << color(next_color, intensity) << lyrics[i].text << "\033[0m\n";
    }
}

