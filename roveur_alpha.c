#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>      // Fournit access(), sleep(), close(), read(), unlink()
#include <arpa/inet.h>    // Structures réseau (sockaddr_in) et fonctions (socket, connect)
#include <errno.h>        // Gestion des erreurs système
#include <time.h>
#include "protocol.h"     // Contrat commun (PORT_TERRE, Paquet, etc.)

/**
 * FONCTION : test_connexion_terre
 * -------------------------------
 * Agit comme une sonde réseau pour vérifier si le serveur principal est en ligne.
 */
int test_connexion_terre() {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return 0;

    struct sockaddr_in serv_addr;
    struct timeval timeout;
    timeout.tv_sec = 0;
    timeout.tv_usec = 500000; // Timeout de 500ms pour ne pas bloquer Alpha
    setsockopt(sock, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ADRESSE_TERRE);
    serv_addr.sin_port = htons(PORT_TERRE);

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
        close(sock);
        return 1; // La Terre répond
    }
    
    close(sock);
    return 0; // La Terre est injoignable
}

/**
 * FONCTION : journaliser_serveur
 * ------------------------------
 * Réalisée par Moise MUSHIMIYIMANA. 
 * Archive les événements même quand Alpha est en mode Relais.
 */
void journaliser_alpha(int id_rover, int pos_x, int pos_y, const char* action) {
    FILE *fichier_log = fopen("serveur_alpha.log", "a");
    if (fichier_log == NULL) return;

    time_t secondes = time(NULL);
    struct tm *info_temps = localtime(&secondes);
    char horodatage[25];
    strftime(horodatage, sizeof(horodatage), "%Y-%m-%d %H:%M:%S", info_temps);

    fprintf(fichier_log, "[%s] [RELAIS ALPHA] [ROVER %02d] (%3d,%3d) -> %s\n", 
            horodatage, id_rover, pos_x, pos_y, action);
    fclose(fichier_log);
}

// --- Fonctions de gestion de la flotte (Similaire au serveur Terre) ---
void sauvegarder_positions(ServeurTerre *t) { 
    FILE *f = fopen("positions_rovers.txt", "w"); 
    if (f == NULL) return;
    for (int i = 0; i < t->nb_rovers; i++) { 
        fprintf(f, "%d %d %d %d %d\n", t->flotte[i].id_robot, t->flotte[i].pos_x, 
                t->flotte[i].pos_y, t->flotte[i].niveau_batterie, t->flotte[i].est_alpha);
    }
    fclose(f);
}

void charger_positions(ServeurTerre *t) {
    FILE *f = fopen("positions_rovers.txt", "r");
    if (f == NULL) return;
    t->nb_rovers = 0;
    while (fscanf(f, "%d %d %d %d %d", &t->flotte[t->nb_rovers].id_robot, 
                  &t->flotte[t->nb_rovers].pos_x, &t->flotte[t->nb_rovers].pos_y, 
                  &t->flotte[t->nb_rovers].niveau_batterie, &t->flotte[t->nb_rovers].est_alpha) == 5) {
        t->flotte[t->nb_rovers].est_occupe = 0;
        t->nb_rovers++;
    }
    fclose(f);
}

int calculer_distance(int x1, int y1, int x2, int y2) { return abs(x1 - x2) + abs(y1 - y2); }

int peut_accomplir_mission(int x, int y, int cx, int cy, int bat) {
    return (bat - calculer_distance(x, y, cx, cy) >= SEUIL_BATTERIE);
}

/**
 * SYNCHRONISATION (Moise MUSHIMIYIMANA) : Verrouillage de l'ID en mode Relais.
 */
void synchroniser_rover_relais(ServeurTerre *t, Paquet *p) {
    int indice = -1;
    for (int i = 0; i < t->nb_rovers; i++) {
        if (t->flotte[i].id_robot == p->id_envoyeur) { indice = i; break; }
    }

    if (indice != -1 && t->flotte[indice].est_occupe == 1) {
        p->type = ERREUR;
        sprintf(p->corps, "[RELAIS] ID %d deja actif sur le relais.", p->id_envoyeur);
        return; 
    }

    if (indice == -1 && t->nb_rovers < MAX_ROVERS) {
        indice = t->nb_rovers++;
        t->flotte[indice].id_robot = p->id_envoyeur;
        t->flotte[indice].pos_x = 0; t->flotte[indice].pos_y = 0;
        t->flotte[indice].niveau_batterie = 100;
    }

    if (indice != -1) {
        t->flotte[indice].est_occupe = 1;
        p->x = t->flotte[indice].pos_x; p->y = t->flotte[indice].pos_y;
        p->batterie = t->flotte[indice].niveau_batterie;
        p->type = STATUS_QUO; 
        sprintf(p->corps, "[RELAIS ALPHA] Session etablie pour R%d.", p->id_envoyeur);
    }
}

int main() {
    int srv_fd, new_sock;
    struct sockaddr_in addr;
    int opt = 1;
    ServeurTerre ma_terre;
    ma_terre.nb_rovers = 0;

    srand(time(NULL));
    charger_positions(&ma_terre);

    while (1) { 
        // --- PHASE 1 : VEILLE ---
        printf("[ALPHA] Mode Veille : Surveillance du serveur principal...\n");
        while (test_connexion_terre()) {
            sleep(3);
        }

        // --- PHASE 2 : MUTATION EN RELAIS ---
        printf("[ALERTE] Terre injoignable. Alpha prend le relais sur le port %d...\n", PORT_TERRE);
        srv_fd = socket(AF_INET, SOCK_STREAM, 0);
        setsockopt(srv_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // Timeout pour que le accept() ne bloque pas indéfiniment
        struct timeval tv = {2, 0}; 
        setsockopt(srv_fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

        addr.sin_family = AF_INET;
        addr.sin_port = htons(PORT_TERRE);
        addr.sin_addr.s_addr = INADDR_ANY;

        if (bind(srv_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            close(srv_fd); sleep(2); continue;
        }

        listen(srv_fd, 5);
        int terre_de_retour = 0;

        while (!terre_de_retour) {
            new_sock = accept(srv_fd, NULL, NULL);
            
            if (new_sock >= 0) {
                Paquet p;
                if (read(new_sock, &p, sizeof(Paquet)) > 0) {
                    
                    if (p.type == DEM_INIT) {
                        synchroniser_rover_relais(&ma_terre, &p);
                        send(new_sock, &p, sizeof(Paquet), 0);
                        if (p.type == ERREUR) { close(new_sock); continue; }
                    }
                    else if (p.type == STATUS_QUO) {
                        int cx = p.x + (rand() % 5 + 1); int cy = p.y + (rand() % 5 + 1);
                        if (peut_accomplir_mission(p.x, p.y, cx, cy, p.batterie)) {
                            p.x = cx; p.y = cy; p.type = ORDRE_DEPLACER;
                        } else { p.type = ORDRE_RECHARGER; }
                        send(new_sock, &p, sizeof(Paquet), 0);
                    }
                    else if (p.type == ALERTE_TRESOR) {
                        journaliser_alpha(p.id_envoyeur, p.x, p.y, "TRESOR_RELAIS");
                    }

                    // Suivi du mouvement
                    while (read(new_sock, &p, sizeof(Paquet)) > 0) {
                        for (int i = 0; i < ma_terre.nb_rovers; i++) {
                            if (ma_terre.flotte[i].id_robot == p.id_envoyeur) {
                                ma_terre.flotte[i].pos_x = p.x;
                                ma_terre.flotte[i].pos_y = p.y;
                                ma_terre.flotte[i].niveau_batterie = p.batterie;
                                break;
                            }
                        }
                    }
                }
                // Libération de l'ID après déconnexion
                for (int i = 0; i < ma_terre.nb_rovers; i++) {
                    if (ma_terre.flotte[i].id_robot == p.id_envoyeur) ma_terre.flotte[i].est_occupe = 0;
                }
                close(new_sock);
                printf("[RELAIS] Session terminee pour Rover %d.\n", p.id_envoyeur);
            }

            // Vérifier si la Terre est revenue via le fichier témoin
            if (access("terre_ready", F_OK) != -1) {
                printf("[ALPHA] La Terre est de retour. Fermeture du relais...\n");
                terre_de_retour = 1;
            }
        }
        close(srv_fd);
        unlink("terre_ready");
        sleep(5); // Sécurité TCP
    }
    return 0;
}