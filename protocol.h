#ifndef PROTOCOL_H
#define PROTOCOL_H

// Les types de messages définis dans ton sujet
typedef enum {
    MSG_DEMANDE_ACTION,   // Le rover demande sa prochaine coordonnée
    MSG_ORDRE_DEPLACER,   // Le serveur donne la nouvelle position
    MSG_RECHARGE,         // Le serveur ordonne de charger (batterie < 20%)
    MSG_TRESOR_TROUVE      // Le rover signale un trésor (1 chance sur 5)
} TypeMessage;

// Structure de données transmise par socket
typedef struct {
    int rover_id;
    TypeMessage type;
    int x;                // Coordonnée X
    int y;                // Coordonnée Y
    int batterie;         // Niveau de batterie (1% = 1 déplacement)
} Paquet;

#endif