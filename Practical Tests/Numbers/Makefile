all: server tcp_client udp_sender

# Compile server.cpp
server:
	g++ -std=c++11 -Wall -Wextra server.cpp -o server

# Compile tcp_client.cpp
tcp_client:
	g++ -std=c++11 -Wall -Wextra tcp_client.cpp -o tcp_client

# Compile udp_sender.cpp
udp_sender:
	g++ -std=c++11 -Wall -Wextra udp_sender.cpp -o udp_sender

.PHONY: clean

clean:
	rm -f server tcp_client udp_sender

