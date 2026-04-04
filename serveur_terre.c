#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>       // Pour close(), read(), write()
#include <arpa/inet.h>    // Pour les structures réseau sockaddr_in
#include <time.h>         // Pour la gestion du temps (logs)
#include "protocol.h"     // Définitions communes (Paquet, ServeurTerre, etc.)

/**
 * Calcule la distance de Manhattan.
 * Indispensable pour la règle : 1 case = 1% de batterie.
 */
int calculer_distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

/**
 * Journalise les événements.
 * 'fflush' garantit que même en cas de crash, la ligne est écrite dans le fichier.
 */
void journaliser_serveur(int id_rover, int x, int y, const char* action) {
    FILE *f = fopen("serveur_terre.log", "a");
    if (f == NULL) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] [ROVER %02d] AT(%3d,%3d) | ACTION: %s\n", 
            s_now, id_rover, x, y, action);

    fflush(f); 
    fclose(f);
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    int opt = 1;

    srand(time(NULL));

    // 1. INITIALISATION DU SERVEUR
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT_TERRE);

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 5);

    // 2. INITIALISATION DE LA "MÉMOIRE" DU SERVEUR
    // La structure ServeurTerre permet de garder une trace des rovers et trésors
    // tant que le programme est allumé.
    ServeurTerre ma_terre;
    ma_terre.nb_rovers = 0;
    ma_terre.nb_tresors_trouves = 0;

    printf("[STATION TERRE] Centre de contrôle intelligent actif sur le port %d...\n", PORT_TERRE);

    while (1) {
        // 3. ACCEPTATION D'UNE CONNEXION
        new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen);
        if (new_socket < 0) continue;

        Paquet p;
        // 4. RÉCEPTION DE L'ÉTAT DU ROVER
        if (read(new_socket, &p, sizeof(Paquet)) > 0) {
            
            printf("[CONNEXION] Rover %d détecté. Batterie: %d%% | POS: (%d,%d)\n", 
                   p.id_envoyeur, p.batterie, p.x, p.y);
                
            // --- MISE À JOUR DE LA CARTE DES ROVERS ---
            // On cherche si le rover est déjà connu dans notre tableau 'flotte'
            int index = -1;
            for(int i=0; i < ma_terre.nb_rovers; i++) {
                if (ma_terre.flotte[i].id_robot == p.id_envoyeur) index = i;
            }
            
            // Si c'est un nouveau rover, on l'ajoute à la flotte
            if (index == -1 && ma_terre.nb_rovers < MAX_ROVERS) {
                index = ma_terre.nb_rovers++;
                ma_terre.flotte[index].id_robot = p.id_envoyeur;
            }
            
            // Mise à jour de ses coordonnées en mémoire vive
            if (index != -1) {
                ma_terre.flotte[index].x = p.x;
                ma_terre.flotte[index].y = p.y;
                ma_terre.flotte[index].batterie = p.batterie;
            }

            // --- PLANIFICATION DE LA MISSION ---
            int cible_x = p.x + (rand() % 10 + 1); 
            int cible_y = p.y + (rand() % 10 + 1);
            int distance = calculer_distance(p.x, p.y, cible_x, cible_y);
            int batterie_apres_trajet = p.batterie - distance;

            // --- SÉCURITÉ ÉNERGÉTIQUE PRÉDICTIVE ---
            if (p.batterie < SEUIL_BATTERIE) {
                p.type = ORDRE_RECHARGER;
                strcpy(p.corps, "ALERTE : Batterie faible. Rechargez maintenant !");
                journaliser_serveur(p.id_envoyeur, p.x, p.y, "ORDRE_RECHARGE_IMMEDIAT");
            } 
            else if (batterie_apres_trajet < SEUIL_BATTERIE) {
                p.type = ORDRE_RECHARGER;
                strcpy(p.corps, "RECHARGE PRÉVENTIVE : Trajet trop long.");
                journaliser_serveur(p.id_envoyeur, p.x, p.y, "RECHARGE_PREVENTIVE");
            }
            else {
                p.type = ORDRE_DEPLACER;
                p.x = cible_x; p.y = cible_y;
                strcpy(p.corps, "Destination validée. Mission en cours.");
                journaliser_serveur(p.id_envoyeur, p.x, p.y, "MISSION_LANCEE");
            }

            // 5. ENVOI DE L'ORDRE
            // Le serveur répond AVANT de passer en mode écoute des trésors
            send(new_socket, &p, sizeof(Paquet), 0);

            // 6. SURVEILLANCE ASYNCHRONE DES TRÉSORS
            // La connexion reste ouverte : le serveur écoute les alertes
            // envoyées par le rover pendant son déplacement.
            while (read(new_socket, &p, sizeof(Paquet)) > 0) {
                if (p.type == ALERTE_TRESOR) {
                    // --- MISE À JOUR DE LA CARTE DES TRÉSORS ---
                    if (ma_terre.nb_tresors_trouves < MAX_TRESORS) {
                        int t = ma_terre.nb_tresors_trouves++;
                        ma_terre.carte_tresors[t].x = p.x;
                        ma_terre.carte_tresors[t].y = p.y;
                        ma_terre.carte_tresors[t].id_rover = p.id_envoyeur;
                        
                        journaliser_serveur(p.id_envoyeur, p.x, p.y, "TRESOR_ENREGISTRE");
                        printf("[CARTE] Trésor n°%d trouvé par R%d en (%d,%d)\n", 
                                ma_terre.nb_tresors_trouves, p.id_envoyeur, p.x, p.y);
                    }
                }
            }
        }

        // 7. FIN DE SESSION
        close(new_socket);
        printf("[STATION] Rover déconnecté. En attente...\n--------------------------\n");
    }

    close(server_fd);
    return 0;
}
