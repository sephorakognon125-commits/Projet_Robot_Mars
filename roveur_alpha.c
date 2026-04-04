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
 * Enregistre les actions d'Alpha lorsqu'il passe en mode "Chef de Mission".
 */
void journaliser_alpha(int id_rover, const char* action) {
    FILE *f = fopen("rover_alpha.log", "a");
    if (f == NULL) return;
    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);
    fprintf(f, "[%s] [ALPHA-RELAIS] Rover %d | ACTION: %s\n", s_now, id_rover, action);
    fflush(f);
    fclose(f);
}

int main() {
    int sock_test, srv_fd;
    struct sockaddr_in addr;
    int opt = 1;

    printf("[ROVER ALPHA] Système de veille de la Station Terre activé.\n");

    // --- ÉTAPE 1 : SURVEILLANCE DE LA TERRE (Mode Sentinelle) ---
    /* Le Rover Alpha essaie de se connecter à la Terre toutes les 5 secondes.
       Tant que la connexion réussit, il sait que le port 8080 est occupé. */
    while(1) {
        sock_test = socket(AF_INET, SOCK_STREAM, 0);
        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT_TERRE); // Cible : 8080
        inet_pton(AF_INET, ADRESSE_TERRE, &addr.sin_addr);

        if (connect(sock_test, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            // ÉCHEC DE CONNEXION : La Terre est éteinte ou crashée.
            printf("\n[ALERTE] Station Terre injoignable sur le port %d !\n", PORT_TERRE);
            printf("[MUTATION] Le Rover Alpha prend le contrôle du port 8080...\n");
            close(sock_test);
            break; // On sort de la veille pour devenir le serveur
        }
        
        printf("[ALPHA] Check-up Terre : OK. En veille...\r");
        fflush(stdout);
        close(sock_test);
        sleep(5); 
    }

    // --- ÉTAPE 2 : MUTATION EN SERVEUR DE SECOURS (Usurpation du Port 8080) ---
    /* Alpha ouvre maintenant son propre service d'écoute sur le port 8080.
       C'est transparent pour les autres rovers qui cherchent toujours ce port. */
    srv_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT_TERRE); // ALPHA DEVIENT LE PORT 8080

    if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("[ERREUR] Impossible de lier le port 8080. Conflit détecté.");
        exit(EXIT_FAILURE);
    }

    listen(srv_fd, 5);
    printf("[ALPHA] Mode RELAIS actif. Je remplace la Terre sur le port %d.\n", PORT_TERRE);

    // --- ÉTAPE 3 : GESTION DES ROVERS RESCAPÉS ---
    while (1) {
        int new_sock = accept(srv_fd, NULL, NULL);
        if (new_sock < 0) continue;

        Paquet p;
        if (read(new_sock, &p, sizeof(Paquet)) > 0) {
            printf("[ALPHA] Secours du Rover %d (Position: %d,%d)\n", p.id_envoyeur, p.x, p.y);
            
            // Ordre de survie : Alpha demande aux rovers de ne plus bouger.
            p.type = MODE_SECOURS; 
            strcpy(p.corps, "Ici Alpha (Relais). Terre KO. RECHARGEZ et attendez.");
            
            journaliser_alpha(p.id_envoyeur, "ORDRE_SURVIE_ENVOYE");
            send(new_sock, &p, sizeof(Paquet), 0);
        }
        close(new_sock);
    }

    close(srv_fd);
    return 0;
}
