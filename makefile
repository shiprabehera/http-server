all: server

server: http_server.c
	gcc http_server.c -o server

clean:
	rm server
