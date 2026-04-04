#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <time.h>
#include "protocol.h" // Inclusion du fichier de structures partagées

/**
 * FONCTION : afficher_menu
 * -------------------------
 * Interface Utilisateur (IHM). Elle permet à l'opérateur humain de choisir
 * l'action que le Rover doit entreprendre.
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
    
    // Sécurité : si l'utilisateur saisit autre chose qu'un nombre
    if (scanf("%d", &choix) != 1) {
        while(getchar() != '\n'); // Nettoyer le buffer d'entrée
        return 0;
    }
    return choix;
}

/**
 * FONCTION : enregistrer_passage
 * ------------------------------
 * Journal de bord local du Rover. Chaque mouvement est écrit dans un fichier 
 * nommé "rover_X.log". C'est la "boîte noire" du robot.
 */
void enregistrer_passage(int id_rover, int x, int y, int bat, char* statut) {
    char nom_fichier[32];
    sprintf(nom_fichier, "rover_%d.log", id_rover);

    FILE *f = fopen(nom_fichier, "a"); // "a" pour ajouter sans effacer l'existant
    if (f == NULL) return;

    time_t now = time(NULL);
    struct tm *t = localtime(&now);
    char s_now[25];
    strftime(s_now, sizeof(s_now), "%Y-%m-%d %H:%M:%S", t); // Formatage de la date

    fprintf(f, "[%s] POS(%3d,%3d) | BAT(%3d%%) | STATUT: %s\n", s_now, x, y, bat, statut);

    fflush(f); // Force l'écriture sur le disque
    fclose(f);
}

/**
 * FONCTION : executer_deplacement
 * -------------------------------
 * C'est l'intelligence de mouvement. Elle gère le trajet case par case.
 * Règle : Déplacement Horizontal (X) d'abord, puis Vertical (Y).
 * 
 * Détecte les trésors et informe le serveur en temps réel.
 */
void executer_deplacement(int *cur_x, int *cur_y, int cible_x, int cible_y, int *batterie, int id_du_rover, int sock) {
    Paquet alerte; // Structure pour envoyer des messages pendant le trajet
    printf("[MOUVEMENT] En route vers (%d, %d)...\n", cible_x, cible_y);

    // Boucle tant qu'on n'est pas arrivé OU qu'on a encore de la batterie
    while ((*cur_x != cible_x || *cur_y != cible_y) && *batterie > 0) {
        
        // 1. DÉPLACEMENT AXE PAR AXE
        if (*cur_x != cible_x) {
            (*cur_x < cible_x) ? (*cur_x)++ : (*cur_x)--; // Pas horizontal
        } else {
            (*cur_y < cible_y) ? (*cur_y)++ : (*cur_y)--; // Pas vertical
        }

        // 2. CONSOMMATION : Chaque case coûte 1%
        (*batterie)--; 
        char* statut_actuel = "VIDE";

        // 3. DÉTECTION DE TRÉSOR (1 chance sur CHANCE_TRESOR)
        if ((rand() % CHANCE_TRESOR) == 0) {
            statut_actuel = "TRESOR"; 
            printf("[DÉCOUVERTE] R%d : Trésor détecté en (%d, %d)!\n", id_du_rover, *cur_x, *cur_y);
            
            // On prépare une alerte immédiate pour le serveur
            alerte.id_envoyeur = id_du_rover;
            alerte.type = ALERTE_TRESOR;
            alerte.x = *cur_x;
            alerte.y = *cur_y;
            alerte.batterie = *batterie;
            strcpy(alerte.corps, "Trésor trouvé !");
            
            // ENVOI ASYNCHRONE : On informe la Terre sans arrêter le mouvement
            send(sock, &alerte, sizeof(Paquet), 0); 
        }

        // 4. LOG : On enregistre chaque étape dans le fichier local
        enregistrer_passage(id_du_rover, *cur_x, *cur_y, *batterie, statut_actuel);

        // 5. SÉCURITÉ : Si la batterie tombe à 0, on stoppe tout
        if (*batterie <= 0) {
            printf("[PANNE] Plus d'énergie. Mission interrompue.\n");
            break;
        }

        printf("  > Rover %d : Position (%d, %d) | Énergie : %d%%\n", id_du_rover, *cur_x, *cur_y, *batterie);
        sleep(1); // Simule le temps de trajet (1 seconde par case)
    }
}

int main(int argc, char *argv[]) {
    srand(time(NULL));

    int sock;
    struct sockaddr_in serv_addr;
    
    // On récupère l'ID via la ligne de commande (ex: ./rover 2)
    int id_rover = (argc > 1) ? atoi(argv[1]) : 1;
    int x = 0, y = 0, bat = 100; 
    int port_actuel = PORT_TERRE;

    printf("[SYSTÈME] Rover %d paré au décollage.\n", id_rover);

    while (bat > 0) {
        int choix = afficher_menu();
        if (choix == 5) break; // Quitter le programme

        // 1. CRÉATION DU SOCKET TCP
        sock = socket(AF_INET, SOCK_STREAM, 0);
        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(port_actuel);
        inet_pton(AF_INET, ADRESSE_TERRE, &serv_addr.sin_addr);

        // 2. CONNEXION AVEC REDONDANCE (Mode Alpha)
        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            if (port_actuel == PORT_TERRE) {
                printf("[ALERTE] Terre injoignable. Bascule sur le port ALPHA (%d)...\n", PORT_ALPHA);
                port_actuel = PORT_ALPHA;
                close(sock);
                continue; // On relance la boucle pour retenter la connexion
            } else {
                printf("[ERREUR] Station de secours injoignable. Fin du programme.\n");
                break;
            }
        }

        // 3. PRÉPARATION DU PAQUET DE DEMANDE
        Paquet p;
        p.id_envoyeur = id_rover;
        p.x = x; p.y = y; p.batterie = bat;
        p.horodatage = time(NULL);

        // On définit le type de message selon le choix du menu
        switch(choix) {
            case 1: p.type = STATUS_QUO; break; // Demande une destination
            case 4: p.type = ORDRE_RECHARGER; break; // Demande à recharger
            default: p.type = STATUS_QUO;
        }

        // 4. ENVOI DE LA REQUÊTE
        send(sock, &p, sizeof(Paquet), 0);

        // 5. RÉCEPTION DE L'ORDRE DU SERVEUR
        if (read(sock, &p, sizeof(Paquet)) > 0) {
            printf("[RADIO] Message Terre : %s\n", p.corps);

            // ANALYSE DE L'ORDRE REÇU
            if (p.type == ORDRE_DEPLACER) {
                // On lance le mouvement intelligent
                executer_deplacement(&x, &y, p.x, p.y, &bat, id_rover, sock);
            } 
            else if (p.type == ORDRE_RECHARGER) {
                // BOUCLE DE RECHARGE (1% = 1 seconde)
                printf("[RECHARGE] Exposition aux panneaux solaires...\n");
                enregistrer_passage(id_rover, x, y, bat, "DÉBUT_RECHARGE");
                while (bat < 100) {
                    sleep(1);
                    bat++;
                    if (bat % 20 == 0) printf("  Batterie : %d%%...\n", bat);
                }
                printf("[RECHARGE] 100%% atteint. Rover prêt.\n");
                enregistrer_passage(id_rover, x, y, bat, "RECHARGE_FINIE");
            }
        }

        // 6. DÉCONNEXION : On ferme le socket après chaque échange (modèle transactionnel)
        close(sock);
        sleep(1); 
    }

    printf("[FIN] Mission terminée pour le Rover %d.\n", id_rover);
    return 0;
}