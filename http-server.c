#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>
#include <signal.h>
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/time.h>
#include <stdlib.h>
#include <memory.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>

#define MAXCLIENTS 1000
#define MAXBUFSIZE 1000
#define FILE_TYPES 9
#define MAXEXTLEN 5

#define OK 200
#define INVALID_METHOD 4001
#define INVALID_VERSION 4002
#define INVALID_URL 4003
#define NOT_FOUND 404
#define INT_SERVER_ERROR 500
#define NOT_IMPLEMENTED 501
#define KEEP_ALIVE_FLAG 1
struct timeval tv;
struct Conf {
	int port;
  	char document_root[MAXBUFSIZE];
    char default_web_page[MAXBUFSIZE];
  	char extensions[FILE_TYPES][MAXBUFSIZE];
  	char encodings [FILE_TYPES][MAXBUFSIZE];
  	int keep_alive;
};

struct HTTPHeader {
    char *method;
    char *URI;
    char *httpversion;
    char *connection;
    char *postdata;
};

struct HTTPResponse {
    char *path;
    char body[MAXBUFSIZE * MAXBUFSIZE];
    int status_code;
};

//Via Ctrl+C
void signal_handler(int sig_num) {
    signal(SIGINT, signal_handler);
    printf("\nExiting httpserver. Bye. \n");
    fflush(stdout);
    exit(0);
}

/* function to trim the trailing space */
char * trim(char *c) {
    char * e = c + strlen(c) - 1;
    while(*c && isspace(*c)) c++;
    while(e > c && isspace(*e)) *e-- = '\0';
    return e;
}

/***********************************************************
	  This function populates the conf struct with
	  the information about our server from the file ws.conf
************************************************************/
void setup_conf(struct Conf* conf, int port) {
	FILE *f = fopen("ws.conf", "r");
    if(f == NULL) {
        perror("Opening ws.conf\n");
        exit(1);
    }
    
    char buffer[MAXBUFSIZE];
    char tbuf[MAXBUFSIZE];
    char *temp;
    int index = 0;
    while(fgets(buffer, MAXBUFSIZE, f)) {
    	char *str1, *str2, *str3, buf[MAXBUFSIZE];
		strcpy(buf,buffer);
        str1 = strtok(buffer," ");
        if(strcmp(str1,"Listen") == 0) {
        	conf->port = port;
        } else if(strcmp(str1, "DocumentRoot") == 0) {
        	strtok(buf," ");
        	str2 = strtok(NULL," ");
        	strcpy(conf->document_root, str2);
        	strtok(conf->document_root, "\n");
        } else if(strcmp(str1, "DirectoryIndex") == 0) {
        	strtok(buf," ");
        	str2 = strtok(NULL," ");
        	strcpy(conf->default_web_page, str2);
        } else if(strcmp(str1, ".html") == 0 || strcmp(str1, ".htm") == 0 || strcmp(str1, ".txt") == 0 ||
        	strcmp(str1, ".png") == 0 || strcmp(str1, ".gif") == 0 || strcmp(str1, ".jpg") == 0 ||
        	strcmp(str1, ".css") == 0 || strcmp(str1, ".js") == 0 || strcmp(str1, ".ico") == 0 ) {
			strcpy(conf->extensions[index], str1);
			index++;
        } else if(strcmp(str1,"Keep-Alive") == 0) {
			strtok(buf," ");
			conf->keep_alive = atoi(strtok(NULL," "));
		}        
    }   
    fclose(f);  
    printf("Extension!!! %s\n", conf->extensions[0]);
}

void get_request_headers(char *req, struct HTTPHeader *header) {
	char postDataBuffer[4000] = {0};
	char pipelineBuffer[4000] = {0};
	bzero(pipelineBuffer, sizeof(pipelineBuffer));
	bzero(postDataBuffer, sizeof(postDataBuffer));
	char *requestLine;

	strncpy(postDataBuffer, req,strlen(req));
	strncpy(pipelineBuffer, req,strlen(req));
	requestLine = strtok (req, "\n");
	header->method = malloc(sizeof(char) * (strlen(requestLine) + 1));
	header->URI = malloc(sizeof(char) * (strlen(requestLine) + 1));
	header->httpversion = malloc(sizeof(char) * (strlen(requestLine) + 1));
	sscanf(requestLine, "%s %s %s", header->method,  header->URI,  header->httpversion);
	
	char *buff;
	int count;
	char received_string[MAXBUFSIZE];
	buff = strtok (postDataBuffer, "\n");
	while(buff != NULL) {	
		if((strlen(buff) == 1)) {
			count = 1;
		}
		buff = strtok (NULL, "\n");
		if(count == 1) {
			bzero(received_string,sizeof(received_string));
			sprintf(received_string,"%s",buff);
			count = 0;
		}		
	}
	//store postdata if any
	header->postdata = malloc(sizeof(char) * (strlen(received_string) + 1));
  	strcpy(header->postdata, received_string);


	char *temp;
	char pp[MAXBUFSIZE];
	temp = strtok (pipelineBuffer, "\n");
	while(temp != NULL) {	
		if(strstr(temp, "Connection") != NULL) {
			bzero(pp, sizeof(pp));
			sprintf(pp,"%s", temp+12);
		}
		temp = strtok (NULL, "\n");			
	}
	//store connection config e.g keep-alive
	header->connection = malloc(sizeof(char) * (strlen(pp) + 1));
  	strcpy(header->connection, pp);

}

void get_response_format(struct Conf *conf_struct, struct HTTPHeader *http_request, struct HTTPResponse *http_response) {
    
    if(strncmp(http_request->httpversion,"HTTP/1.1", 8) != 0 && strncmp(http_request->httpversion,"HTTP/1.0", 8) != 0) {
    	printf("%lu\n", strlen(http_request->httpversion));
    	printf("%lu\n", strlen("HTTP/1.1"));
    	printf("Invalid Version: HTTP/1.1\n");
        printf("Invalid Version: %s\n", http_request->httpversion);
        http_response->path = malloc(strlen("NO PATH")+1);
        strcpy(http_response->path, "NO PATH");
        http_response->status_code = 4002;
        return;
    } // to check bad http versions
    else if(strstr(http_request->URI, "{") != NULL || strstr(http_request->URI, "}") != NULL || strstr(http_request->URI, "|") != NULL ||
            strstr(http_request->URI, "\\") != NULL ||strstr(http_request->URI, "^") != NULL || strstr(http_request->URI, "~") != NULL ||
            strstr(http_request->URI, "[") != NULL || strstr(http_request->URI, "]") != NULL || strstr(http_request->URI, "`") != NULL ||
            strstr(http_request->URI, "%") != NULL || strstr(http_request->URI, "\"") != NULL || strstr(http_request->URI, "<") != NULL ||
            strstr(http_request->URI, ">") != NULL) {
        printf("Invalid URL: %s\n", http_request->URI);
    	http_response->path = malloc(strlen("NO PATH")+1);
        strcpy(http_response->path, "NO PATH");
        http_response->status_code = 4003;
        return;
    } 
    
    char *path = http_request->URI;
    int i = 0;
    
    // create full path
    
    char full_path[(sizeof(char) * (strlen(conf_struct->document_root) + strlen(http_request->URI)))];
    memset(full_path, 0, sizeof(full_path));
    
    strcat(full_path, conf_struct->document_root);
    strcat(full_path, http_request->URI);
    /*printf("File path requested !!!! %s\n", full_path);
    
    printf("uri is %lu\n", strlen(http_request->URI));
    printf("uri is %s\n", http_request->URI);*/

    if (strcmp(http_request->URI, "/") == 0) {
        strcat(full_path, "index.html");
        http_response->status_code = 200;
        http_response->path = malloc(strlen(full_path)+1);
        strcpy(http_response->path, full_path);
        return;
    }

    else if(strcmp(http_request->URI, "/index.html") == 0) {
        http_response->status_code = 200;
        http_response->path = malloc(strlen(full_path)+1);
        strcpy(http_response->path, full_path);
        return;
    }

    if(strstr(http_request->URI, ".") == NULL) {
        http_response->status_code = 404;
        http_response->path = malloc(strlen(full_path)+1);
        strcpy(http_response->path, full_path);
        return;
    }
    i = 0;
    char *ext = malloc(sizeof(char) * MAXEXTLEN);
    
    for(i = strlen(http_request->URI)-1; i >= 0; i--) {
        if(http_request->URI[i] == '.') {
            strcpy(ext, &http_request->URI[i]);
            break;
        }
    }
    int ext_flag = 1;
    
    //GET supports all extensions but POST only supports .html
    if ((strcmp(ext, ".html") == 0 && strcmp(http_request->method, "POST") == 0) || strcmp(http_request->method, "GET") == 0) {
    	    for(i = 0; i < FILE_TYPES; i++) {
        if(strcmp(ext, conf_struct->extensions[i]) == 0) {
            ext_flag = 0;
        }
    }
    
    if(ext_flag == 1) {
        http_response->status_code = 501;
        strcpy(http_response->path, full_path);
        return;
    }
    
    FILE *f = fopen(full_path, "rb");
    
    int rval = access(full_path, F_OK);
    if(f != NULL) {
    	http_response->status_code = OK;
        http_response->path = malloc(strlen(full_path)+1);
        strcpy(http_response->path, full_path);
        fclose(f);
        return;
    }
    else {
    	
    	if(errno == EACCES) { // check file access
        	http_response->status_code = 500;
            http_response->path = malloc(strlen(full_path)+1);
            strcpy(http_response->path, full_path);
            return;
        }
        http_response->status_code = 404;
        http_response->path = malloc(strlen(full_path)+1);
        strcpy(http_response->path, full_path);
        return;
    }	
    }
    
}

void get_file(int client, struct HTTPResponse *http_response, struct HTTPHeader *http_request) {
    char buffer[MAXBUFSIZE];
    long file_size;
    FILE *f;
    int nbytes;
    int i;
    
    char *ext = malloc(sizeof(char) * MAXEXTLEN);
    
    for(int i = strlen(http_response->path)-1; i >= 0; i--) {
        if(http_response->path[i] == '.') {
            strcpy(ext, &http_response->path[i]);
            break;
        }
    }

    f = fopen(http_response->path, "rb");
    fseek(f, 0, SEEK_END);
  	file_size = ftell(f);
  	fseek(f, 0, SEEK_SET);
    
    char ok_response[100];
    strcpy(ok_response, http_request->httpversion);
    strcat(ok_response, " 200 Document Follows\r\n");
    char content_length[1024];
    sprintf(content_length, "%ld", file_size);


    strcpy(http_response->body, ok_response);
    //POST will only respond to .html for now
    if (strcmp(http_request->method, "POST") == 0) {
    	strcat(http_response->body, "Content-Type: text/html\r\nContent-Length: ");
    	char post_data_tags[1024];
	    strcpy(post_data_tags, "<html><body><pre><h1>");
	    strcat(post_data_tags, http_request->postdata);
	    strcat(post_data_tags, "</h1></pre>\n");

	    strcat(http_response->body, content_length);
	    strcat(http_response->body, "\r\n\r\n");
	    send(client, http_response->body, strlen(http_response->body), 0);

	    send(client, post_data_tags, strlen(post_data_tags)-1, 0);
	    while (!feof(f)) {
	        nbytes = fread(buffer, 1, MAXBUFSIZE, f);
	        send(client, buffer, nbytes, 0);
	    }
	    fclose(f);

    } else {
    	if ((strcmp(ext, ".png")) == 0) {
        strcat(http_response->body, "Content-Type: image/png\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".gif")) == 0) {
	        strcat(http_response->body, "Content-Type: image/gif\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".html")) == 0 || strcmp(ext, ".htm") == 0) {
	        strcat(http_response->body, "Content-Type: text/html\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".jpg")) == 0) {
	        strcat(http_response->body, "Content-Type: image/jpeg\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".txt")) == 0) {
	        strcat(http_response->body, "Content-Type: text/plain\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".css")) == 0) {
	        strcat(http_response->body, "Content-Type: text/css\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".js")) == 0) {
	        strcat(http_response->body, "Content-Type: text/javascript\r\nContent-Length: ");
	    } else if ((strcmp(ext, ".ico")) == 0) {
	        strcat(http_response->body, "Content-Type: image/x-icon\r\nContent-Length: ");
	    }
	    strcat(http_response->body, content_length);
	    strcat(http_response->body, "\r\n\r\n");
	    send(client, http_response->body, strlen(http_response->body), 0);
	    while (!feof(f)) {
	        nbytes = fread(buffer, 1, MAXBUFSIZE, f);
	        send(client, buffer, nbytes, 0);
	    }
	    fclose(f);
    }
    
    
}

void send_error_response(int client, struct HTTPResponse *http_response, struct HTTPHeader *http_request) {
	char err_response[100];
	strcpy(err_response, http_request->httpversion);
	if (http_response->status_code == 404) {
		strcat(err_response, " 404 Not Found\r\n");
	} else {
		strcat(err_response, " 500 Internal Server Error\r\n");
	}
	strcpy(http_response->body, err_response);
	send(client, http_response->body, strlen(http_response->body), 0);
}

int client_handler(int client, struct Conf *ws_conf) {
    char request[MAXBUFSIZE];
	char header[MAXBUFSIZE];
	struct HTTPHeader request_headers;
	struct HTTPResponse http_response;
  	int status_code;
  	char res_data[MAXBUFSIZE], absolute_file_path[MAXBUFSIZE];
	// store the request body in request
    int nbytes = recv(client, request, MAXBUFSIZE, 0);
  	if (nbytes < 0) {
    	perror("Error while receiving request: ");
    	char internal_server_error[] = "HTTP/1.1 500 Internal Server Error";
        write (client, internal_server_error,strlen(internal_server_error));
		write(client,"\n\n", strlen("\n\n"));
    	exit(-1);
  	} else {
  		get_request_headers((char *)&request, &request_headers);
        printf("\n************************** REQUEST *********************************************\n\n");
		printf("Request Method: %s\n", request_headers.method);
		printf("Request URL: %s\n", request_headers.URI);
		printf("HTTP Version: %s\n", request_headers.httpversion);
		printf("Connection: %s\n", request_headers.connection);
		printf("Post Data: %s\n", request_headers.postdata);
		printf("\n********************************************************************************\n\n");

    	printf("\n***Calling getstatuscode***\n\n");
    	get_response_format(ws_conf, &request_headers, &http_response);
        printf("\n************************** RESPONSE *********************************************\n\n");
        printf("Status: %d\n", http_response.status_code);
        printf("Full Path: %s\n\n", http_response.path);
    	printf("\n********************************************************************************\n\n");

        char version[100];
        strcpy(version, request_headers.httpversion);

        if(http_response.status_code == OK) {
            get_file(client, &http_response, &request_headers);
        } else {
        	send_error_response(client, &http_response, &request_headers);
        }
  	}

  	if( KEEP_ALIVE_FLAG == 1) {	
		if(strstr(request_headers.connection, "Keep-alive") != NULL) {
			printf("Keep-alive valuse in secs is :%d\n", ws_conf->keep_alive);
			fd_set fd;
			FD_ZERO(&fd);
			FD_SET(client, &fd);
			int n = client+1;
			tv.tv_sec = ws_conf->keep_alive;
			tv.tv_usec = 0;

			int rv = 0; 
			printf("\n*****Timer Started*****\n");
			rv = select(n, &fd, NULL, NULL, &tv); 
			if(rv == -1) {
				perror("Select error\n");
				close(client);
				return(0);
			} else if(rv == 0) {
				printf("\n***** Timeout exceeded ****\n");        
				close(client);
				return(0);
			} else {   
				printf("\n***** Within timeout *****\n");
				if(FD_ISSET(client, &fd)) {
					printf("\n*****Calling again within timeout *****\n");
					client_handler(client, ws_conf);
				}		
			}
		}	
	}        
  	return 0;
}

int main( int argc, char* argv[]) {
	int sock;                           //This will be our socket
	struct sockaddr_in sin, remote;     //"Internet socket address structure"
	unsigned int remote_length = sizeof(remote);;         //length of the sockaddr_in structure
	int nbytes;                        //number of bytes we receive in our message
	char buffer[MAXBUFSIZE];             //a buffer to store our received message
	FILE *infp; 							// input file pointer
	FILE *outfp;							// output file pointer
	FILE *md5fp;							//md5 file pointer
	int client_fd; 						// client file descriptor for the accepted socket
	int pid;
  	
	char checksum[100], cmd[100];

	if (argc != 2)
	{
		printf ("USAGE:  <port>\n");
		exit(1);
	}
	struct Conf ws_conf;
    setup_conf(&ws_conf, atoi(argv[1])); 
	// print information of server
    printf("\n********************************************************************************\n\n");
    printf("Port Number: %d\n", ws_conf.port);
    printf("Document Root: %s\n", ws_conf.document_root);
    printf("Default Web Page: %s\n", ws_conf.default_web_page);
    printf("Keep-Alive Timeout: %d\n", ws_conf.keep_alive);
    printf("\n********************************************************************************\n\n");

	/******************
	  This code populates the sockaddr_in struct with
	  the information about our socket
	 ******************/
	bzero(&sin,sizeof(sin));                    //zero the struct
	sin.sin_family = AF_INET;                   //address family
	sin.sin_port = htons(atoi(argv[1]));        //htons() sets the port # to network byte order
	sin.sin_addr.s_addr = INADDR_ANY;           //supplies the IP address of the local machine

	int timer = ws_conf.keep_alive;

	//Causes the system to create a generic socket of type TCP (stream)
	if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0)
	{
		printf("unable to create socket");
	}


	/******************
	 Once we've created a socket, we must bind that socket to the 
	 local address and port we've supplied in the sockaddr_in struct
	 ******************/
	if (bind(sock, (struct sockaddr *)&sin, sizeof(sin)) < 0)
	{
		printf("unable to bind socket\n");
	}

	if ((listen(sock, MAXCLIENTS)) < 0) {
        perror("Listen");
        exit(1);
    }
  
    signal(SIGINT, signal_handler); // via Ctrl-C
    printf("\nListening and waiting for clients to accept\n");
    while (1) {
    	// accept client
        if((client_fd = accept(sock, (struct sockaddr *)&remote, &remote_length)) < 0) {
            perror("Accept error");
            exit(1);
        }
        pid = fork();
	    if (pid < 0) {
	      perror("ERROR on fork");
	      exit(1);
	    }
	    //Return to parent
	    /*if (pid > 0) {
	      close(client_fd);
	      waitpid(0, NULL, WNOHANG); //indicates that the parent process shouldn’t wait
	    }*/
	    //The child process will handle individual clients, so we can  close the main socket
	    if (pid == 0) {
	      close(sock);
	      // start child process
	      printf("\n***calling client handler!!!!! ***\n\n");
	      exit(client_handler(client_fd, &ws_conf));
	      /*while(ws_conf.keep_alive) {
		  	ws_conf.keep_alive --;
		  }*/
	      //exit(0);
	    }
	    /* Close the connection */
		if(close(client_fd)<0)
		{
			printf("Error closing socket\n");
		}

    }
}