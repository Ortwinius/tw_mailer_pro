# Compiler und Flags
CC = g++
CFLAGS = -std=c++17 -Wall -Werror -g

# Verzeichnisse für Quell- und Header-Dateien
SERVER_DIR = Server
CLIENT_DIR = Client
UTILS_DIR = utils

# Alle Quell- und Header-Dateien finden
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.cpp)
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.cpp) Client/commandBuilder/commandBuilder.cpp
UTILS_SRCS = utils/helpers.cpp

# Ziel-Executables
TARGETS = twmailer-server twmailer-client

# Hauptregel: Alle Ziele bauen
all: $(TARGETS)

# Regeln zum Bauen der Ziele
twmailer-server: $(SERVER_SRCS) $(UTILS_SRCS)
	$(CC) $(CFLAGS) $^ -o $@

twmailer-client: $(CLIENT_SRCS) $(UTILS_SRCS) 
	$(CC) $(CFLAGS) $^ -o $@

# Regel zum Aufräumen
clean:
	rm -f $(TARGETS)
