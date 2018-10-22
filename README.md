### Multiclient HTTP Server

This HTTP Server is written in C and supports multiple client connections. This is done using fork(), and while the children are processing the requests, parent process is available to accept other connections. It also supports persistent connections and pipelining of client requests.

To build the server run : make server To start the server run : "./server <port_number>"


It contains the following files: 

1. README.md
2. http_server.c  
3. ws.conf 
4. www/ (root directory which has all the files hosted by the server)
5. makefile

**Compile and Start the Server:**
1. Put the server folder in appropriate location, either on your local machine or remote server.
2. Type _make clean_ followed by _make server_. You will see an executable called server in that folder.
3. If you are on a remote linux machine, get the IP address of the server by entering _ifconfig | grep "inet addr"_. 
4. Start the server by entering _./server <port_number>
5. If you get the error "unable to bind socket", please use a different port or kill the process using that port.


**GET:**

	Usage: Enter localhost:<port_number>/index.html on your browser 

	Purpose: The web server tries to find a default web page such as “index.html” on the requested directory (root directory www).

**POST:**

	Usage: (echo -en "POST /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\nPOSTDATA") | nc 127.0.0.1 <port_number>

	Purpose: Hnadles POST requests for .html files. When the server receives a POST request, it handles the same way it handles GET, i.e returns the .html file with an added section header, <html><body><pre><h1>POSTDATA </h1></pre>... and so on.

**PIPELINING:**

	Usage: (echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\nConnection: Keep-alive\r\n\r\n"; sleep 10; echo -en "GET /index.html HTTP/1.1\r\nHost: localhost\r\n\r\n") | nc 127.0.0.1 <port_number>

	Purpose: If client wants to send multiple HTTP requests over the same connection, server reads the request from same connection till the time all of them are served and further waits for a given time period (configurable via Keep-Alive parameter in ws.conf) before closing the connection.  After the results of a single request are returned (e.g.,index.html), the server leaves the connection open for some period of time, allowing the client to reuse that connection socket to make subsequent requests. 10 seconds is this timeout value. To use this functionality the request headers should have "Connection: keep-alive" parameter.  A timer is run using 'select' interface, to check for any further requests. The server will keep that socket connection alive till the timeout value ends. The server should close the socket connection only after the timeout period is over if no other requests are received on that particular socket. If any subsequent requests arrive on that socket before the timeout period (essentially pipelined requests), the server resets the timeout period for that socket back to the timeout value and continues to wait to receive further requests. If there is no “Connection: Keep-alive” header in the request message, the server closes the socket upon responding and reply with “Connection: Close” header line. If there is a “Connection: Close” header in the request message, the server again closes the socket upon responding and reply with the same “Connection: Close” header line.
	

**Few things to keep in mind:**

- POST in only supported for .html files.
- Server returns 404 if file is not found, for all other errors it return 500.
