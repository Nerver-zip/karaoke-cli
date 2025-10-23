# === Configurações ===
CXX = g++
CXXFLAGS = -std=c++17 -Wall -O2 -march=native -lssl -lcrypto
SRC_DIR = src
BIN_DIR = bin
TARGET = $(BIN_DIR)/karaoke

SRC = $(SRC_DIR)/main.cpp $(SRC_DIR)/karaoke.cpp $(SRC_DIR)/utils.cpp
HEADERS = $(SRC_DIR)/karaoke.h $(SRC_DIR)/utils.h

# === Regras ===
all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	@mkdir -p $(BIN_DIR)
	$(CXX) $(CXXFLAGS) -o $(TARGET) $(SRC)
	@echo "✅ Successfully compiled: $(TARGET)"

fetch:
	@python3 $(SRC_DIR)/fetchLyrics.py "In the End" "Linkin Park"

clean:
	rm -f $(TARGET)
	@echo "🧹 Cleaned binary files."

# 🔥 Limpa arquivos locais (letras baixadas, dumps, etc.)
clean_local:
	@echo "🧺 Cleaning local files..."
	@rm -rf local/songs/* local/icons/* 2>/dev/null || true
	@echo "🧹 Done."

debug:
	$(CXX) $(CXXFLAGS) -g -DDEBUG -o $(TARGET) $(SRC)
	@echo "🐞 Compiled on debug mode."

.PHONY: all run clean debug fetch clean_local
