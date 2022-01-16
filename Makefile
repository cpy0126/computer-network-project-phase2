all:
	g++ server.cpp -o server
	g++ client.cpp -o client
	g++ console.cpp -o console
clean:
	rm server
	rm client
	rm console
