#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>

/*
 * Macro de verificare a erorilor
 * Exemplu:
 *     int fd = open(file_name, O_RDONLY);
 *     DIE(fd == -1, "open failed");
 */

#define DIE(assertion, call_description)	\
	do {									\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",	\
					__FILE__, __LINE__);	\
			perror(call_description);		\
			exit(EXIT_FAILURE);				\
		}									\
	} while(0)

#define BUFLEN		1600	// dimensiunea maxima a calupului de date
#define MAX_CLIENTS	5	// numarul maxim de clienti in asteptare

typedef struct{
	char topic[50];
	char type[11];
	char content[1500];
	char ip[16];
	char port[6];
} store;

typedef struct{
	char topic[50];
	char data_type;
	char sign;
	uint32_t number;
} intDatagram;

typedef struct{
	char topic[50];
	char data_type;
	uint16_t number;
} shortDatagram;

typedef struct{
	char topic[50];
	char data_type;
	char sign;
	uint32_t number;
	uint8_t power;
} floatDatagram;

typedef struct {
	char topic[50];
	char data_type;
	char payload[1500];
} stringDatagram;

typedef struct{
	char clientID[11];
	char topics[100][50];
	int SF[100];
	int online;
	int topicNumber;
} clientInfo;

typedef struct{

	store info;
	int offlineSubs;
} storage;

#endif
