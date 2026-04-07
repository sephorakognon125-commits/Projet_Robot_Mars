#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include <stdbool.h>
#include "protocol.h"

/**
 * FONCTION : enregistrer_passage (Journal local)
 * realisée par Moise MUSHIMIYIMANA pour centraliser la journalisation locale.
 */
void journal_roveur(int id_rover, int pos_x, int pos_y, int niveau_bat, char* statut) { 
    char nom_fichier[32];
    sprintf(nom_fichier, "rover_%d.log", id_rover);
    FILE *f = fopen(nom_fichier, "a");
    if (f == NULL) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] POS(%3d,%3d) | BAT(%3d%%) | STATUT: %s\n", s_now, pos_x, pos_y, niveau_bat, statut);
    fclose(f);
}

/**
 * FONCTION : gerer_recharge
 * Centralise la logique de recharge solaire.
 */
void gerer_recharge(int id_rover, int pos_x, int pos_y, int *niveau_bat, const char* raison) {
    printf("\n[ACTION] -> Activation de la mise en veille : %s\n", raison);
    journal_roveur(id_rover, pos_x, pos_y, *niveau_bat, "DEBUT_RECHARGE");

    while (*niveau_bat < 100) {
        sleep(1); // Simule le temps de charge martien
        (*niveau_bat)++;
        if (*niveau_bat % 20 == 0) printf("  [CHARGEMENT] Batterie : %d%%...\n", *niveau_bat);
    }

    printf("[OK] -> Batterie pleine. Fin de la veille.\n");
    journal_roveur(id_rover, pos_x, pos_y, *niveau_bat, "FIN_RECHARGE");
}

/**
 * realisée par Moise MUSHIMIYIMANA pour éviter les coordonnées aléatoires.
 * Bloque le rover tant que la synchronisation initiale n'est pas faite.
 */
void initialisation(int id_rover, int *pos_x, int *pos_y, int *niveau_bat, bool *est_alpha) {
    int sock; // Socket pour la communication avec le serveur Terre
    struct sockaddr_in serv_addr; // Adresse du serveur Terre
    int synchronisation = 0; // Etat de la synchronisation

    printf("[SYSTÈME] -> Rover %d Lancement : Phase de synchronisation initiale...\n", id_rover);

    while (!synchronisation) {
        // Création de la socket et configuration de l'adresse du serveur Terre et ecoute du port de controle PORt_TERRE
        sock = socket(AF_INET, SOCK_STREAM, 0); 
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT_TERRE);
        inet_pton(AF_INET, ADRESSE_TERRE, &serv_addr.sin_addr); 

        // Tentative de connexion au serveur Terre pour la synchronisation initiale
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) { // Connexion réussie

            // Preparation du paquet de synchronisation initiale avec l'ID du rover et une indication s'il s'agit d'Alpha ou non,
            // pour permettre au serveur de gérer les rôles spécifiques dès le départ.
            Paquet p_init;
            memset(&p_init, 0, sizeof(Paquet)); // Initialisation à zéro pour éviter les données résiduelles

            p_init.id_envoyeur = id_rover;
            p_init.type = DEM_INIT; // Type de message spécifique pour la synchronisation initiale
            strcpy(p_init.corps, *est_alpha ? "Alpha" : "Non-Alpha");

            send(sock, &p_init, sizeof(Paquet), 0); // Envoi du paquet de synchronisation initiale

            // Attente de la réponse du serveur Terre avec les coordonnées exactes et le niveau de batterie initial,
            // pour éviter les coordonnées aléatoires et permettre une gestion réaliste de l'énergie dès le départ.
            if (read(sock, &p_init, sizeof(Paquet)) > 0) {
                // Vérification si le serveur a renvoyé une erreur (ex: ID déjà utilisé)
                if (p_init.type == ERREUR) {
                    printf("[STOP] -> %s\n", p_init.corps);
                    exit(EXIT_FAILURE);
                }
                *pos_x = p_init.x; 
                *pos_y = p_init.y; 
                *niveau_bat = p_init.batterie;
                synchronisation = 1;

                printf("[SYNCHRO] -> Synchronisation réussie avec le serveur.\n");
                printf("[OK] -> Position : (%d,%d) | Batterie : %d%%\n", *pos_x, *pos_y, *niveau_bat);
            }
            close(sock); // Fermeture de la socket après la synchronisation pour liberer les ressources
        } else {
            printf("[RADIO] -> Station Terre injoignable | Nouvelle tentative dans 3s...\n");
            close(sock); // Liberation des ressources
            sleep(3); // Attente de 3 secondes
        }
    }
}

/**
 * FONCTION : executer_deplacement
 * Gère le mouvement pas à pas et la consommation d'énergie.
 */
void executer_deplacement(int *cur_x, int *cur_y, int cible_x, int cible_y, int *niveau_bat, int id_du_rover, int sock) {
    Paquet p_upd;
    printf("\n[ACTION] -> En route vers (%d, %d)\n", cible_x, cible_y);

    while ((*cur_x != cible_x || *cur_y != cible_y) && *niveau_bat > 0) {
        if (*cur_x != cible_x) (*cur_x < cible_x) ? (*cur_x)++ : (*cur_x)--;
        else (*cur_y < cible_y) ? (*cur_y)++ : (*cur_y)--;

        (*niveau_bat)--;
        char* statut = "VIDE";

        if ((rand() % CHANCE_TRESOR) == 0) {
            statut = "TRESOR";
            printf("[DÉCOUVERTE] Trésor détecté en (%d, %d)!\n", *cur_x, *cur_y);
            memset(&p_upd, 0, sizeof(Paquet));
            p_upd.id_envoyeur = id_du_rover;
            p_upd.type = ALERTE_TRESOR;
            p_upd.x = *cur_x; p_upd.y = *cur_y; p_upd.batterie = *niveau_bat;
            send(sock, &p_upd, sizeof(Paquet), 0);
        }

        journal_roveur(id_du_rover, *cur_x, *cur_y, *niveau_bat, statut);
        printf("  > LOCALISATION (%d,%d) | ÉNERGIE : %d%%\n", *cur_x, *cur_y, *niveau_bat);
        sleep(1);
    }
}

// Réalisée par Moise MUSHIMIYIMANA pour centraliser la logique de validation de mission.
int afficher_menu() {
    int choix = 0;
    
    while (choix < 1 || choix > 5) {
        printf("\n--- CONTRÔLE MISSION MARS ---\n");
        printf("1. Demander une nouvelle mission (Déplacement)\n");
        printf("2. Trouver le rover le plus proche\n");
        printf("3. Envoyer les logs au serveur\n");
        printf("4. Mode Recharge solaire (Forcer)\n");
        printf("5. Quitter le programme\n");
        printf("Choix : ");

        if (scanf("%d", &choix) != 1) {
            printf("[ERREUR] -> Entrée invalide : Veuillez entrer un chiffre.\n");
            while(getchar() != '\n'); 
            choix = 0;
        } else if (choix < 1 || choix > 5) {
            printf("[ERREUR] -> Veuillez choisir entre 1 et 5.\n");
        }
    }
    return choix;
}

// Réalisée par Moise MUSHIMIYIMANA pour centraliser la gestion du rover.
int main(int argc, char *argv[]) { 

    srand(time(NULL));

    // Récupération de l'ID du rover depuis les arguments, par défaut 2.
    int id_rover = (argc > 1) ? atoi(argv[1]) : 2; 

    printf("[SYSTÈME] -> Démarrage du Rover %d...\n", id_rover);

    // Validation de l'ID pour éviter les conflits serveurs (0 et 1)
    if (id_rover < 2) {
        printf("[ERREUR] -> ID %d invalide ou réservé (0 et 1 réservés au contrôle).\n", id_rover);
        exit(EXIT_FAILURE);
    }

    int x, y, bat; bool est_alpha = false; 

    initialisation(id_rover, &x, &y, &bat, &est_alpha);
    printf("[SYSTÈME] -> Rover %d prêt pour la mission !\n", id_rover);

    while (bat > 0) {
        if (bat < SEUIL_BATTERIE) {
            printf("[ALERTE] -> Rover %d : Niveau de batterie critique (%d%%)!\n", id_rover, bat);
            gerer_recharge(id_rover, x, y, &bat, "Niveau critique");
        }
        
        int choix = afficher_menu();
        if (choix == 5) break; // Quitter

        int sock = socket(AF_INET, SOCK_STREAM, 0);
        struct sockaddr_in serv_addr;
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT_TERRE);
        inet_pton(AF_INET, ADRESSE_TERRE, &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) == 0) {
            Paquet p;
            memset(&p, 0, sizeof(Paquet));
            p.id_envoyeur = id_rover; p.x = x; p.y = y; p.batterie = bat;

            switch (choix) {
                case 1: p.type = STATUS_QUO; break;      
                case 2: p.type = REQ_IDENTITE; break;    
                case 3: p.type = ENVOI_LOGS; break;      
                case 4: p.type = ORDRE_RECHARGER; break; 
            }

            send(sock, &p, sizeof(Paquet), 0);
            
            if (read(sock, &p, sizeof(Paquet)) > 0) {
                if (p.type == ORDRE_RECHARGER) {
                    printf("[RADIO] -> %s\n", p.corps);
                    gerer_recharge(id_rover, x, y, &bat, "Ordre Serveur");
                } 
                else if (p.type == REQ_IDENTITE) {
                    printf("[RADIO] -> Voisin le plus proche : %s\n", p.corps);
                    journal_roveur(id_rover, x, y, bat, p.corps);
                } 
                else if (p.type == ENVOI_LOGS) {
                    printf("[RADIO] -> %s\n", p.corps);
                } 
                else if (p.type == ORDRE_DEPLACER) {
                    printf("[RADIO] -> Mission reçue : Aller vers (%d, %d)\n", p.x, p.y);
                    executer_deplacement(&x, &y, p.x, p.y, &bat, id_rover, sock);
                }
            }
            close(sock);
        } else {
            printf("[ERREUR] -> Connexion perdue avec le serveur.\n");
        }
    }

    printf("[FIN] -> Mission terminée pour le Rover %d.\n", id_rover);
    return 0;
}