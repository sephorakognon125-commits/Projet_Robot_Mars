#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "protocol.h"

#define PORT_TERRE 8080
#define PORT_ALPHA 8081

void ecrire_log(const char *message) {
    FILE *f = fopen("rover.log", "a");
    if (f == NULL) return;
    time_t now = time(NULL);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';
    fprintf(f, "[%s] %s\n", date, message);
    fclose(f);
}

// Fonction de déplacement case par case [cite: 37]
void avancer_vers(int sock, Paquet *p, int cibleX, int cibleY) {
    char buffer[256];
    // Horizontal
    while (p->x != cibleX) {
        if (p->x < cibleX) p->x++; else p->x--;
        sprintf(buffer, "Marche H vers (%d, %d)", p->x, p->y);
        printf("[Rover] %s\n", buffer);
        ecrire_log(buffer);
        if ((rand() % 5) == 0) { // Chance de trésor 
            p->type = MSG_TRESOR_TROUVE;
            send(sock, p, sizeof(Paquet), 0);
            ecrire_log("TRÉSOR TROUVÉ !");
        }
        usleep(300000);
    }
    // Vertical
    while (p->y != cibleY) {
        if (p->y < cibleY) p->y++; else p->y--;
        sprintf(buffer, "Marche V vers (%d, %d)", p->x, p->y);
        printf("[Rover] %s\n", buffer);
        ecrire_log(buffer);
        if ((rand() % 5) == 0) {
            p->type = MSG_TRESOR_TROUVE;
            send(sock, p, sizeof(Paquet), 0);
            ecrire_log("TRÉSOR TROUVÉ !");
        }
        usleep(300000);
    }
}

int main() {
    int sock = 0;
    struct sockaddr_in serv_addr;
    Paquet p;
    srand(time(NULL));

    p.rover_id = 125; // Ton ID GitHub
    p.x = 0; p.y = 0; p.batterie = 100;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr);
    serv_addr.sin_port = htons(PORT_TERRE);

    // TENTATIVE 1 : Connexion à la Terre [cite: 22]
    printf("[Rover] Connexion à la Terre (Port %d)...\n", PORT_TERRE);
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        
        // TENTATIVE 2 : Bascule vers Alpha si Terre KO [cite: 44, 46]
        printf("[Rover] Terre injoignable. Tentative vers ROVER ALPHA...\n");
        ecrire_log("Bascule vers mode Rover Alpha");
        close(sock);
        
        sock = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_port = htons(PORT_ALPHA); // Port Alpha [cite: 52]
        
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            printf("[Rover] Erreur : Aucun serveur trouvé. Fin de mission.\n");
            return -1;
        }
        printf("[Rover] Connecté au ROVER ALPHA (Port %d).\n", PORT_ALPHA);
    } else {
        printf("[Rover] Connecté au CENTRE TERRE.\n");
    }

    // Boucle de mission (5 cycles)
    for(int i = 0; i < 5; i++) {
        p.type = MSG_DEMANDE_ACTION;
        if (send(sock, &p, sizeof(Paquet), 0) <= 0) break;
        if (recv(sock, &p, sizeof(Paquet), 0) <= 0) {
            printf("[Rover] Perte de connexion !\n");
            break;
        }

        if (p.type == MSG_RECHARGE) {
            printf("[Rover] Ordre reçu : RECHARGE (%d%%)...\n", p.batterie);
            sleep(2);
            p.batterie = 100;
        } else {
            printf("[Rover] Nouveau cap : (%d, %d)\n", p.x, p.y);
            avancer_vers(sock, &p, p.x, p.y);
            p.batterie -= 10;
        }
    }

    printf("[Rover] Mission terminée.\n");
    close(sock);
    return 0;
}