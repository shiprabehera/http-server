all: server

server: http-server.c
	gcc http-server.c -o server

clean:
	rm server
