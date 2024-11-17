# Compiler und Flags
CC = g++
CFLAGS = -std=c++17 -Wall -Werror -g 
LDAPFLAGS = -lldap -llber 

# Verzeichnisse für Quell- und Header-Dateien
SERVER_DIR = Server
CLIENT_DIR = Client
UTILS_DIR = utils
MAILMANAGER_DIR = Server/MailManager
BLACKLIST_DIR = Server/Blacklist

# Alle Quell- und Header-Dateien finden
SERVER_SRCS = $(wildcard $(SERVER_DIR)/*.cpp)
MAILMANAGER_SRCS=$(wildcard $(MAILMANAGER_DIR)/*.cpp)
BLACKLIST_SRCS=$(wildcard $(BLACKLIST_DIR)/*.cpp)
CLIENT_SRCS = $(wildcard $(CLIENT_DIR)/*.cpp) Client/command_builder/command_builder.cpp
UTILS_SRCS = utils/helpers.cpp

# Ziel-Executables
TARGETS = twmailer-server twmailer-client

# Hauptregel: Alle Ziele bauen
all: $(TARGETS)

# Regeln zum Bauen der Ziele
twmailer-server: $(SERVER_SRCS) $(UTILS_SRCS) $(MAILMANAGER_SRCS) $(BLACKLIST_SRCS)
	$(CC) $(CFLAGS) $^ -o $@ $(LDAPFLAGS)

twmailer-client: $(CLIENT_SRCS) $(UTILS_SRCS) 
	$(CC) $(CFLAGS) $^ -o $@

# Regel zum Aufräumen
clean:
	rm -f $(TARGETS)
