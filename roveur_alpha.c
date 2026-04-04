int main() {
    // ... initialisation des sockets ...

    // On initialise l'état global du serveur
    ServeurTerre ma_terre;
    ma_terre.nb_rovers = 0;
    ma_terre.nb_tresors_trouves = 0;

    printf("[STATION TERRE] Mémoire de mission initialisée.\n");

    while (1) {
        new_socket = accept(server_fd, ...);
        
        Paquet p;
        if (read(new_socket, &p, sizeof(Paquet)) > 0) {
            
            // --- MISE À JOUR DE LA CARTE DES ROVERS ---
            // On enregistre/met à jour la position du rover dans la flotte
            int index = -1;
            for(int i=0; i < ma_terre.nb_rovers; i++) {
                if (ma_terre.flotte[i].id_robot == p.id_envoyeur) index = i;
            }
            
            if (index == -1 && ma_terre.nb_rovers < MAX_ROVERS) {
                index = ma_terre.nb_rovers++;
                ma_terre.flotte[index].id_robot = p.id_envoyeur;
            }
            
            if (index != -1) {
                ma_terre.flotte[index].x = p.x;
                ma_terre.flotte[index].y = p.y;
                ma_terre.flotte[index].batterie = p.batterie;
            }

            // ... logique de calcul de mission ...

            // --- MISE À JOUR DE LA CARTE DES TRÉSORS ---
            while (read(new_socket, &p, sizeof(Paquet)) > 0) {
                if (p.type == ALERTE_TRESOR) {
                    if (ma_terre.nb_tresors_trouves < MAX_TRESORS) {
                        int t = ma_terre.nb_tresors_trouves++;
                        ma_terre.carte_tresors[t].x = p.x;
                        ma_terre.carte_tresors[t].y = p.y;
                        ma_terre.carte_tresors[t].id_rover = p.id_envoyeur;
                        
                        printf("[CARTE] Trésor n°%d enregistré en (%d,%d)\n", 
                                ma_terre.nb_tresors_trouves, p.x, p.y);
                    }
                }
            }
        }
        close(new_socket);
    }
}