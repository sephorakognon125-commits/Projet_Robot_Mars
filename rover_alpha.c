#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include "protocol.h"

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int addrlen = sizeof(address);
    Paquet p;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(8081); // Port de secours

    bind(server_fd, (struct sockaddr *)&address, sizeof(address));
    listen(server_fd, 3);

    printf("[Alpha] Rover Alpha prêt (Port 8081). En attente des autres rovers...\n");

    while((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))) {
        while(recv(new_socket, &p, sizeof(Paquet), 0) > 0) {
            printf("[Alpha] Rover %d demande de l'aide.\n", p.rover_id);
            // Réponse forcée du sujet : recharge et attends [cite: 51]
            p.type = MSG_RECHARGE; 
            send(new_socket, &p, sizeof(Paquet), 0);
        }
        close(new_socket);
    }
    return 0;
}