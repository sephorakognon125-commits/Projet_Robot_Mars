#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include "protocol.h"

/**
 * Gère l'historique centralisé du serveur Terre.
 * Chaque action est tracée avec un mot-clé unique pour faciliter l'analyse post-mission.
 */
void journaliser_serveur(int id_rover, int x, int y, const char* action) {
    FILE *f = fopen("serveur_terre.log", "a");
    if (f == NULL) {
        perror("Erreur : Impossible d'écrire dans le log serveur");
        return;
    }

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);

    // [Journalisation Précise] : Formatage aligné pour une lecture claire
    fprintf(f, "[%s] [ROVER %02d] AT(%3d,%3d) | ACTION: %s\n", 
            s_now, id_rover, x, y, action);

    fflush(f); // Sécurité : force l'écriture disque
    fclose(f);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    srand(time(NULL));

    // 1. Initialisation du Socket TCP
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Échec socket");
        exit(EXIT_FAILURE);
    }

    // Autorise la réutilisation rapide du port en cas de redémarrage
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_TERRE);

    // 2. Liaison et mise en écoute
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Échec bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, 5) < 0) {
        perror("Échec listen");
        exit(EXIT_FAILURE);
    }

    printf("[STATION TERRE] Centre de contrôle actif sur le port %d...\n", PORT_TERRE);

    while (1) {
        // 3. Attente d'un Rover (Connexion)
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        Paquet p;
        // 4. Lecture de l'état initial envoyé par le Rover
        if (read(new_socket, &p, sizeof(Paquet)) > 0) {
            
            printf("[CONNEXION] Rover %d détecté. Batterie: %d%% | Position: (%d,%d)\n", 
                   p.id_envoyeur, p.batterie, p.x, p.y);

            // --- [SÉCURITÉ ÉNERGÉTIQUE] ---
            // Si la batterie est < SEUIL_BATTERIE (20%), on refuse le mouvement
            if (p.batterie < SEUIL_BATTERIE) {
                p.type = ORDRE_RECHARGER;
                strcpy(p.corps, "ALERTE : Énergie critique. Mode recharge activé.");
                
                // [Journalisation Précise]
                journaliser_serveur(p.id_envoyeur, p.x, p.y, "ORDRE_RECHARGE_FORCE");
                
                printf("[STATION] Batterie faible pour R%d. Ordre de recharge envoyé.\n", p.id_envoyeur);
            } 
            else {
                // Sinon, on calcule une nouvelle destination
                p.type = ORDRE_DEPLACER;
                p.x += (rand() % 10 + 1); 
                p.y += (rand() % 10 + 1);
                strcpy(p.corps, "Destination validée. En route !");
                
                // [Journalisation Précise]
                journaliser_serveur(p.id_envoyeur, p.x, p.y, "MISSION_DEPLACEMENT_VALIDE");
                
                printf("[STATION] Mission envoyée vers (%d,%d) pour R%d.\n", p.x, p.y, p.id_envoyeur);
            }

            // Envoi de l'ordre au Rover
            send(new_socket, &p, sizeof(Paquet), 0);

            // --- [SURVEILLANCE ASYNCHRONE] ---
            /* On garde la connexion ouverte tant que le Rover se déplace.
               Cela permet au serveur de recevoir les messages ALERTE_TRESOR 
               envoyés par le Rover "en plein vol" (sans attendre un nouveau cycle).
            */
            printf("[INFO] Surveillance en temps réel du Rover %d active...\n", p.id_envoyeur);
            
            while (read(new_socket, &p, sizeof(Paquet)) > 0) {
                if (p.type == ALERTE_TRESOR) {
                    // [Journalisation Précise]
                    journaliser_serveur(p.id_envoyeur, p.x, p.y, "TRESOR_CONFIRME");
                    
                    printf("[URGENT] Trésor localisé par R%d aux coordonnées (%d,%d) !\n", 
                            p.id_envoyeur, p.x, p.y);
                }
                // Ici, on pourrait ajouter d'autres types d'alertes en temps réel
            }
        }

        // 5. Fermeture de la session
        close(new_socket);
        printf("[STATION] Rover déconnecté. Session archivée.\n--------------------------\n");
    }

    close(server_fd);
    return 0;
}