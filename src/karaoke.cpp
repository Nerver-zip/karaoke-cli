#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <cstdio>
#include <regex>
#include <filesystem>
#include <unordered_map>
#include <sstream>
#include <cstdlib>
#include <atomic>
#include <sys/ioctl.h>
#include <unistd.h>

struct LyricLine {
    double time;
    std::string text;
};

// === Execução de comando com captura de stdout ===
std::string exec(const char* cmd) {
    std::string result;
    char buffer[128];
    std::string command = std::string(cmd) + " 2>/dev/null";
    FILE* pipe = popen(command.c_str(), "r");
    if (!pipe) return "";
    while (fgets(buffer, sizeof(buffer), pipe)) result += buffer;
    pclose(pipe);
    if (!result.empty() && result.back() == '\n') result.pop_back();
    return result;
}

// === Carrega config.ini com suporte a seções ===
std::unordered_map<std::string, std::string> loadConfig(const std::string& path) {
    std::unordered_map<std::string, std::string> cfg;
    std::ifstream file(path);
    if (!file) return cfg;

    std::string line, section;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        if (line.front() == '[' && line.back() == ']') {
            section = line.substr(1, line.size() - 2);
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string key = line.substr(0, eq);
        std::string val = line.substr(eq + 1);

        key.erase(0, key.find_first_not_of(" \t"));
        key.erase(key.find_last_not_of(" \t") + 1);
        val.erase(0, val.find_first_not_of(" \t"));
        val.erase(val.find_last_not_of(" \t") + 1);

        if (!section.empty())
            key = section + "." + key;

        cfg[key] = val;
    }
    return cfg;
}

// === Leitura de arquivo .lrc ===
std::vector<LyricLine> readLRC(const std::string& filename) {
    std::vector<LyricLine> lyrics;
    std::ifstream file(filename);
    if (!file) return lyrics;

    std::string line;
    std::regex pattern(R"(\[(\d+):(\d+)\.(\d+)\](.*))");
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

// === Busca LRC local ou baixa via Python ===
std::string findLRC(const std::string& title, const std::string& artist) {
    std::vector<std::string> candidates = {
        "../local/songs/" + title + " - " + artist + ".lrc",
        "../local/songs/" + artist + " - " + title + ".lrc",
        "../local/songs/" + title + ".lrc",
        "../local/songs/" + artist + ".lrc"
    };

    for (const auto& f : candidates)
        if (std::filesystem::exists(f)) return f;

    std::cout << "⚙️  Song not found locally. Fetching from LRCLib...\n";
    std::string cmd = "python3 ../src/fetchLyrics.py \"" + title + "\" \"" + artist + "\"";
    system(cmd.c_str());

    for (const auto& f : candidates)
        if (std::filesystem::exists(f)) return f;

    std::cout << "🚫 Download failed.\n";
    return "";
}

/**
 * Limpa apenas a área das letras e redesenha título/artista no topo.
 */
void clearLyricsArea(
    const std::string& title = "",
    const std::string& artist = "",
    const std::unordered_map<std::string, std::string>& cfg = {}
) {
    if (title.empty() || artist.empty() || cfg.empty()) return;

    // === Padding e cores ===
    int titlePadTop  = cfg.count("display.title_padding_top") ? std::stoi(cfg.at("display.title_padding_top")) : 1;
    int titlePadLeft = cfg.count("display.title_padding_left") ? std::stoi(cfg.at("display.title_padding_left")) : 2;
    int artistPadLeft = cfg.count("display.artist_padding_left") ? std::stoi(cfg.at("display.artist_padding_left")) : 2;

    std::string titleColor  = cfg.count("display.title_color")  ? cfg.at("display.title_color")  : "cyan";
    std::string artistColor = cfg.count("display.artist_color") ? cfg.at("display.artist_color") : "bright_black";

    auto color = [&](const std::string& name) -> std::string {
        auto rgb = [](int r, int g, int b) {
            return "\033[38;2;" + std::to_string(r) + ";" +
                   std::to_string(g) + ";" + std::to_string(b) + "m";
        };

        // ======== Paleta ANSI clássica ========
        if (name == "black") return "\033[30m";
        if (name == "red") return "\033[31m";
        if (name == "green") return "\033[32m";
        if (name == "yellow") return "\033[33m";
        if (name == "blue") return "\033[34m";
        if (name == "magenta") return "\033[35m";
        if (name == "cyan") return "\033[36m";
        if (name == "white") return "\033[37m";
        if (name == "bright_black") return "\033[90m";
        if (name == "bright_red") return "\033[91m";
        if (name == "bright_green") return "\033[92m";
        if (name == "bright_yellow") return "\033[93m";
        if (name == "bright_blue") return "\033[94m";
        if (name == "bright_magenta") return "\033[95m";
        if (name == "bright_cyan") return "\033[96m";
        if (name == "bright_white") return "\033[97m";

        // ======== Tokyo Night Palette ========
        if (name == "tokyo_yellow") return rgb(224, 175, 104);
        if (name == "tokyo_blue") return rgb(122, 162, 247);
        if (name == "tokyo_cyan") return rgb(125, 207, 255);
        if (name == "tokyo_magenta") return rgb(187, 154, 247);
        if (name == "tokyo_green") return rgb(158, 206, 106);
        if (name == "tokyo_red") return rgb(255, 117, 127);
        if (name == "tokyo_fg") return rgb(169, 177, 214);
        if (name == "tokyo_comment") return rgb(92, 99, 112);
        if (name == "tokyo_bg") return rgb(26, 27, 38);

        // ======== Hexadecimal (#RRGGBB) ========
        if (!name.empty() && name[0] == '#' && name.size() == 7) {
            int r = std::stoi(name.substr(1, 2), nullptr, 16);
            int g = std::stoi(name.substr(3, 2), nullptr, 16);
            int b = std::stoi(name.substr(5, 2), nullptr, 16);
            return rgb(r, g, b);
        }

        return "\033[0m";
    };

    // === Limpa área textual ===
    std::cout << "\033[1;1H\033[J";

    // === Redesenha título e artista ===
    int titleY = 1 + titlePadTop;
    std::ostringstream meta;
    meta << "\033[" << titleY << ";" << titlePadLeft << "H"
         << color(titleColor) << title << "\033[0m";
    meta << "\033[" << (titleY + 1) << ";" << artistPadLeft << "H"
         << color(artistColor) << artist << "\033[0m";
    std::cout << meta.str() << std::flush;
}



// === Impressão das letras com contexto e fade ===
void printLyrics(
    const std::vector<LyricLine>& lyrics,
    size_t currentLine,
    const std::string& title,
    const std::string& artist,
    const std::unordered_map<std::string, std::string>& cfg
) {
    clearLyricsArea(title, artist, cfg);

    [[maybe_unused]] int coverW = cfg.count("cover.cover_width") ? std::stoi(cfg.at("cover.cover_width")) : 40;
    [[maybe_unused]] int coverX = cfg.count("cover.cover_x") ? std::stoi(cfg.at("cover.cover_x")) : 110;
    int lyricsBaseY = cfg.count("display.lyrics_base_y") ? std::stoi(cfg.at("display.lyrics_base_y")) : 5;

    int baseY = lyricsBaseY;
    int baseX = 2;

    std::cout << "\033[" << baseY << ";" << baseX << "H";

    int context = cfg.count("display.context_lines") ? std::stoi(cfg.at("display.context_lines")) : 3;
    int padding = cfg.count("display.lyrics_padding") ? std::stoi(cfg.at("display.lyrics_padding")) : 4;
    bool fade_enabled = cfg.count("fade.fade_enabled") ? (cfg.at("fade.fade_enabled") == "true") : true;
    int fade_depth = cfg.count("fade.fade_depth") ? std::stoi(cfg.at("fade.fade_depth")) : 3;
    std::string fade_style = cfg.count("fade.fade_style") ? cfg.at("fade.fade_style") : "blend";

    auto color = [&](const std::string& name, int intensity = 0) -> std::string {
        std::string prefix;
        if (fade_enabled) {
            if (fade_style == "dim") {
                if (intensity >= 1) prefix = "\033[2m";
            } else if (fade_style == "gray") {
                if (intensity == 1) prefix = "\033[37m";
                else if (intensity >= 2) prefix = "\033[90m";
            } else if (fade_style == "blend") {
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

    std::string highlight_color = cfg.count("display.highlight_color") ? cfg.at("display.highlight_color") : "cyan";
    std::string past_color = cfg.count("display.past_color") ? cfg.at("display.past_color") : "bright_black";
    std::string next_color = cfg.count("display.next_color") ? cfg.at("display.next_color") : "cyan";

    size_t start = (currentLine >= static_cast<size_t>(context)) ? currentLine - static_cast<size_t>(context) : 0;
    size_t end = std::min(currentLine + static_cast<size_t>(context) + 1, lyrics.size());
    std::string pad(padding, ' ');

    for (size_t i = start; i < end; ++i) {
        int distance = std::abs(static_cast<int>(i) - static_cast<int>(currentLine));
        int intensity = std::min(distance, fade_depth);

        std::cout << "\033[" << (baseY + static_cast<int>(i - start)) << ";" << baseX << "H";

        if (i == currentLine)
            std::cout << pad << color(highlight_color, 0) << lyrics[i].text << "\033[0m";
        else if (i < currentLine)
            std::cout << pad << color(past_color, intensity) << lyrics[i].text << "\033[0m";
        else
            std::cout << pad << color(next_color, intensity) << lyrics[i].text << "\033[0m";
    }

    std::cout << std::flush;
}

std::string formatTime(double seconds) {
    if (seconds < 0) seconds = 0;
    int totalSec = static_cast<int>(seconds);
    int minutes = totalSec / 60;
    int sec = totalSec % 60;
    std::ostringstream oss;
    oss << minutes << ":" << std::setw(2) << std::setfill('0') << sec;
    return oss.str();
}

// === Desenha a barra de progresso musical ===
void drawProgressBar(
    double position,
    double duration,
    const std::unordered_map<std::string, std::string>& cfg
) {
    if (duration <= 0.0 || position < 0.0) return;

    // ======= Lê configurações =======
    bool enabled = cfg.count("progress.enabled") && cfg.at("progress.enabled") == "true";
    if (!enabled) 
        return;

    int baseWidth = cfg.count("progress.width") ? std::stoi(cfg.at("progress.width")) : 40;
    int maxWidth = cfg.count("progress.max_width") ? std::stoi(cfg.at("progress.max_width")) : 100;
    bool resizable = cfg.count("progress.resizable") && cfg.at("progress.resizable") == "true";
    int barY = cfg.count("progress.y") ? std::stoi(cfg.at("progress.y")) : 12;
    int barX = cfg.count("progress.x") ? std::stoi(cfg.at("progress.x")) : 4;
    std::string marker = cfg.count("progress.marker_char") ? cfg.at("progress.marker_char") : "o";

    std::string colorPlayed    = cfg.count("progress.color_played")    ? cfg.at("progress.color_played")    : "cyan";
    std::string colorRemaining = cfg.count("progress.color_remaining") ? cfg.at("progress.color_remaining") : "bright_black";
    std::string colorMarker    = cfg.count("progress.color_marker")    ? cfg.at("progress.color_marker")    : "yellow";
    std::string colorBrackets  = cfg.count("progress.color_brackets")  ? cfg.at("progress.color_brackets")  : "white";
    std::string colorTime      = cfg.count("progress.color_time")      ? cfg.at("progress.color_time")      : "white";

    // ======= Calcula largura dinâmica =======
    struct winsize w;
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &w);
    int termWidth = w.ws_col;

    int barWidth = baseWidth;
    if (resizable) {
        int usable = std::max(10, termWidth - 20);
        barWidth = std::min(usable, maxWidth);
    }

    // ======= Calcula progresso =======
    double progress = position / duration;
    if (progress < 0) progress = 0;
    if (progress > 1) progress = 1;

    int markerPos = static_cast<int>(progress * (barWidth - 1));

    auto color = [&](const std::string& name) -> std::string {
        auto rgb = [](int r, int g, int b) {
            return "\033[38;2;" + std::to_string(r) + ";" +
                   std::to_string(g) + ";" + std::to_string(b) + "m";
        };

        // ANSI padrão
        static const std::unordered_map<std::string, std::string> ansi = {
            {"black", "\033[30m"}, {"red", "\033[31m"}, {"green", "\033[32m"},
            {"yellow", "\033[33m"}, {"blue", "\033[34m"}, {"magenta", "\033[35m"},
            {"cyan", "\033[36m"}, {"white", "\033[37m"},
            {"bright_black", "\033[90m"}, {"bright_red", "\033[91m"},
            {"bright_green", "\033[92m"}, {"bright_yellow", "\033[93m"},
            {"bright_blue", "\033[94m"}, {"bright_magenta", "\033[95m"},
            {"bright_cyan", "\033[96m"}, {"bright_white", "\033[97m"}
        };

        auto it = ansi.find(name);
        if (it != ansi.end()) return it->second;

        // Tokyo Night, Dracula, Nord, etc
        static const std::unordered_map<std::string, std::tuple<int,int,int>> themes = {
            {"tokyo_blue", {122,162,247}}, {"tokyo_cyan", {125,207,255}}, {"tokyo_yellow", {224,175,104}},
            {"tokyo_magenta", {187,154,247}}, {"tokyo_green", {158,206,106}},
            {"dracula_pink", {255,121,198}}, {"dracula_yellow", {241,250,140}},
            {"dracula_cyan", {139,233,253}}, {"nord_blue", {94,129,172}},
            {"nord_cyan", {136,192,208}}, {"nord_yellow", {235,203,139}}
        };

        auto th = themes.find(name);
        if (th != themes.end()) {
            auto [r,g,b] = th->second;
            return rgb(r,g,b);
        }

        // Hexa (#RRGGBB)
        if (!name.empty() && name[0] == '#' && name.size() == 7) {
            int r = std::stoi(name.substr(1, 2), nullptr, 16);
            int g = std::stoi(name.substr(3, 2), nullptr, 16);
            int b = std::stoi(name.substr(5, 2), nullptr, 16);
            return rgb(r, g, b);
        }

        return "\033[0m";
    };

    // ======= Monta barra =======
    std::string leftTime  = formatTime(position);
    std::string rightTime = formatTime(duration);

    std::ostringstream out;
    out << "\033[" << barY << ";" << barX << "H"; 

    //tempo inicial
    out << color(colorTime) << leftTime << "\033[0m ";

    //abre colchete
    out << color(colorBrackets) << "[" << "\033[0m";

    //parte tocada
    out << color(colorPlayed);
    for (int i = 0; i < markerPos; ++i) out << "-";
    out << "\033[0m";

    //marcador
    out << color(colorMarker) << marker << "\033[0m";

    //parte restante
    out << color(colorRemaining);
    for (int i = markerPos + 1; i < barWidth; ++i) out << "-";
    out << "\033[0m";

    //fecha colchete + tempo final
    out << color(colorBrackets) << "]" << "\033[0m "
        << color(colorTime) << rightTime << "\033[0m";

    std::cout << out.str() << std::flush;
}

// === Limpeza controlada ===
void clearScreen() { std::cout << "\033[2J\033[H"; }

void restoreCursor() {
    std::cout << "\033[?25h" << std::flush;
}

void restoreFont() {
    system("kitty @ set-font-size 0 2>/dev/null");
}

void handleSignal(int) {
    restoreCursor();
    restoreFont();
    std::_Exit(0);
}

std::atomic<bool> resized(false);
void handleResize(int) {
    resized = true;
}
