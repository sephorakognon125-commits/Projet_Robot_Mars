# Projet Robot Mars - L3 MIASHS
**Auteurs :** 
- Kognon-Lengo Sephora
- Moïse MUSHIMIYIMANA
- KARANGANWA Jean Claude

## Présentation
Ce projet simule la communication entre des robots explorateurs sur Mars et une station de contrôle sur Terre. Il met en œuvre des concepts de programmation système, de réseaux (sockets TCP) et de robustesse.

## Compilation
Utilisez les commandes suivantes dans votre terminal Ubuntu :
- Serveur Terre : `gcc serveur_terre.c -o serveur_terre -lpthread`
- Rover Alpha (Secours) : `gcc rover_alpha.c -o rover_alpha`
- Rover : `gcc rover.c -o rover`

## Fonctionnement
1. **Lancer le secours** : `./rover_alpha` (écoute sur le port 8081)
2. **Lancer la station Terre** : `./serveur_terre` (écoute sur le port 8080)
3. **Lancer l'exploration** : `./rover`

## Fonctionnalités avancées
- **Déplacement réaliste** : Le rover se déplace case par case (horizontalement puis verticalement).
- **Gestion d'énergie** : Ordre de recharge automatique si la batterie est < 20%.
- **Détection de trésors** : Probabilité de 1/5 de trouver un trésor à chaque pas.
- **Robustesse (Mode Alpha)** : Bascule automatique vers le Rover Alpha si le serveur Terre est indisponible.
- **Logs** : Historique complet des actions dans `rover.log` et `serveur_terre.log`.
