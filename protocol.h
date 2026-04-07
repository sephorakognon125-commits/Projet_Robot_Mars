#ifndef PROTOCOL_H
#define PROTOCOL_H

#include <time.h>
#include <stdint.h>
#include <stdbool.h>

/* * --- CONSTANTES DE CONFIGURATION --- 
 * Centraliser ces valeurs ici permet de modifier le comportement 
 * de toute la mission sans toucher au code source du Rover ou du Serveur.
 */
#define PORT_TERRE 8080
#define PORT_ALPHA 8081      // Port de secours (Rover Alpha)
#define ADRESSE_TERRE "127.0.0.1"

#define CHANCE_TRESOR 5    // 1 chance sur 5 de trouver un trésor par case
#define SEUIL_BATTERIE 20    // Seuil critique déclenchant la recharge solaire
#define MAX_ROVERS 10        // Limite théorique de la flotte pour le serveur
#define MAX_TRESORS 100      // Capacité maximale de trésors
#define SAUVEGARDE_PAS 10    // Fréquence d'envoi de mises à jour au serveur (toutes les 10 étapes)

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
    MODE_SECOURS,      // Signal spécifique au Rover Alpha (Redondance)
    DEM_INIT,          // Message de synchronisation initiale (Rover -> Terre) utilisé pour l'initialisation bloquante afin de la récupération de coordonnées exactes (idée de Moise MUSHIMIYIMANA pour éviter les coordonnées aléatoires)
    ERREUR,            // Message d'erreur générique (ex: "Donnee invalide", etc.) Idee de Moise MUSHIMIYIMANA pour centraliser la gestion des erreurs et permettre une communication claire en cas de problème
} TypeMessage;

/* * 1. Structure PAQUET 
 * C'est l'objet qui circule sur le réseau via TCP.
 * Sa taille doit être fixe pour éviter les erreurs de lecture (sizeof(Paquet)).
 */
typedef struct {
    int id_envoyeur;      // ID du Rover (1, 2, 3...) ou 0 pour la Terre
    int x;                // Coordonnée X actuelle ou cible
    int y;                // Coordonnée Y actuelle ou cible
    int batterie;         // Niveau d'énergie (0-100)
    TypeMessage type;     // Nature du message
    time_t horodatage;    // Temps Unix pour la synchronisation des logs
    char corps[256];      // Message texte (ex: "Mission accomplie", "Trésor trouvé")
} Paquet;

// --- STRUCTURES POUR LA CARTE (Mémoire du serveur) ---
typedef struct {
    int pos_x;            // Traduit : x
    int pos_y;            // Traduit : y
    int id_rover;         // Qui l'a trouvé ?
} Tresor;

/* * 2. Structure CLIENT (Côté Serveur)
 * Permet au serveur de garder en mémoire l'état de chaque Rover connecté
 * sans avoir à lui redemander à chaque fois.
 */
typedef struct {
    int id_robot;
    int pos_x;            // Traduit : x
    int pos_y;            // Traduit : y
    int niveau_batterie;  // Traduit : batterie
    int est_alpha;        // Flag pour la redondance
    int est_occupe;        // 0 = Disponible, 1 = En mission (Empêche les doublons d'ID)
    Paquet dernier_paquet; // Historique du dernier échange
} ClientRover;

/* * 3. Structure SERVEUR 
 * État global de la Station Terre.
 */
typedef struct {
    int id_serveur;                     // Toujours 0
    int base_x, base_y;                 // Traduit : x, y (Coordonnées de la base)
    int nb_rovers;                      // Compteur actif de la flotte
    int nb_tresors_trouves;             // Compteur de trésors trouvés
    Tresor carte_tresors[MAX_TRESORS];  // Tableau de trésors trouvés
    ClientRover flotte[MAX_ROVERS];     // Défini la flotte liste de rovers connus du serveur
} ServeurTerre;

// ===== PROTOTYPES =====

// Journal serveur
void journaliser_serveur(int id_rover, int pos_x, int pos_y, const char* action);

// Sauvegarde / Chargement
void sauvegarder_positions(ServeurTerre *t);
void charger_positions(ServeurTerre *t);

// Logique métier
void trouver_voisin_proche(ServeurTerre *t, Paquet *p);
int calculer_distance(int x1, int y1, int x2, int y2);
int peut_accomplir_mission(int x_actuel, int y_actuel, int x_cible, int y_cible, int batterie_actuelle);

// Synchronisation
void synchroniser_rover(ServeurTerre *t, Paquet *p);

// ===== PROTOTYPES =====
void journaliser_serveur(int id_rover, int pos_x, int pos_y, const char* action);
void sauvegarder_positions(ServeurTerre *t);
void charger_positions(ServeurTerre *t);
void trouver_voisin_proche(ServeurTerre *t, Paquet *p);
int calculer_distance(int x1, int y1, int x2, int y2);
int peut_accomplir_mission(int x_actuel, int y_actuel, int x_cible, int y_cible, int batterie_actuelle);
void synchroniser_rover(ServeurTerre *t, Paquet *p);

#endif