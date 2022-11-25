#ifndef _HELPERS_H
#define _HELPERS_H 1

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <vector>
#include <queue>

#define DEBUG 1

/*
* Macro de verificare a erorilo
* Exemplu: 
* 		int fd = open (file_name , O_RDONLY);	
* 		DIE( fd == -1, "open failed");
*/

#define DIE(assertion, call_description)				\
	do {								\
		if (assertion) {					\
			fprintf(stderr, "(%s, %d): ",			\
					__FILE__, __LINE__);		\
			perror(call_description);			\
			exit(EXIT_FAILURE);				\
		}							\
	} while(0)
	
/* Dimensiunea maxima a calupului de date */
#define BUFLEN 1600
#define MAXSTRLEN 1501
#define TOPICLEN 51
#define IPCHARLEN 16
#define IDLEN 11

/**
 * Structura pentru mesajele cu tip de date 0 - INT
 */
struct type0_s {
	char sign;
	uint32_t number;
};

/**
 * Structura pentru mesajele cu tip de date 2 - FLOAT
 */
struct type2_s {
	char sign;
	uint32_t number;
	uint8_t precision;
};

/**
 * Uniune ce va cotine un singur tip de mesaj primit de la un client UDP.
 * Tipurile 0(INT) si 2(FLOAT) vor folosi strucrturile anterior definite.
 * Tipurile 1(SHORT_REAL) si 3(STRING) vor folosi tipuri deja definite, adica
 * un uint16_t respectiv un vector de octeti.
 */
union content {
	struct type0_s type0;
	uint16_t type1;
	struct type2_s type2;
	char type3[MAXSTRLEN];
};

/**
 * Structura ce retine un mesaj primit de la clienti UDP.
 * ip_src este IP-ul clientului UDP care a trimis mesajul.
 * port este portul clientului UDP care a trimis mesajul.
 * Aceasta contine numele unui topic, tipul de date codificat astfel:
 *    ->  0 - INT
 *    ->  1 - SHORT_REAL
 *    ->  2 - FLOAT
 * 	  ->  3 - STRING
 *    -> -1 - mesaj de inchidere pentru un client TCP.
 * 
 * De asemenea, structura contine si campul data, care este o uniune content.
 * 
 * In cazul in care se trimite un mesaj de inchidere, celealte campuri nu este
 * necesar sa fie completate.
 */
typedef struct udpPacket_t {
	char ip_src[IPCHARLEN];
	int port_src;
	char topic[TOPICLEN];
	char dataType;
	union content data;
} udpPacket;

/**
 * Structura ce contine date despre un client TCP.
 * 
 * In cazul in care firstContact este setat ca true, clientul trimite doar ID-ul
 * sau catre server pentru a fi inregistrat in baza de date a acestuia. 
 * In cazul in care se trimite un mesaj de (un)subscribe, firstContact va fi 
 * setat ca false. In caz contrar, serverul va arunca pachetul.
 * 
 * Daca se trimite un mesaj de (un)subscribe se vor seta in mod obligatoriu
 * campurile userID si topic.
 * Pentru o comanda de subscribe, campul de subscribe va fi setat pe 1, iar
 * pentru unsubscribe, pe 0.
 * Campul sf va fi setat doar in cazul unui mesaj de subscribe.
 */
typedef struct tcpPacket_t {
	bool firstContact;
	char userID[IDLEN];
	int subscribe;
	char topic[TOPICLEN];
	int sf;

} tcpPacket;


/**
 * Functie ce returenaza valoarea lui 10^n.
 * 0 <= n < 10
 * 
 * Src: https://stackoverflow.com/a/18581693
 */
int qPow10(int n) {
    static int p10[10] = {
        1, 10, 100, 1000, 10000, 
        100000, 1000000, 10000000, 100000000, 1000000000
    };

    return p10[n]; 
}

#endif
