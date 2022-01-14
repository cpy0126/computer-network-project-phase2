all:
	g++ server.cpp -o server
	g++ client.cpp -o client
	g++ console_client.cpp -o console_client
clean:
	rm server
	rm client
	rm console_client
