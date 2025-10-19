# === Configurações ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2
SRC_DIR = src
BIN_DIR = bin
TARGET = $(BIN_DIR)/karaoke

SRC = $(SRC_DIR)/main.cpp $(SRC_DIR)/karaoke.cpp
HEADERS = $(SRC_DIR)/karaoke.h

# === Regras ===
all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)
	@echo "✅ Compilado com sucesso: $(TARGET)"

fetch:
	@python3 $(SRC_DIR)/fetchLyrics.py "In the End" "Linkin Park"

clean:
	rm -f $(TARGET)
	@echo "🧹 Limpeza concluída."

# 🔥 Limpa arquivos locais (letras baixadas, dumps, etc.)
clean_local:
	@echo "🧺 Limpando arquivos locais..."
	@rm -rf local/songs/* local/dump/* 2>/dev/null || true
	@echo "🧹 Arquivos locais removidos."

debug:
	$(CXX) $(CXXFLAGS) -g -DDEBUG -o $(TARGET) $(SRC)
	@echo "🐞 Compilado em modo debug."

.PHONY: all run clean debug fetch clean_local
