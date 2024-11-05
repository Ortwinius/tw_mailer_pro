# Variables for compiler and flags
CC = g++
CFLAGS = -std=c++17 -Wall -Werror

# Rule to build both 'twmailer-server' and 'twmailer-client'
all: twmailer-server twmailer-client

# Rule to build the 'twmailer-server' executable from 'server_main.cpp' and 'server.cpp'
twmailer-server: Server/server_main.o Server/server.o
	$(CC) $(CFLAGS) -o twmailer-server Server/server_main.o Server/server.o

# Compile server_main.cpp into an object file
Server/server_main.o: Server/server_main.cpp Server/server.h
	$(CC) $(CFLAGS) -c Server/server_main.cpp -o Server/server_main.o

# Compile server.cpp into an object file
Server/server.o: Server/server.cpp Server/server.h
	$(CC) $(CFLAGS) -c Server/server.cpp -o Server/server.o

# Rule to build the 'twmailer-client' executable from 'client_main.cpp' and 'client.cpp'
twmailer-client: Client/client_main.o Client/client.o
	$(CC) $(CFLAGS) -o twmailer-client Client/client_main.o Client/client.o

# Compile client_main.cpp into an object file
Client/client_main.o: Client/client_main.cpp Client/client.h
	$(CC) $(CFLAGS) -c Client/client_main.cpp -o Client/client_main.o

# Compile client.cpp into an object file
Client/client.o: Client/client.cpp Client/client.h
	$(CC) $(CFLAGS) -c Client/client.cpp -o Client/client.o

# Clean up the 'twmailer-server' and 'twmailer-client' executables and object files
clean:
	rm -f twmailer-server twmailer-client Server/*.o Client/*.o
