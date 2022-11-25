#include <fcntl.h>
#include <netinet/in.h>
#include <netdb.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <string.h>
#include <map>

#include "helpers.h"

using namespace std;

/**
 * Obiecte ce vor retine date despre topic-urile la care s-a abonat un client
 * cu ID-ul id.
 * 
 * id reprezinta numele clientului care s-a abonat la un anumit topic.
 * 
 * sf reprezinta optiunea clientului de a primi mesajele pe un anumit topic, pe
 * care le-a pierdut, la reconectare.
 */
class TopicClientData {
 public:
    char id[10];
	int sf;

    TopicClientData() {}

    /**
     * Seteaza id-ul clientului si optiunea de store-and-forward.
     */
    TopicClientData(char *newId, int newSf) {
        strncpy(id, newId, 10);
        sf = newSf;
    }
};

/**
 * Obiecte ce vor retine date despre clientii care s-au conectat la server.
 * 
 * sockfd reprezinta socket-ul pe care clientul poate primi date. Daca 
 * sockfd = -1, clientul este inactiv(nu mai este conectat la server).
 * 
 * messagesStored pastreaza mesaje venite de la clienti UDP ce vor fi trimise
 * imediat ce clientul va redeveni activ.
 */
class ClientData {
 public:
    int sockfd;
	vector<udpPacket> messagesStored;

    ClientData() {}

    /**
     * Seteaza sockfd. Se asigura ca messagesStored este gol.
     */
    ClientData(int newSockFd) {
        sockfd = newSockFd;
        messagesStored.clear();
    }
};

// Src: lab08
void usage(char *file) {
	fprintf(stderr,"Usage: %s <PORT>\n", file);
	exit(0);
}

/**
 * Functia preia datele din buf si completeaza o structura udpPacket ce va fi
 * returnata.
 * Se presupune ca in buffer se afla date venite de la un client UDP
 * Functia nu completeaza si datele despre IP-ul si portul de pe care clientul
 * UDP a trimis datele.
 */
udpPacket parseUDPmsg(char buf[]) {
    udpPacket recvUdpPacket;
    memset(&recvUdpPacket, 0, sizeof(udpPacket));
    memcpy(&recvUdpPacket.topic, buf, 50);
    recvUdpPacket.dataType = buf[50];

    uint16_t temp16;
    uint32_t temp32;
    switch (recvUdpPacket.dataType) {
        case 0:
            // Tip 0 - INT
            // Este necesara completarea campului de semn alaturi de numar.
            recvUdpPacket.data.type0.sign = buf[51];
            memcpy(&temp32, buf + 52, 4);
            recvUdpPacket.data.type0.number = ntohl(temp32);
            break;

        case 1:
            // Tip 1 - SHORT_REAL
            // Este necesara doar completarea numarului.
            memcpy(&temp16, buf + 51, 2);
            recvUdpPacket.data.type1 = ntohs(temp16);
            break;

        case 2:
            // Tip 2 - FLOAT
            // Este necesara completarea campului de semn, campului de precizie,
            // alaturi de numar.
            recvUdpPacket.data.type2.sign = buf[51];
            memcpy(&temp32, buf + 52, 4);
            recvUdpPacket.data.type2.precision = buf[56];
            recvUdpPacket.data.type2.number = ntohl(temp32);
            break;
        
        case 3:
            // Tip 3 - STRING
            // Este necesara doar copierea stringului.
            snprintf(recvUdpPacket.data.type3, MAXSTRLEN, "%s", buf + 51);
            break;

        default:
            break;
    }

    return recvUdpPacket;
}


int main(int argc, char** argv) {
    // Dezactivare buffering
    setvbuf(stdout, NULL, _IONBF, BUFSIZ);
    
    ///////////////////////// DECLARARE VARIABILE //////////////////////////////
	struct sockaddr_in srvSetup;        // Date despre socketi
	char buf[BUFLEN];                   // Buffer pentru citire
    int udpSockFd = -1;                 // Socket UDP
    int tcpSockFd = -1;                 // Socket TCP
    int res;                            // Variabila pentru rezultate functii
    
    // Map-ul clients va contine date despre toti clientii care s-au conectat
    // pe parcursul executiei serverului.
    // Maparea se va face astfel: <ID_Client, ClientData>
    map<string, ClientData> clients;

    // Map-ul topics va contine date despre ce clienti s-au abonat la un topic
    // pe parcursul executiei serverului.
    // Maparea se va face astfel: <Denumire_Topic, map<ID_Client, ClientData>>
    map<string, map<string, TopicClientData>> topics;

    // Iterator pentru map-ul clients
	map<string, ClientData>::iterator clientsItr;

    // Iterator pentru map-ul topics
	map<string, map<string, TopicClientData>>::iterator topicClientsItr;
    
    tcpPacket recvTcpPacket;        // Pachet TCP receptionat
    
    fd_set read_fds;	            // multimea de citire folosita in select()
	fd_set tmp_fds;		            // multime folosita temporar
	int fdmax;			            // valoare maxima fd din multimea read_fds

    int port;                       // Portul pe care va functiona serverul
	///////////////////// SFARSIT DECLARARE VARIABILE //////////////////////////
    
    
    // Verificare numar argumente.
    if (argc != 2) {
		usage(argv[0]);
    }

    // Verificare port .
    port = atoi(argv[1]);
    DIE(port == 0, "atoi port init");

    // Golim multimea de FD-uri.
    FD_ZERO(&read_fds);
	FD_ZERO(&tmp_fds);

	// Deschidere socket UDP.
	udpSockFd = socket(AF_INET, SOCK_DGRAM, 0);
	DIE(udpSockFd == -1, "UDP socket open failed");

    // Deschidere socket TCP.
    tcpSockFd = socket(AF_INET, SOCK_STREAM, 0);
    DIE(udpSockFd == -1, "TCP socket open failed");
	
	// Setare struct sockaddr_in pentru a asculta pe portul respectiv.
	srvSetup.sin_family = AF_INET;
	srvSetup.sin_port = htons(port);
	srvSetup.sin_addr.s_addr = INADDR_ANY;
	socklen_t sizeSrvSetup = sizeof(srvSetup);
	
    // Deazactivare Algortim Neagle.
    int opt = 1;
    res = setsockopt(tcpSockFd, IPPROTO_TCP, TCP_NODELAY, (char *) &opt,
                                                                sizeof(int));
    DIE(res < 0, "Neagle Algorithm deactivation failed");
	
    // Bind socket UDP.
	res = bind(udpSockFd, (struct sockaddr *) &srvSetup, sizeof(srvSetup));
	DIE(res < 0, "UDP bind error");

    // Bind socket TCP.
    res = bind(tcpSockFd, (struct sockaddr *) &srvSetup, sizeof(srvSetup));
	DIE(res < 0, "TCP bind error");

    // Listen socket TCP.
    res = listen(tcpSockFd, SOMAXCONN);
	DIE(res < 0, "TCP listen error");

    // Actualizare FD-uri cu socketi TCP & UDP + STDIN.
    FD_SET(STDIN_FILENO, &read_fds);
    FD_SET(udpSockFd, &read_fds);
    FD_SET(tcpSockFd, &read_fds);
    
    // Determinare fdmax
    fdmax = udpSockFd < tcpSockFd ? tcpSockFd : udpSockFd;
    
    // Bucla infinita pentru handle-uirea de clienti si mesaje TCP/UDP.
    while (1) {
		tmp_fds = read_fds; 
		res = select(fdmax + 1, &tmp_fds, NULL, NULL, NULL);
		DIE(res < 0, "select");
        
        if (FD_ISSET(STDIN_FILENO, &tmp_fds)) {
            // Citire de la tastatura.
            memset(buf, 0, BUFLEN);
            fgets(buf, BUFLEN - 1, stdin);

            // Inchidere in caz ca s-a primit "exit".
            if (strncmp(buf, "exit", 4) == 0) {
                // Inchidem toti clientii conectati.
                for (clientsItr = clients.begin();
                            clientsItr != clients.end(); ++clientsItr) {
                    // Trimitem mesaje de inchidere catre toti clientii
                    // conectati.
                    if (clientsItr->second.sockfd != -1) {
                        udpPacket sentUDPpacket;
                        sentUDPpacket.dataType = -1;
                        res = send(clientsItr->second.sockfd, &sentUDPpacket,
                                                        sizeof(udpPacket), 0);
                        shutdown(clientsItr->second.sockfd, SHUT_RDWR);
                        close(clientsItr->second.sockfd);
                        FD_CLR(clientsItr->second.sockfd, &read_fds);
                    }
                }
                break;
            }

            // In cazul in care de la tastatura nu s-a citit exit, bufferul nu
            // este utilizat.
        }

        // Vom verifica toti file-descriptorii aflati intre 0 si fdmax.
        for (int i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &tmp_fds)) {
                // Avem un fd conectat la server.
                if (i == udpSockFd) {
                    // Primire mesaj de la client UDP.
                    memset(buf, 0, BUFLEN);
                    int rf = recvfrom(udpSockFd, buf, BUFLEN, 0,
                                (struct sockaddr *) (&srvSetup), &sizeSrvSetup);
                    // Daca nu s-a primit nimic, se merge mai departe.
                    if (rf < 0) {
                        break;
                    }

                    // Parsare mesaj UDP.
                    udpPacket recvUdpPacket = parseUDPmsg(buf);
                    strcpy(recvUdpPacket.ip_src, inet_ntoa(srvSetup.sin_addr));
                    recvUdpPacket.port_src = ntohs(srvSetup.sin_port);  
                    
                    // Trimitere mesaje catre clientii care au dat subscribe
                    // la acest topic.
                    topicClientsItr = topics.find(string(recvUdpPacket.topic));
                    if (topicClientsItr != topics.end()) {
                        for (auto const& currClient : topicClientsItr->second) {
                            if (clients[string(currClient.second.id)]
                                                                .sockfd != -1) {
                                // Trimitem mesajul catre clientii conectati
                                res = send(clients[string(currClient.second.id)]
                                        .sockfd, &recvUdpPacket,
                                                 sizeof(udpPacket), 0);
                                DIE(res < 0, "sending data to TCP Client");
                            } else if (currClient.second.sf == 1) {
                                // Punem mesajul in coada de trimis pentru
                                // clientii neconectati dar abonati la acest
                                // topic
                                clients[string(currClient.second.id)]
                                    .messagesStored.push_back(recvUdpPacket);
                            }
                        }
                    }
                } else if (i == tcpSockFd) {
                    // Conectare noua pe socketul TCP.
                    struct sockaddr_in cliSetup;
                    socklen_t clilen;
                    clilen = sizeof(cliSetup);

                    // Asignare fd nou
                    int newClientSockFd = accept(tcpSockFd,
                                                (struct sockaddr*) &cliSetup,
                                                &clilen);
                    
                    // Adaugare fd in lista
                    FD_SET(newClientSockFd, &read_fds);

                    // Actualizare fdmax, daca este cazul
                    fdmax = fdmax < newClientSockFd ? newClientSockFd : fdmax;

                    // Primire pachet TCP initial
                    res = recv(newClientSockFd, &recvTcpPacket,
                                                sizeof(tcpPacket), 0);
                    DIE(res < 0, "Error receiving initial data from client");
                    DIE(!recvTcpPacket.firstContact, "Initial data packet from \
                                        client has firstContact set as false");

                    // Cautare un client cu ID-ul trimis in pachetul TCP pentru
                    // primul contact.
                    clientsItr = clients.find(string(recvTcpPacket.userID));
                    
                    if (clientsItr != clients.end()) {
                        if (clientsItr->second.sockfd != -1) {
                            // Avem deja un client cu id-ul primit care este
                            // deja conectat. Trimitem mesajul de exit catre
                            // client. Afisam mesajul corespunzator in server.
                            printf("Client %s already connected.\n",
                                                        recvTcpPacket.userID);
                            udpPacket sentUDPpacket;
                            sentUDPpacket.dataType = -1;
                            res = send(newClientSockFd, &sentUDPpacket,
                                                        sizeof(udpPacket), 0);
                            DIE(res < 0, "'Already connected' message send");
                            
                            // Scoatem fd-ul din lista
                            FD_CLR(newClientSockFd, &read_fds);

                            // Inchidem socket-ul
                            shutdown(newClientSockFd, SHUT_RDWR);
                            close(newClientSockFd);
                        } else {
                            // Avem deja un client cu id-ul primit care NU este
                            // conectat. Actualizam socket fd-ul utilizatorului.
                            clientsItr->second.sockfd = newClientSockFd;
                            printf("New client %s connected from %s:%hu\n",
                                            recvTcpPacket.userID, 
                                            inet_ntoa(cliSetup.sin_addr),
                                            ntohs(cliSetup.sin_port));

                            // Trimitem toate mesajele pe care clientul
                            // reconectat le-a ratat.
                            for (auto const& currentPacket :
                                            clientsItr->second.messagesStored) {
                                res = send(clientsItr->second.sockfd,
                                            &currentPacket,
                                            sizeof(udpPacket),
                                            0);
                                DIE(res < 0, "sending data to TCP Client");
                            }

                            // Stergem toate mesajele care au fost trimise.
                            clientsItr->second.messagesStored.clear();
                        }
                    } else {
                        // Nu avem un client cu acest ID.
                        // Adaugare client in vector Afisare mesaj in server.
                        clients.insert(pair<string, ClientData>
                                            (string(recvTcpPacket.userID),
                                             ClientData(newClientSockFd)));
                        printf("New client %s connected from %s:%hu\n",
                                    recvTcpPacket.userID, 
                                    inet_ntoa(cliSetup.sin_addr),
                                    ntohs(cliSetup.sin_port));
                    }
                } else {
                    // Primire pachet TCP.
                    memset(&recvTcpPacket, 0, sizeof(tcpPacket));
                    res = recv(i, &recvTcpPacket, sizeof(tcpPacket), 0);
                    
                    if (res == 0) {
                        // S-a primit un pachet cu 0 bytes, deci clientul s-a
                        // deconectat.
                        
                        // Cautam clientul cu acest fd.
                        for (clientsItr = clients.begin();
                                    clientsItr != clients.end(); clientsItr++) {
                            if (clientsItr->second.sockfd == i) {
                                break;
                            }
                        }
                        DIE(clientsItr == clients.end(),
                                "searchClientBySock: no id with this socket");

                        // Afisare mesaj deconectare
                        printf("Client %s disconnected.\n",
                                                    clientsItr->first.c_str());
                        
                        // Inchidere socket TCP pentru client.
                        shutdown(i, SHUT_RDWR);
                        close(i);
                        
                        // Setare camp fd client ca inactiv
                        clientsItr->second.sockfd = -1;

                        // Actualizare lista fd
                        FD_CLR(i, &read_fds);
                    } else {
                        // Mesaj subscribe/unsubscribe.
                        
                        // Verificare ca pachetul primit este de tipul asteptat.
                        if (recvTcpPacket.firstContact == 1) {
                            // Daca avem un firstContact neasteptat, aruncam
                            // pachetul. Afisam mesaj de eroare la stderr.
                            fprintf(stderr, "[ERROR] Normal TCP packet received\
                                 with firstContact set as true(Client: %s)\n",
                                recvTcpPacket.userID);
                            break;
                        }

                        if (recvTcpPacket.subscribe == 1) {
                            // S-a primit o cerere de subscribe.

                            // Cautare topic in baza de date.
                            topicClientsItr = topics.find(string(recvTcpPacket.topic));
                            if (topicClientsItr == topics.end()) {
                                // Nu avem acest topic in baza de date.

                                // Adaugam topicul in baza de date.
                                topics.insert(pair<string,map<string, TopicClientData>>
                                    (string(recvTcpPacket.topic), map<string, TopicClientData>()));
                                topicClientsItr = topics.find(string(recvTcpPacket.topic));
                            }
                            
                            // Adaugam clientul in cadrul acestui topic.
                            topicClientsItr->second.insert(pair<string, TopicClientData>
                                (string(recvTcpPacket.userID), TopicClientData
                                                        (recvTcpPacket.userID, recvTcpPacket.sf)));
                            
                        } else if (recvTcpPacket.subscribe == 0) {
                            // S-a primit o cerere de unsubscribe.

                            // Gaseste topic-ul corespunzator.
                            topicClientsItr = topics.find(string(recvTcpPacket.topic));
                            if (topicClientsItr != topics.end()) {
                                // Efectuam actiuni doar daca topicul exista deja.
                                topicClientsItr->second.erase(string(recvTcpPacket.userID));
                            }
                        }
                    }
                }
            }
        }
    }


	/* Inchidere socket UDP */	
	shutdown(udpSockFd, SHUT_RDWR);
    close(udpSockFd);

    /* Inchidere socket TCP */	
	shutdown(tcpSockFd, SHUT_RDWR);
    close(tcpSockFd);

	return 0;
}
