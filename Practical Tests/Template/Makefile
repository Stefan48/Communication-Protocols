all: server client

# Compile server.cpp
server:
	g++ -std=c++11 -Wall -Wextra server.cpp -o server

# Compile subscriber.cpp
client:
	g++ -std=c++11 -Wall -Wextra client.cpp -o client

.PHONY: clean

clean:
	rm -f server client

