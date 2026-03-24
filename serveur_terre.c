#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <time.h>
#include "protocol.h"

#define PORT 8080

// Fonction pour écrire les logs du serveur 
void ecrire_log_serveur(const char *message) {
    FILE *f = fopen("serveur_terre.log", "a");
    if (f == NULL) return;
    time_t now = time(NULL);
    char *date = ctime(&now);
    date[strlen(date) - 1] = '\0';
    fprintf(f, "[%s] %s\n", date, message);
    fclose(f);
}

void *gerer_rover(void *socket_desc) {
    int sock = *(int*)socket_desc;
    Paquet p;
    char log_buffer[256];
    
    printf("[Terre] Connexion d'un nouveau rover établie.\n");
    ecrire_log_serveur("Nouveau rover connecté au centre de contrôle.");

    while(recv(sock, &p, sizeof(Paquet), 0) > 0) {
        if (p.type == MSG_TRESOR_TROUVE) {
            sprintf(log_buffer, "Rover %d a trouvé un TRÉSOR en (%d, %d) !", p.rover_id, p.x, p.y);
            printf("[Terre] %s\n", log_buffer);
            ecrire_log_serveur(log_buffer);
        } 
        else if (p.type == MSG_DEMANDE_ACTION) {
            // Logique de batterie 
            if (p.batterie < 20) {
                p.type = MSG_RECHARGE;
                sprintf(log_buffer, "Rover %d : Batterie faible (%d%%). Ordre : RECHARGE.", p.rover_id, p.batterie);
            } else {
                p.type = MSG_ORDRE_DEPLACER;
                // On donne une nouvelle destination (aléatoire pour simuler l'exploration) [cite: 19]
                p.x = rand() % 10;
                p.y = rand() % 10;
                sprintf(log_buffer, "Rover %d : Envoi vers nouvelle cible (%d, %d).", p.rover_id, p.x, p.y);
            }
            printf("[Terre] %s\n", log_buffer);
            ecrire_log_serveur(log_buffer);
            send(sock, &p, sizeof(Paquet), 0);
        }
    }

    printf("[Terre] Rover déconnecté.\n");
    ecrire_log_serveur("Un rover s'est déconnecté.");
    close(sock);
    free(socket_desc);
    return NULL;
}

int main() {
    int socket_terre, client_sock, *nouvelle_sock;
    struct sockaddr_in serveur, client;
    socklen_t c = sizeof(struct sockaddr_in);
    srand(time(NULL));

    socket_terre = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(socket_terre, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    serveur.sin_family = AF_INET;
    serveur.sin_addr.s_addr = INADDR_ANY;
    serveur.sin_port = htons(PORT);

    if(bind(socket_terre, (struct sockaddr *)&serveur, sizeof(serveur)) < 0) {
        perror("[Erreur] Bind échoué");
        return 1;
    }

    listen(socket_terre, 5);
    printf("[Terre] Station de Toulouse active sur le port %d...\n", PORT);
    ecrire_log_serveur("DÉMARRAGE DU SERVEUR TERRE");

    while((client_sock = accept(socket_terre, (struct sockaddr *)&client, &c))) {
        pthread_t thread_rover;
        nouvelle_sock = malloc(sizeof(int));
        *nouvelle_sock = client_sock;
        pthread_create(&thread_rover, NULL, gerer_rover, (void*)nouvelle_sock);
        pthread_detach(thread_rover); // Gestion asynchrone [cite: 9]
    }

    return 0;
}