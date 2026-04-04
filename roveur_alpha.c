#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "protocol.h"

/**
 * FONCTION : journaliser_alpha
 * ----------------------------
 * Comme pour la Terre, Alpha doit garder une trace de ses décisions
 * lorsqu'il passe en mode "Chef de mission".
 */
void journaliser_alpha(int id_rover, const char* action) {
    FILE *f = fopen("rover_alpha.log", "a");
    if (f == NULL) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "[%s] [ALPHA RELAIS] Rover %d | %s\n", s_now, id_rover, action);
    fflush(f);
    fclose(f);
}

int main() {
    int sock_client, srv_fd, new_sock;
    struct sockaddr_in serv_addr;
    int opt = 1;
    Paquet p;

    printf("[ROVER ALPHA] Initialisation du système de secours...\n");

    // --- ÉTAPE 1 : TENTATIVE DE CONNEXION À LA TERRE (Mode Client) ---
    // Alpha essaie de voir si la base (Port 8080) est active.
    sock_client = socket(AF_INET, SOCK_STREAM, 0);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT_TERRE); // 8080
    inet_pton(AF_INET, ADRESSE_TERRE, &serv_addr.sin_addr);

    printf("[ALPHA] Vérification de la liaison Terre (Port %d)...\n", PORT_TERRE);

    if (connect(sock_client, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) >= 0) {
        // SI LA TERRE RÉPOND : Alpha se comporte comme un rover normal.
        printf("[ALPHA] Liaison Terre OK. Mode exploration standard.\n");
        p.id_envoyeur = 99; // ID spécial pour Alpha
        p.type = STATUS_QUO;
        send(sock_client, &p, sizeof(Paquet), 0);
        close(sock_client);
        exit(0); // Dans ce cas, Alpha a fini son job de test.
    }

    // --- ÉTAPE 2 : MUTATION EN MODE RELAIS (Mode Serveur) ---
    // Si connect() a échoué, on arrive ici. Alpha prend le contrôle du réseau.
    printf("[ALERTE] Terre injoignable ! Mutation en CENTRE DE CONTRÔLE DE SECOURS.\n");
    close(sock_client);

    // Création du socket serveur pour écouter les autres Rovers
    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // IMPORTANT : Alpha écoute sur le PORT_ALPHA (8081) 
    // pour que les rovers qui basculent puissent le trouver.
    serv_addr.sin_port = htons(PORT_ALPHA); 
    serv_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(srv_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("Échec mutation Bind");
        exit(EXIT_FAILURE);
    }

    listen(srv_fd, 5);
    printf("[ALPHA] Serveur de relais actif sur le port %d. En attente des rescapés...\n", PORT_ALPHA);

    while (1) {
        // Attente d'une connexion d'un rover "perdu"
        new_sock = accept(srv_fd, NULL, NULL);
        if (new_sock < 0) continue;

        if (read(new_sock, &p, sizeof(Paquet)) > 0) {
            printf("[ALPHA RELAIS] Requête reçue du Rover %d (Position: %d,%d)\n", p.id_envoyeur, p.x, p.y);

            // LOGIQUE DE SECOURS : Alpha donne des ordres de prudence
            // On ne veut pas que les rovers s'éparpillent sans la Terre.
            p.type = MODE_SECOURS; 
            strcpy(p.corps, "Ici Alpha. Liaison Terre perdue. RECHARGEZ et attendez les instructions.");
            
            // On force la recharge pour économiser l'énergie en attendant la Terre
            journaliser_alpha(p.id_envoyeur, "ORDRE_SURVIE_ENVOYE");

            send(new_sock, &p, sizeof(Paquet), 0);
        }
        close(new_sock);
        printf("[ALPHA] Rover %d mis en sécurité.\n--------------------------\n", p.id_envoyeur);
    }

    close(srv_fd);
    return 0;
}
