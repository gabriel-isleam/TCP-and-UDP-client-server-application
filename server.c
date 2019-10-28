#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include "helpers.h"

void usage(char *file)
{
	fprintf(stderr, "Usage: %s server_port\n", file);
	exit(0);
}

int findClient(char* clientID, clientInfo* clients, int clientsNumber){

	int i;
	for (i = 0; i < clientsNumber; i++)
		if(strcmp(clients[i].clientID, clientID) == 0)
			return i;

	return -1;
}

char* findSubTopic(char* buffer){						// find subscribe topic

	char  *word, *topic, *character;
	word = strtok(buffer, " ");
	if (strcmp(word, "subscribe") != 0)
		return NULL;
	
	topic = strtok(NULL, " ");

	character = strtok(NULL, " ");
	if (character == NULL)
		return NULL;
	
	if (strlen(character) != 2)
		return NULL;
	
	return topic;
}

char *findUnsubTopic(char* buffer){						// find unsubscribe topic

	char  *word, *topic;
	word = strtok(buffer, " ");
	if (strcmp(word, "unsubscribe") != 0)
		return NULL;
	
	topic = strtok(NULL, " ");
	topic[strlen(topic) - 1] = 0;
		
	return topic;
}

int SorU(char *buffer){									// subscribe or unsubscribe

	char *word;
	word = strtok(buffer, " ");
	if (strcmp (word, "subscribe") == 0)
		return 1;										// return 1 for subscribe
	else if (strcmp(word, "unsubscribe") == 0)
		return 2;										// return 2 for unsubscribe
	else
		return 0;										// return 0 for a wrong query
}

int searchTopic(char *clientID, clientInfo* clients, int clientsNumber, char *topic){

	int i;

	for (i = 0; i < clientsNumber; i++)
		if(strcmp(clients[i].clientID, clientID) == 0)
			break;
		
	if (i == clientsNumber)
		return 0;

	int j;
	for (j = 0; j < clients[i].topicNumber; j++)
		if (strcmp(clients[i].topics[j], topic) == 0)
			return 1;										// found the searched topic in the client received as a parameter
		
	return 0;
}

void setSubscription(char *clientID, clientInfo *clients, int clientsNumber, char *topic, int SF){

	int i;
	for (i = 0; i < clientsNumber; i++)
		if(strcmp(clients[i].clientID, clientID) == 0)
			break;

	int topicNr = clients[i].topicNumber;
	strcpy(clients[i].topics[topicNr], topic);
	clients[i].SF[topicNr] = SF;
	clients[i].topicNumber++;
}

void removeTopic(char *clientID, clientInfo *clients, int clientsNumber, char* topic){

	int i;
	for (i = 0; i < clientsNumber; i++)
		if (strcmp(clients[i].clientID, clientID) == 0)
			break;

	int j;
	for (j = 0; j < clients[i].topicNumber; j++)
		if (strcmp(clients[i].topics[j], topic) == 0)
			break;

	int k;
	for (k = j; k < clients[i].topicNumber; k++)
		if ((k + 1) != clients[i].topicNumber){
			strcpy(clients[i].topics[k], clients[i].topics[k + 1]);
			clients[i].SF[k] = clients[i].SF[k + 1];
		}
	
	clients[i].topicNumber--;
}

double powC(double base, double exponent){

	double result = 1;
	while (exponent != 0){
		result *= base;
		exponent--;
	}
	return result;
}

int offline(clientInfo* clients, int clientsNumber, char* topic){

	int i, sum = 0, topics, j;
	for (i = 0; i < clientsNumber; i++)
		if (clients[i].online == 0){
			topics = clients[i].topicNumber;
			for (j = 0; j < topics; j++)
				if (strcmp(clients[i].topics[j], topic) == 0){
					if (clients[i].SF[j] == 1)
						sum++;
					break;
				}
		}
	return sum;
}

void removeAUX(storage *saved, int storedTopics, int pos){

	int i;
	for (i = pos; i < storedTopics; i++){
		if(i != (storedTopics - 1))
			saved[i] = saved[i + 1];
	}
}

int removeFromStorage(storage *saved, int storedTopics){

	int i;
	for (i = 0; i < storedTopics; i++){
		if (saved[i].offlineSubs == 0){
			removeAUX(saved, storedTopics, i);
			i--;
			storedTopics--;
		}
	}
	return storedTopics;
}

int main(int argc, char *argv[])
{
	int sockfd, newsockfd, portno;
	char buffer[BUFLEN], auxBuffer[BUFLEN];
	struct sockaddr_in serv_addr, cli_addr;
	int n, i, ret, j, storedTopics = 0, k;
	socklen_t clilen;

	char* exitMsg = "exit";
	char* usageCommand = "subscribe <topic> <SF=0/1> // unsubscribe <topic>";
	char* subscribed = "You are already subscribed to this topic";
	char* notSubscribed = "You are not subscribed to this topic";
	char message[65];
	char clientID[10];
	char toSend[1583];			// 1583 - dimension of a structure of type "store"
	
	intDatagram intd;
    shortDatagram shortd;
    floatDatagram floatd;
    stringDatagram stringd;
    store info;
    storage saved[100];			// saves 100 received messages for offline clients

	fd_set read_fds;			// read set used in select()
	fd_set tmp_fds;				// temporarly used set
	fd_set offline_fds;
	int fdmax;					// maximum vakue for fd from read_fds set

	if (argc < 2) {
		usage(argv[0]);
	}
	// the set of read descriptors (read_fds) and the temporary set (tmp_fds) are cleared
	FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);
	FD_ZERO(&offline_fds);

	sockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(sockfd < 0, "socket");

	portno = atoi(argv[1]);
	DIE(portno == 0, "atoi");

	memset((char *) &serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(portno);
	serv_addr.sin_addr.s_addr = INADDR_ANY;

	ret = bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(struct sockaddr));
	DIE(ret < 0, "bind");

	ret = listen(sockfd, MAX_CLIENTS);
	DIE(ret < 0, "listen");

	// add the new file descriptor (the socket to listen for connections) in the read_fds set
	int sockUDP = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(sockUDP < 0, "socket");

	struct sockaddr_in servaddr, cliaddr;
	char received[1552];

	memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr));

    ret = bind(sockUDP, (struct sockaddr *)&serv_addr, sizeof(serv_addr));
    DIE(ret < 0, "bind");

	FD_SET(0, &read_fds);
	FD_SET(sockUDP, &read_fds);
	FD_SET(sockfd, &read_fds);
	if (sockfd > sockUDP)
		fdmax = sockfd;
	else
		fdmax = sockUDP;

	unsigned int len = sizeof(cliaddr);
	char onlineClients[100][10];				// saves the names of 100 online clients
	char* topic;							
	clientInfo clients[100], client;
	int clientsNumber = 0, clientIndex, SF, request;

	while (1) {
		tmp_fds = read_fds; 
		
		ret = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(ret < 0, "select");

		for (i = 0; i <= fdmax; i++) {
			if (FD_ISSET(i, &tmp_fds)) {
				if (i == sockfd) {
					// appeared a connection request on the inactive socket (the one with the listen),
					// which the server accepts
					clilen = sizeof(cli_addr);
					newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);
					DIE(newsockfd < 0, "accept");

					// add the new return socket from accept() to the set of read descriptors
					FD_SET(newsockfd, &read_fds);
					if (newsockfd > fdmax) { 
						fdmax = newsockfd;
					}

					memset(clientID, 0, sizeof(clientID));
					memset(&client, 0, sizeof(client));
					n = recv(newsockfd, clientID, sizeof(clientID), 0);
					DIE(n < 0, "send");

					strcpy(onlineClients[newsockfd], clientID);
					
					clientIndex = findClient(clientID, clients, clientsNumber);
					if (clientIndex == -1){
					
						strcpy(client.clientID, clientID);
						client.online = 1;
						client.topicNumber = 0;
						clients[clientsNumber] = client;
						clientsNumber ++;
					} 
					else{
						
						clients[clientIndex].online = 1;
						for (k = 0; k < storedTopics; k++)
							if (searchTopic(clientID, clients, clientsNumber, saved[k].info.topic) == 1){
								memset(toSend, 0, sizeof(toSend));
								memcpy(toSend, &(saved[k].info), sizeof(saved[k].info));
								n = send(newsockfd, toSend, sizeof(toSend), 0);
								DIE(n < 0, "send");
								saved[k].offlineSubs--;
							}

						storedTopics = removeFromStorage(saved, storedTopics);
					}
					printf("New client %s connected from %s:%d.\n", clientID, inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));
							
				} else if (i != sockUDP){

					if (i != 0){
						memset(buffer, 0, BUFLEN);
						n = recv(i, buffer, sizeof(buffer), 0);
						DIE(n < 0, "recv");

						if (n == 0) {

							printf("Client %s disconnected.\n", onlineClients[i]);
							
							clientIndex = findClient(onlineClients[i], clients, clientsNumber);
							clients[clientIndex].online = 0;

							close(i);
							FD_CLR(i, &read_fds);
						} else {
							
							strcpy(auxBuffer, buffer);
							request = SorU(buffer);
							strcpy(buffer, auxBuffer);

							if (request == 0){
								n = send(i, usageCommand, strlen(usageCommand), 0);
								DIE(n < 0, "send");
							}
							else if (request == 1){										// request is a subscription
								topic = findSubTopic(buffer);
								
								if (topic == NULL){								
									n = send(i, usageCommand, strlen(usageCommand), 0);
									DIE(n < 0, "send");
								}
								else {
									SF = buffer[strlen(auxBuffer) - 2] - '0';
									if (SF != 0 && SF != 1){
										n = send(i, usageCommand, strlen(usageCommand), 0);
										DIE(n < 0, "send");
									}
									else{
										if (searchTopic(onlineClients[i], clients, clientsNumber, topic) == 1){
											n = send(i, subscribed, strlen(subscribed), 0);
											DIE(n < 0, "send");
										}
										else{ 
											
											setSubscription(onlineClients[i], clients, clientsNumber, topic, SF);
											printf("Client %s subscribed at %s\n", onlineClients[i], topic);
											memset(message, 0, sizeof(message));
											sprintf(message, "subscribed %s", topic); 
											n = send(i, message, strlen(message), 0);
											DIE(n < 0, "send");
										}
									}
								}
							}
							else if (request == 2){
								topic = findUnsubTopic(buffer);
								
								if (topic == NULL){
									n = send(i, usageCommand, strlen(usageCommand), 0);
									DIE(n < 0, "send");
								}
								else{
									if (searchTopic(onlineClients[i], clients, clientsNumber, topic) == 0){
											n = send(i, notSubscribed, strlen(notSubscribed), 0);
											DIE(n < 0, "send");
										}
									else{ 
										removeTopic(onlineClients[i], clients, clientsNumber, topic);
										printf("Client %s unsubscribe from %s\n", onlineClients[i], topic);
										memset(message, 0, sizeof(message));
										sprintf(message, "unsubscribed %s", topic); 
										n = send(i, message, strlen(message), 0);
										DIE(n < 0, "send");
									}
								}
							}
						}

					} else{
						fgets(buffer, BUFLEN - 1, stdin);
						if (strncmp(buffer, exitMsg, 4) == 0) {
							
							for (i = 1; i <= fdmax; i++) {
							 	if (FD_ISSET(i, &tmp_fds)){
									n = send(4, exitMsg, strlen(exitMsg), 0);
							 		DIE(n < 0, "send");
								}
							}
							close(sockfd);
							return 0;
						}
						printf("S-a citit asta: %s\n", buffer);
					}					
				}
				else if (i == sockUDP){

					len = sizeof(cliaddr);
					memset(&received, 0, sizeof(received));
					n = recvfrom(sockUDP, received, sizeof(received), 0, (struct sockaddr *) &cliaddr, &len);
					received[n] = '\0';
					memset(&info, 0, sizeof(info));

				    if (received[50] == 0){

				    	memset(&intd, 0, sizeof(intd));
				    	memcpy(&intd, received, sizeof(intd));
				    	intd.number = ntohl(intd.number);
				    	
				    	strcpy(info.topic, intd.topic);
				    	strcpy(info.type, "INT");
				    	if(intd.sign == 0)
				    		sprintf(info.content, "%d", intd.number);
				    	else
				    		sprintf(info.content, "-%d", intd.number);
				    } 
				    else if (received[50] == 1){

				    	memset(&shortd, 0, sizeof(shortd));
				    	memcpy(&shortd, received, sizeof(shortd));
				    	shortd.number = ntohs(shortd.number);

				    	char number[30];
				    	sprintf(number, "%g", (float)shortd.number/100);
				    	
				    	strcpy(info.topic, shortd.topic);
				    	strcpy(info.type, "SHORT_REAL");
				    	strcpy(info.content, number);		
				    }
				    else if (received[50] == 2){

				    	memset(&floatd, 0, sizeof(floatd));
				    	memcpy(&floatd, received, sizeof(floatd));
				    	floatd.number = ntohl(floatd.number);

				    	strcpy(info.topic, floatd.topic);
				    	strcpy(info.type, "FLOAT");
				    	if(floatd.sign == 0)
				    		sprintf(info.content, "%.*f", floatd.power, floatd.number/(powC(10, floatd.power)));
				    	else
				    		sprintf(info.content, "-%.*f", floatd.power, floatd.number/(powC(10, floatd.power)));	
				    }
				    else if (received[50] == 3){

				    	memset(&stringd, 0, sizeof(stringd));
				    	memcpy(&stringd, received, sizeof(stringd));

				    	strcpy(info.topic, stringd.topic);
				    	strcpy(info.type, "STRING");
				    	strcpy(info.content, stringd.payload);	
				    }

				    memset(info.ip, 0, sizeof(info.ip));
				    strcpy(info.ip, inet_ntoa(cliaddr.sin_addr));
				    sprintf(info.port, "%d", ntohs(cliaddr.sin_port));

				    memset(toSend, 0, sizeof(toSend));
				    memcpy(toSend, &info, sizeof(info));

				    for (j = 5; j < 100; j++){														//cautarea incepe cu portul 5
					    if (FD_ISSET(j, &read_fds))
					    	if (searchTopic(onlineClients[j], clients, clientsNumber, info.topic) == 1){
					    		n = send(j, toSend, sizeof(toSend), 0);
					    		DIE(n < 0, "send");
					    	}
				    }

				    int offs = offline(clients, clientsNumber, info.topic);
				    if (offs != 0){
				    	saved[storedTopics].info = info;
				    	saved[storedTopics].offlineSubs = offs;
				    	storedTopics ++;
				    }
				}
			}
		}		
	}
	close(sockfd);

	return 0;
}
