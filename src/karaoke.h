#pragma once

#include <iostream>
#include <unordered_map>
#include <vector>

struct LyricLine {
    double time;
    std::string text;
};

// Executa comando e captura stdout, ignorando stderr
std::string exec(const char* cmd);

std::unordered_map<std::string, std::string> loadConfig(const std::string& path);

// Lê arquivo .lrc com suporte a milissegundos de 3 dígitos
std::vector<LyricLine> readLRC(const std::string& filename);
// Procura arquivo LRC em várias combinações de título e artista

std::string findLRC(const std::string& title, const std::string& artist); 

void clearScreen();

// Mostra a linha atual e algumas linhas de contexto
void printLyrics(const std::vector<LyricLine>& lyrics,
                 size_t currentLine,
                 const std::unordered_map<std::string, std::string>& cfg);
