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
#include<ctype.h>

#define MAXCLIENTS 1000
#define MAXBUFSIZE 1000
#define FILE_TYPES 9

struct Conf {
	int port;
  	char document_root[MAXBUFSIZE];
    char default_web_page[MAXBUFSIZE];
  	char extensions[FILE_TYPES][MAXBUFSIZE];
  	char encodings [FILE_TYPES][MAXBUFSIZE];
  	int keep_alive;
};

//Via Ctrl+C
void signal_handler(int sig_num) {
    signal(SIGINT, signal_handler);
    printf("\nExiting httpserver. Bye. \n");
    fflush(stdout);
    exit(0);
}

/******************
	  This function populates the conf struct with
	  the information about our server from the file ws.conf
******************/
void setup_conf(struct Conf* conf, int port) {
	FILE *f = fopen("ws.conf", "r");
    if(f == NULL) {
        perror("Opening ws.conf\n");
        exit(1);
    }
    
    char buffer[MAXBUFSIZE];
    char tbuf[MAXBUFSIZE];
    char *temp;
    
    while(fgets(buffer, MAXBUFSIZE, f)) {
    	char *str1, *str2, *str3, buf[MAXBUFSIZE];
		strcpy(buf,buffer);
        str1 = strtok(buffer," ");
        if(strcmp(str1,"Listen") == 0) {
        	conf->port = port;
        }
        else if(strcmp(str1,"DocumentRoot") == 0) {
        	strtok(buf," ");
        	str2 = strtok(NULL," ");
        	strcpy(conf->document_root, str2);
        }
        else if(strcmp(str1,"DirectoryIndex") == 0) {
        	strtok(buf," ");
        	str2 = strtok(NULL," ");
        	strcpy(conf->default_web_page, str2);
        }
		else if(strcmp(str1,"Keep-Alive") == 0) {
			strtok(buf," ");
			conf->keep_alive = atoi(strtok(NULL," "));
		}        
    }   
    fclose(f);   
}

void client_handler(int client, struct Conf *ws_conf) {

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
	    if (pid > 0) {
	      close(client_fd);
	      waitpid(0, NULL, WNOHANG);
	    }
	    //The child process will handle individual clients, so we can  close the main socket
	    if (pid == 0) {
	      close(sock);
	      // start child process
	      client_handler(client_fd, &ws_conf);
	      exit(0);
	    }

    }
}