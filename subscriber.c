#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include "helpers.h"
#define BUFLENGTH 1583

void usage(char *file)
{
	fprintf(stderr, "Usage: %s id_client server_address server_port\n", file);
	exit(0);
}

int main(int argc, char *argv[])
{
	int sockfd, n, ret;
	struct sockaddr_in serv_addr;
	char buffer[1583];
	char clientID[10];
	store info;

	if (argc < 4) {
		usage(argv[0]);
	}

	strcpy(clientID, argv[1]);
	fd_set read_fds;	// read set used in select ()
	fd_set tmp_fds;		// temporarlt used set
	int fdmax;

	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(atoi(argv[3]));
	ret = inet_aton(argv[2], &serv_addr.sin_addr);
	DIE(ret == 0, "inet_aton");

	ret = connect(sockfd, (struct sockaddr*) &serv_addr, sizeof(serv_addr));
	DIE(ret < 0, "connect");

	n = send(sockfd, clientID, strlen(clientID), 0);
	DIE(n < 0, "send");

	FD_SET(0, &read_fds);
	FD_SET(sockfd, &read_fds);
	fdmax = sockfd;

	while (1) {
  		// it takes input from the keyboard
		tmp_fds = read_fds;

		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		if (FD_ISSET(0, &tmp_fds)){
			
			memset(buffer, 0, BUFLENGTH);
			fgets(buffer, BUFLENGTH - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0)
				break;

			// a message is sent to the server
			n = send(sockfd, buffer, strlen(buffer), 0);
			DIE(n < 0, "send");

		} else{

			memset(buffer, 0, BUFLENGTH);
			n = recv(sockfd, buffer, sizeof(buffer), 0);
			DIE(n < 0, "recv");

			if (n == 0 || strncmp(buffer, "exit", 4) == 0){
				printf("Server closed the connection!\n");
				break;
			}

			if (n > 1500){										// received a message (sent by the UDP client)
				memset(&info, 0, sizeof(info));
				memcpy(&info, buffer, sizeof(buffer));
				printf("%s:%s - %s - %s - %s\n", info.ip, info.port, info.topic, info.type, info.content);
			}
			else 
				printf("Server: %s\n", buffer);
		}
	}

	close(sockfd);

	return 0;
}
