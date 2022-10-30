all: server subscriber

# Compile server.cpp
server:
	g++ -std=c++11 -Wall -Wextra server.cpp -o server

# Compile subscriber.cpp
subscriber:
	g++ -std=c++11 -Wall -Wextra subscriber.cpp -o subscriber

.PHONY: clean

clean:
	rm -f server subscriber

