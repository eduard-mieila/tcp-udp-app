#! /bin/bash

ID = testID

PORT = 12345

IP = 127.0.0.1

build: server subscriber

server:
	g++ -g -Wall -Wextra -std=c++17 server.cpp -o server

subscriber:
	g++ -g -Wall -Wextra -std=c++17 subscriber.cpp -o subscriber

run-server:
	./server $(PORT)

run-tcp-client:
	./subscriber $(ID) $(IP) $(PORT)

clean:
	rm -f server subscriber