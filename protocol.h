#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <time.h>
#include <stdint.h> // Pour des types de données plus précis si nécessaire

/* * --- CONSTANTES DE CONFIGURATION --- 
 * Centraliser ces valeurs ici permet de modifier le comportement 
 * de toute la mission sans toucher au code source du Rover ou du Serveur.
 */
#define PORT_TERRE 8080
#define PORT_ALPHA 8081      // Port de secours (Rover Alpha)
#define ADRESSE_TERRE "127.0.0.1"

#define CHANCE_TRESOR 10     // 1 chance sur 10 de trouver un trésor par case
#define SEUIL_BATTERIE 20    // Seuil critique déclenchant la recharge solaire
#define MAX_ROVERS 10        // Limite théorique de la flotte pour le serveur

/* * --- TYPES DE MESSAGES --- 
 * Définit le protocole de communication. 
 * Chaque paquet envoyé DOIT avoir un type pour être interprété.
 */
typedef enum {
    ORDRE_DEPLACER,    // La Terre envoie une cible (X, Y)
    ORDRE_RECHARGER,   // La Terre ordonne l'arrêt pour recharge
    ALERTE_TRESOR,     // Le Rover signale une découverte en temps réel
    REQ_IDENTITE,      // Requête pour localiser les voisins
    ENVOI_LOGS,        // Commande d'archivage/transfert de fichiers
    STATUS_QUO,        // Message générique / Demande de mission
    MODE_SECOURS       // Signal spécifique au Rover Alpha (Redondance)
} TypeMessage;

/* * 1. Structure PAQUET 
 * C'est l'objet qui circule sur le réseau via TCP.
 * Sa taille doit être fixe pour éviter les erreurs de lecture (sizeof(Paquet)).
 */
typedef struct {
    int id_envoyeur;      // ID du Rover (1, 2, 3...) ou 0 pour la Terre
    TypeMessage type;     // Nature du message
    int x;                // Coordonnée X actuelle ou cible
    int y;                // Coordonnée Y actuelle ou cible
    int batterie;         // Niveau d'énergie (0-100)
    char corps[256];      // Message texte (ex: "Mission accomplie", "Trésor trouvé")
    time_t horodatage;    // Temps Unix pour la synchronisation des logs
} Paquet;

/* * 2. Structure CLIENT (Côté Serveur)
 * Permet au serveur de garder en mémoire l'état de chaque Rover connecté
 * sans avoir à lui redemander à chaque fois.
 */
typedef struct {
    int id_robot;
    int x;
    int y;
    int batterie;
    int est_alpha;        // Flag pour la redondance
    int en_recharge;      // Flag d'état (booléen)
    Paquet dernier_paquet; // Historique du dernier échange
} ClientRover;

/* * 3. Structure SERVEUR 
 * État global de la Station Terre.
 */
typedef struct {
    int id_serveur;       // Toujours 0
    int x, y;             // Coordonnées de la base (souvent 0,0)
    int nb_rovers;        // Compteur actif de la flotte
    // On pourrait ajouter ici un tableau de trésors trouvés
} ServeurTerre;

#endif