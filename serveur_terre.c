#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <stdbool.h>
#include "protocol.h"

/**
 * FONCTION : journaliser_serveur
 * ------------------------------
 * Réalisée par Moise MUSHIMIYIMANA pour l'historique de la Station Terre.
 * Enregistre les actions dans "serveur_terre.log".
 */
void journaliser_serveur(int id_rover, int pos_x, int pos_y, const char* action) {
    // Ouverture en mode "a" (Append) pour ajouter à la fin du fichier
    FILE *fichier_log = fopen("serveur_terre.log", "a");
    if (fichier_log == NULL) return;

    // Récupération de l'heure actuelle
    time_t secondes = time(NULL);
    struct tm *info_temps = localtime(&secondes);
    char horodatage[25];
    strftime(horodatage, sizeof(horodatage), "%Y-%m-%d %H:%M:%S", info_temps);

    // Écriture du log formaté
    fprintf(fichier_log, "[%s] [ROVER %02d] (%3d,%3d) -> ACTION: %s\n", 
            horodatage, id_rover, pos_x, pos_y, action);

    fflush(fichier_log); // Force l'écriture immédiate sur le disque
    fclose(fichier_log);
}

// --- Fonctions de sauvegarde simulant la BD ---
// Réalisées par Moise MUSHIMIYIMANA pour permettre la persistance des données
void sauvegarder_positions(ServeurTerre *t) { 
    FILE *f = fopen("positions_rovers.txt", "w"); 
    if (f == NULL) return;
    for (int i = 0; i < t->nb_rovers; i++) { 
        fprintf(f, "%d %d %d %d %d\n", 
                t->flotte[i].id_robot, 
                t->flotte[i].pos_x, 
                t->flotte[i].pos_y, 
                t->flotte[i].niveau_batterie, 
                t->flotte[i].est_alpha);
    }
    fclose(f);
}

// --- Fonction de chargement des positions ---
void charger_positions(ServeurTerre *t) {
    FILE *f = fopen("positions_rovers.txt", "r");
    if (f == NULL) return;
    t->nb_rovers = 0;

    while (fscanf(f, "%d %d %d %d %d", 
                  &t->flotte[t->nb_rovers].id_robot, 
                  &t->flotte[t->nb_rovers].pos_x, 
                  &t->flotte[t->nb_rovers].pos_y, 
                  &t->flotte[t->nb_rovers].niveau_batterie, 
                  &t->flotte[t->nb_rovers].est_alpha) == 5) 
    {
        t->flotte[t->nb_rovers].est_occupe = 0; // Au démarrage, personne n'est connecté
        t->nb_rovers++;
        if (t->nb_rovers >= MAX_ROVERS) break;
    }
    fclose(f);
    printf("[SYSTÈME] -> %d rovers restaurés depuis la base de données.\n", t->nb_rovers);
}

// --- Recherche du voisin le plus proche ---
void trouver_voisin_proche(ServeurTerre *t, Paquet *p) {
    int id_proche = -1;
    int dist_min = 10000;

    for (int i = 0; i < t->nb_rovers; i++) {
        if (t->flotte[i].id_robot != p->id_envoyeur) {
            int d = abs(p->x - t->flotte[i].pos_x) + abs(p->y - t->flotte[i].pos_y);
            if (d < dist_min) {
                dist_min = d;
                id_proche = t->flotte[i].id_robot;
            }
        }
    }

    if (id_proche != -1) {
        sprintf(p->corps, "[IDENTITE] -> Voisin detecte : Rover %d a %d cases.", id_proche, dist_min);
    } else {
        strcpy(p->corps, "[IDENTITE] -> Signal radio isole : Aucun autre roveur detecte.");
    }
}

// --- Calcul de la distance de Manhattan ---
int calculer_distance(int x1, int y1, int x2, int y2) {
    return abs(x1 - x2) + abs(y1 - y2);
}

// --- Vérification de la connexion à la Terre ---
int peut_accomplir_mission(int x_actuel, int y_actuel, int x_cible, int y_cible, int batterie_actuelle) {
    int distance = calculer_distance(x_actuel, y_actuel, x_cible, y_cible);
    return (batterie_actuelle - distance >= SEUIL_BATTERIE);
}

/**
 * realisée par Moise MUSHIMIYIMANA : Synchronisation avec sécurité anti-doublon ID.
 */
void synchroniser_rover(ServeurTerre *t, Paquet *p) {
    int indice = -1;

    for (int i = 0; i < t->nb_rovers; i++) {
        if (t->flotte[i].id_robot == p->id_envoyeur) {
            indice = i;
            break;
        }
    }

    // --- SÉCURITÉ : VERIFICATION DE DOUBLON ---
    if (indice != -1 && t->flotte[indice].est_occupe == 1) {
        p->type = ERREUR;
        sprintf(p->corps, "[ERREUR] -> Le Roveur %d est déjà en mission sur Mars !", p->id_envoyeur);
        printf("[ALERTE] -> Tentative de usurpation d'ID : Rover %d.\n", p->id_envoyeur);
        return; 
    }

    // ÉTAPE B : Création si nouveau
    if (indice == -1 && t->nb_rovers < MAX_ROVERS) {
        indice = t->nb_rovers++;
        t->flotte[indice].id_robot = p->id_envoyeur;
        t->flotte[indice].pos_x = 0; 
        t->flotte[indice].pos_y = 0;
        t->flotte[indice].niveau_batterie = 100;
        t->flotte[indice].est_alpha = (strcmp(p->corps, "Alpha") == 0) ? 1 : 0;
        t->flotte[indice].est_occupe = 0; 
        printf("[INFO] -> Nouveau Rover detecte : R %d.\n", p->id_envoyeur);
    }

    // ÉTAPE C : Validation de la connexion
    if (indice != -1) {
        t->flotte[indice].est_occupe = 1; 
        p->x = t->flotte[indice].pos_x;
        p->y = t->flotte[indice].pos_y;
        p->batterie = t->flotte[indice].niveau_batterie;
        p->type = STATUS_QUO; 
        sprintf(p->corps, "[MESSAGE] -> Bienvenue Roveur %d ! Liaison etablie.", p->id_envoyeur);
        printf("[INFO] -> Rover %d est maintenant CONNECTE.\n", p->id_envoyeur);
    }
}

int main() {
    int serveur_fd, socket_client;
    struct sockaddr_in adresse;
    int taille_adr = sizeof(adresse);
    int opt = 1;

    printf("[STATION TERRE] -> Initialisation du serveur de contrôle...\n");
    srand(time(NULL)); 

    serveur_fd = socket(AF_INET, SOCK_STREAM, 0); 
    setsockopt(serveur_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)); 

    adresse.sin_family = AF_INET; 
    adresse.sin_addr.s_addr = INADDR_ANY; 
    adresse.sin_port = htons(PORT_TERRE); 

    bind(serveur_fd, (struct sockaddr *)&adresse, sizeof(adresse)); 
    listen(serveur_fd, 5); 

    ServeurTerre ma_terre;
    ma_terre.nb_rovers = 0;
    charger_positions(&ma_terre);

    printf("[STATION TERRE] -> Serveur en ligne sur le port %d.\n", PORT_TERRE);

    while (1) {
        socket_client = accept(serveur_fd, (struct sockaddr *)&adresse, (socklen_t*)&taille_adr);
        if (socket_client < 0) continue; 

        Paquet p;
        if (read(socket_client, &p, sizeof(Paquet)) > 0) {
            
            // 1. GESTION DE LA SYNCHRONISATION
            if (p.type == DEM_INIT) {
                synchroniser_rover(&ma_terre, &p);
                send(socket_client, &p, sizeof(Paquet), 0);
                if (p.type == ERREUR) {
                    close(socket_client);
                    continue; 
                }
                sauvegarder_positions(&ma_terre);
            }
            
            // 2. GESTION DES COMMANDES (MISSION / IDENTITÉ / LOGS)
            else if (p.type == STATUS_QUO) {
                int cible_x = p.x + (rand() % 10 + 1); 
                int cible_y = p.y + (rand() % 10 + 1);
                
                if (peut_accomplir_mission(p.x, p.y, cible_x, cible_y, p.batterie)) {
                    p.x = cible_x; p.y = cible_y; p.type = ORDRE_DEPLACER;
                    strcpy(p.corps, "[VALIDATION] -> Mission validee.");
                } else {
                    p.type = ORDRE_RECHARGER;
                    strcpy(p.corps, "[ANNULATION] -> Cible trop loin.");
                }
                send(socket_client, &p, sizeof(Paquet), 0);
            }
            else if (p.type == REQ_IDENTITE) {
                trouver_voisin_proche(&ma_terre, &p);
                send(socket_client, &p, sizeof(Paquet), 0);
            }
            else if ((p.type == ENVOI_LOGS) || (p.type == ALERTE_TRESOR)) {
                journaliser_serveur(p.id_envoyeur, p.x, p.y, p.corps);
                strcpy(p.corps, "[STATION] -> Information enregistree.");
                send(socket_client, &p, sizeof(Paquet), 0);
            }

            // 3. BOUCLE DE SUIVI UNIQUE (Maintient l'ID occupé pendant le trajet)
            while (read(socket_client, &p, sizeof(Paquet)) > 0) {
                for (int i = 0; i < ma_terre.nb_rovers; i++) {
                    if (ma_terre.flotte[i].id_robot == p.id_envoyeur) {
                        ma_terre.flotte[i].pos_x = p.x;
                        ma_terre.flotte[i].pos_y = p.y;
                        ma_terre.flotte[i].niveau_batterie = p.batterie;
                        ma_terre.flotte[i].est_occupe = 1; 
                        break;
                    }
                }
                if (p.type == ALERTE_TRESOR) {
                    journaliser_serveur(p.id_envoyeur, p.x, p.y, "TRESOR_CONFIRME");
                }
                sauvegarder_positions(&ma_terre);
            }
        }

        // --- LIBÉRATION FINALE ---
        for (int i = 0; i < ma_terre.nb_rovers; i++) {
            if (ma_terre.flotte[i].id_robot == p.id_envoyeur) {
                ma_terre.flotte[i].est_occupe = 0; 
                printf("[INFO] -> Rover %d s'est deconnecte. ID libere.\n", p.id_envoyeur);
                break;
            }
        }
        close(socket_client);
    }
    return 0;
}