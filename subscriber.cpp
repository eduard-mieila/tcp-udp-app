#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>

#include "helpers.h"

/**
 * Functia preia datele din recvUdpPacket si scrie mesajul ce urmeaza sa fie
 * afisat in msg.
 * Se presupune ca mesajele sunt de tip 0, 1, 2 sau 3.
 * Functia nu scrie si datele despre IP-ul si portul de pe care clientul UDP
 * a trimis datele.
 */
void parseUDPstruct(udpPacket recvUdpPacket, char* msg) {

    switch (recvUdpPacket.dataType) {
        case 0:
            // Tip 0 - INT
            // Afisare in functie de semn
            if (recvUdpPacket.data.type0.sign == 1) {
                sprintf(msg, "%s - INT - -%u", recvUdpPacket.topic,
                                            recvUdpPacket.data.type0.number);
            } else {
                sprintf(msg, "%s - INT - %u", recvUdpPacket.topic,
                                            recvUdpPacket.data.type0.number);
            }
            break;

        case 1:
            // Tip 1 - SHORT_REAL
            // Numarul este trimis fara virgula => pentru afisarea cu 2 zecimale
            // impartim la 10^2.
            sprintf(msg, "%s - SHORT_REAL - %.2f", recvUdpPacket.topic,
                                            recvUdpPacket.data.type1 / 100.0);
            break;

        case 2:
            // Tip 2 - FLOAT
            // Afisare in functie de semn si de precizie
            // La fel ca la tipul 1, numarul este trimis fara virgula, asadar se
            // face impartirea la 10^precision si se specifica precizia cu care
            // se doreste sa se faca afisarea.
            if (recvUdpPacket.data.type2.precision) {
                if (recvUdpPacket.data.type2.sign) {
                    sprintf(msg, "%s - FLOAT - -%.*f", recvUdpPacket.topic,
                            recvUdpPacket.data.type2.precision,
                            (float)recvUdpPacket.data.type2.number /
                                    qPow10(recvUdpPacket.data.type2.precision));
                } else {
                    sprintf(msg, "%s - FLOAT - %.*f", recvUdpPacket.topic,
                            recvUdpPacket.data.type2.precision,
                            (float)recvUdpPacket.data.type2.number /
                                    qPow10(recvUdpPacket.data.type2.precision));
                }
            } else {
                if (recvUdpPacket.data.type2.sign) {
                    sprintf(msg, "%s - FLOAT - -%u", recvUdpPacket.topic,
                                            recvUdpPacket.data.type2.number);
                } else {
                    sprintf(msg, "%s - FLOAT - %u", recvUdpPacket.topic,
                                            recvUdpPacket.data.type2.number);
                }
            }
            break;
        
        case 3:
            // Tip 3 - STRING
            // Simpla formare a mesajului
            sprintf(msg, "%s - STRING - %s", recvUdpPacket.topic, recvUdpPacket.data.type3);
            break;

        default:
            break;
        }
    
}

int main(int argc, char** argv) {
    // Dezactivare buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);

    fd_set read_fds;	// multimea de citire folosita in select()
	fd_set tmp_fds;		// multime folosita temporar
    struct sockaddr_in serverAddress;
    udpPacket recvUdpPacket;
    int res, port;
    char id[10], buffer[BUFLEN], trash[BUFLEN];

    // Verificare numar parametri
    if (argc < 4) {
        printf("Usage: %s <CLIENT_ID> <SERVER_IP> <SERVER_PORT>\n", argv[0]);
        return 0;
    }

    // Copiere ID si port
    memcpy(id, argv[1], 10);
    port = atoi(argv[3]);
    
    // Deschidere socket
    int TCPsockfd = socket(AF_INET, SOCK_STREAM, 0);
	DIE(TCPsockfd < 0, "socket");

    // Setare parametri socket
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_port = htons(port);
    res = inet_aton(argv[2], &serverAddress.sin_addr);
    DIE(res < 0, "IP address conversion");
    socklen_t sizeOfSocket = sizeof(serverAddress);
    
    // Deazactivare Algortim Neagle
    int opt = 1;
    res = setsockopt(TCPsockfd, IPPROTO_TCP, TCP_NODELAY, (char *) &opt, sizeof(int));
    DIE(res < 0, "Neagle Algorithm deactivation failed");
    
    // Conectare socket
    res = connect(TCPsockfd, (struct sockaddr *) &serverAddress, sizeOfSocket);
    DIE(res < 0, "connect");

    // Trimitere pachet initial
    tcpPacket sentTcpPacket;
    sentTcpPacket.firstContact = true;
    strncpy(sentTcpPacket.userID, id, 10);
    res = send(TCPsockfd, &sentTcpPacket, sizeof(tcpPacket), 0);
    DIE(res < 0, "IDsend");

    // Setare FD-uri pentru select
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	FD_SET(TCPsockfd, &read_fds);
	FD_SET(STDIN_FILENO, &read_fds);

    while (1) {
		tmp_fds = read_fds; 
		
		res = select(TCPsockfd + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(res < 0, "select");

		if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
			// se citeste de la tastatura
			memset(buffer, 0, BUFLEN);
			fgets(buffer, BUFLEN - 1, stdin);

			if (strncmp(buffer, "exit", 4) == 0) {
				// S-a primit exit de la tastatura
                // Se iese din bucla infinta
                break;
			}

            memset(&sentTcpPacket, 0, sizeof(tcpPacket));
            
            // Trimitere mesaj subscribe
            if (strncmp(buffer, "subscribe", 9) == 0) {
                sentTcpPacket.firstContact = false;
                sentTcpPacket.subscribe = 1;
                sscanf(buffer, "%s %s %d", trash, sentTcpPacket.topic, &sentTcpPacket.sf);
                memcpy(sentTcpPacket.userID, id, 10);
                res = send(TCPsockfd, &sentTcpPacket, sizeof(sentTcpPacket), 0);
                DIE(res < 0, "send subscribe");
                printf("Subscribed to topic.\n");
            }

            // Trimitere mesaj unsubscribe
            if (strncmp(buffer, "unsubscribe", 11) == 0) {
                sentTcpPacket.firstContact = false;
                sentTcpPacket.subscribe = 0;
                sscanf(buffer, "%s %s", trash, sentTcpPacket.topic);
                memcpy(sentTcpPacket.userID, id, 10);
                res = send(TCPsockfd, &sentTcpPacket, sizeof(sentTcpPacket), 0);
                DIE(res < 0, "send subscribe");
                printf("Unsubscribed from topic.\n");
            }
		}

		if (FD_ISSET(TCPsockfd, &tmp_fds)) {
			// Primeste mesaj
			memset(&recvUdpPacket, 0, sizeof(udpPacket));
			res = recv(TCPsockfd, &recvUdpPacket, sizeof(udpPacket), 0);
			DIE(res < 0, "recv");

            if (recvUdpPacket.dataType == -1) {
                // S-a primit mesaj de inchidere; actionam in consecinta.
                close(TCPsockfd);
                return 0;
            }

			// Parseaza mseaj si afiseaza
            char msg[BUFLEN];
            memset(msg, 0, BUFLEN);
            parseUDPstruct(recvUdpPacket, msg);
            printf("%s:%d - %s\n", recvUdpPacket.ip_src, recvUdpPacket.port_src,
                                msg);
		}
	}

    // Inchidere socket TCP
	close(TCPsockfd);

    return 0;
}
