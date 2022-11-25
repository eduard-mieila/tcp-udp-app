Tema 2 - Aplicatie client-server TCP si UDP pentru gestionarea mesajelor - Protocoale de Comunicatie
Copyright (c) 2021. MIEILĂ Eduard-Robert, 323CA
edymieila@gmail.com
eduard.mieila@stud.acs.upb.ro

Tema contine:
│
├── 1. Un server TCP/UDP care se ocupa cu gestionarea mesajelor intre clientii UDP si clientii TCP.
│      Implementare in fisierul server.cpp
│
├── 2. Un client TCP(subscriber) care trimite cereri de abonare/dezabonare catre server si primeste
│      mesaje de la server pe topic-urile pe care s-a abonat.
│      Implementare in fisierul subscriber.cpp
└── 3. Un fisier helpers.h cu functii ajutatoare, structuri si macrouri utilizate

Server-ul TCP/UDP va functiona dupa urmatoarea schema:
│
├── 0. Configurări - verificare parametri server, deschidere socketi, efectuare bind,
│                    dezactivare algoritm Neagle, efectuare listen pentru socket TCP,
│                    actualizare lista read_fds, determinare fdmax, dezactivare buffering
│
├── 1. Citire de la tastatură
│   │
│   ├── 1.A. S-a citit "exit"
│   │   │
│   │   ├───── trimite mesaj de exit către toți clienții conectați
│   │   │
│   │   ├───── închide sockeții clienților
│   │   │
│   │   └───── iese din buclă
│   │
│   └── 1.B. S-a primit orice altceva - se ignoră input-ul
│
│
└── 2. Verificare file-descriptori din read_fds
    │
    ├─── 2.A. S-a primit mesaj pe socket-ul UDP
    │    │
    │    ├──── Se parsează mesajul UDP si se scriu datele intr-o structura udpPacket
    │    │
    │    ├──── Se trimit datele catre clientii conectati si abonati la topic-ul
    │    │
    │    └──── Se adauga mesajele in coada pentru fiecare client abonat la topic-ul respectiv
    │          dacă are SF=1
    │
    │
    │
    ├─── 2.B. Se realizeaza o conexiune nouă pe socket-ul TCP
    │    │
    │    ├──── Se asigneaza un socket nou, se adauga fd-ul in read_fds, se actualizează
    │    │     fd_max(dacă este cazul)
    │    │
    │    └──── Se primește un pachet TCP inițial de la client
    │          │
    │          ├── Dacă avem un client activ cu același ID, trimitem mesaj de
    │          │   închidere către client, scoatem fd-ul lui din read_fds, inchidem
    │          │   socketul nou alocat, afisăm mesajul corespunzător în server
    │          │
    │          ├── Dacă avem un client inactiv cu acest ID, actualizăm fd-ul lui,
    │          │   trimitem mesajele pe topic-urile pe care s-a abonat cu SF=1,
    │          │   stergem coada de mesaje, afisăm mesajul de conectare în server
    │          │
    │          └── Dacă nu avem un client cu acest ID în baza de date, îl adăugăm și
    │              afișăm mesajul de conectare în server
    │
    │
    └─── 2.C. S-a primit un mesaj TCP de la unul dintre clienții activi
         │
         ├───── Dacă s-a primit un pachet de 0 bytes, clientul s-a deconectat, deci
         │      inchidem socketul, setam clientul ca fiind inactiv, actualizam read_fds
         │      afișăm mesajul de deconectare în server
         │
         └───── S-a primit un mesaj de subscribe/unsubscribe
                │
                ├─── Dacă s-a primit un mesaj de subscribe se verifică dacă topic-ul există
                │    deja în baza de date - dacă nu, se creează - se adaugă clientul în
                │    baza de date a topicului cu SF-ul setat corespunzător
                │
                └─── Dacă s-a primit un mesaj de unsubscribe, se scoate clientul din baza de
                     date a topicului

Nota: pasii 1 si 2 se vor rula in bucla infinita. La final se inchid socketii TCP si UDP.


Subscriber-ul va functiona astfel:
│
├── 0. Configurări - verificare parametri subscriber, deschidere socketi, efectuare bind,
│                    dezactivare algoritm Neagle, efectuare connect pentru socket TCP,
│                    actualizare lista read_fds, dezactivare buffering
│
├── 1. Citire de la tastatură, daca este cazul
│   │
│   ├── 1.A. S-a citit "exit" - iese din bucla
│   │
│   ├── 1.B. Daca s-a primit subscribe/unsubscribe, se trimit mesaje corespunzatoare catre server
│   │
│   └── 1.C. S-a primit orice altceva - se ignoră input-ul
│
└── 2. Citire de la socket, daca este cazul
    │
    ├── 2.A. Daca s-a primit mesaj de inchidere - iese din bucla
    │
    └── 2.B. Daca s-a primit alt mesaj, se parseaza mesajul si se afiseaza
Nota: pasii 1 si 2 se vor rula in bucla infinita. La final se inchide socketul TCP.


Intre clienti si server vor circula urmatoarele tipuri de mesaje:
De la:        Catre:        TipMesaj
-------------------------------------------------
Server        Client TCP    udpPacket(detalii despre acesta se gasesc in helpers.h)
Client TCP    Server        tcpPacket(detalii despre acesta se gasesc in helpers.h)
Client UDP    Server        Flux de octeti ce sunt prsati intr-un udpPacket