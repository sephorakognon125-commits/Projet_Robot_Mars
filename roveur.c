#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include "protocol.h"

/**
 * Interface de commande : permet à l'humain de piloter les intentions du Rover.
 */
int afficher_menu() {
    int choix;
    printf("\n--- CONTRÔLE MISSION MARS ---\n");
    printf("1. Demander une nouvelle mission (Déplacement)\n");
    printf("2. Trouver le rover le plus proche\n");
    printf("3. Envoyer les logs au serveur\n");
    printf("4. Mode Recharge solaire (Forcer)\n");
    printf("5. Quitter le programme\n");
    printf("Choix : ");
    
    // Sécurisation de la saisie
    if (scanf("%d", &choix) != 1) {
        while(getchar() != '\n'); // Nettoyer le buffer
        return 0;
    }
    return choix;
}

/**
 * Journalisation locale : chaque coordonnée et action est gravée dans le log.
 */
void enregistrer_passage(int id_rover, int x, int y, int bat, char* statut) {
    char nom_fichier[32];
    sprintf(nom_fichier, "rover_%d.log", id_rover);

    FILE *f = fopen(nom_fichier, "a");
    if (f == NULL) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t);

    fprintf(f, "[%s] POS(%d,%d) | BAT(%d%%) | STATUT: %s\n", s_now, x, y, bat, statut);
    fflush(f);
    fclose(f);
}

/**
 * Déplacement intelligent : Horizontal puis Vertical.
 * Détecte les trésors et informe le serveur en temps réel.
 */
void executer_deplacement(int *cur_x, int *cur_y, int cible_x, int cible_y, int *batterie, int id_du_rover, int sock) {
    Paquet alerte;
    printf("[MOUVEMENT] Direction cible : (%d, %d)\n", cible_x, cible_y);

    while ((*cur_x != cible_x || *cur_y != cible_y) && *batterie > 0) {
        // 1. Logique d'axe : Horizontal (X) d'abord, puis Vertical (Y)
        if (*cur_x != cible_x) {
            (*cur_x < cible_x) ? (*cur_x)++ : (*cur_x)--;
        } else {
            (*cur_y < cible_y) ? (*cur_y)++ : (*cur_y)--;
        }

        // 2. Consommation d'énergie (1% par case)
        (*batterie)--; 
        char* statut_actuel = "VIDE";

        // 3. Probabilité de trésor (ex: 1/10 défini dans protocol.h)
        if ((rand() % CHANCE_TRESOR) == 0) {
            statut_actuel = "TRESOR"; 
            printf("[DÉCOUVERTE] Rover %d a trouvé un trésor en (%d, %d)!\n", id_du_rover, *cur_x, *cur_y);
            
            alerte.id_envoyeur = id_du_rover;
            alerte.type = ALERTE_TRESOR;
            alerte.x = *cur_x;
            alerte.y = *cur_y;
            alerte.batterie = *batterie;
            strcpy(alerte.corps, "Trésor localisé !");
            send(sock, &alerte, sizeof(Paquet), 0); // Envoi immédiat à la Terre
        }

        // 4. Archivage du mouvement
        enregistrer_passage(id_du_rover, *cur_x, *cur_y, *batterie, statut_actuel);

        // 5. Sécurité batterie basse (Arrêt forcé si < 5%)
        if (*batterie < 5) {
            printf("[STOP] Batterie critique (%d%%). Arrêt du moteur.\n", *batterie);
            break;
        }

        printf("  > Rover %d en (%d, %d) [Bat: %d%%]\n", id_du_rover, *cur_x, *cur_y, *batterie);
        sleep(1); 
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int sock;
    struct sockaddr_in serv_addr;
    int id_rover = (argc > 1) ? atoi(argv[1]) : 1;
    int x = 0, y = 0, bat = 100; 
    int port_actuel = PORT_TERRE;

    printf("[SYSTÈME] Rover %d initialisé au point (0,0).\n", id_rover);

    while (bat > 0) {
        int choix = afficher_menu();
        if (choix == 5) break;
        if (choix == 0) continue;

        // Connexion au serveur (Terre ou Alpha)
        sock = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_actuel);
        inet_pton(AF_INET, ADRESSE_TERRE, &serv_addr.sin_addr);

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            if (port_actuel == PORT_TERRE) {
                printf("[RELAIS] Terre hors ligne. Connexion au Rover Alpha...\n");
                port_actuel = PORT_ALPHA;
                close(sock);
                continue; 
            } else {
                printf("[PANNE] Aucun serveur disponible. Mode survie activé.\n");
                break;
            }
        }

        // Préparation du paquet de requête
        Paquet p;
        p.id_envoyeur = id_rover;
        p.x = x; p.y = y; p.batterie = bat;
        p.horodatage = time(NULL);

        switch(choix) {
            case 1: p.type = STATUS_QUO; break;
            case 2: p.type = REQ_IDENTITE; break;
            case 3: p.type = ENVOI_LOGS; strcpy(p.corps, "Transfert des logs..."); break;
            case 4: p.type = ORDRE_RECHARGER; break;
        }

        send(sock, &p, sizeof(Paquet), 0);

        // Réception de l'ordre
        if (read(sock, &p, sizeof(Paquet)) > 0) {
            printf("[TERRE] Message : %s\n", p.corps);

            if (p.type == ORDRE_DEPLACER) {
                executer_deplacement(&x, &y, p.x, p.y, &bat, id_rover, sock);
            } 
            else if (p.type == ORDRE_RECHARGER || p.type == MODE_SECOURS) {
                // Règle : 1% = 1 seconde
                printf("[SOLAIRE] Début de la recharge (Batterie actuelle : %d%%)\n", bat);
                enregistrer_passage(id_rover, x, y, bat, "DÉBUT_RECHARGE");
                
                while (bat < 100) {
                    sleep(1);
                    bat++;
                    if (bat % 20 == 0) {
                        printf("  ... %d%% ...\n", bat);
                        enregistrer_passage(id_rover, x, y, bat, "RECHARGE_EN_COURS");
                    }
                }
                printf("[SOLAIRE] Batterie 100%%. Rover prêt.\n");
                enregistrer_passage(id_rover, x, y, bat, "RECHARGE_TERMINÉE");
            }
            else if (p.type == REQ_IDENTITE) {
                printf("[RADIO] Informations reçues : %s\n", p.corps);
            }
        }

        close(sock);
        sleep(1); 
    }

    printf("[FIN] Mission terminée. Extinction du Rover %d.\n", id_rover);
    return 0;
}