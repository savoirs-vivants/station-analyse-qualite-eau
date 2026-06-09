# 💻 Code Embarqué (Firmware STM32)

Ce dossier contient le code source en **C** développé sous **STM32CubeIDE / Eclipse**. Le firmware orchestre l'acquisition des données environnementales, leur stockage sécurisé et leur transmission.

## ⚙️ Environnement & Outils

* **Cible :** STM32U083RC
* **IDE :** STM32CubeIDE (Génération initiale via STM32CubeMX)
* **Documentation :** Doxygen + Graphviz (Voir la section *Documentation* ci-dessous).

## 📂 Architecture du Code

Le projet suit la structure standard de STM32CubeIDE, enrichie de nos propres bibliothèques matérielles :

* `Core/Src/main.c` : Point d'entrée.
* `Core/Src/app_main_test.c` : Fichier d'orchestration principale (Machine d'états, gestion des priorités et du mode Sleep).
* `Drivers/Custom/` : Pilotes développés spécifiquement pour la station :
    * `driverDS18B20` : Lecture de la température (1-Wire émulé via UART).
    * `driverAnalog` : Gestion de l'ADC avec séquenceur fixe et transferts DMA.
    * `driverFRAM` & `driverDataLog` : Pilotes SPI bas niveau et système de fichiers pour le stockage des logs.
    * `driverLoRaWAN` : Empaquetage (Cayenne LPP) et transmission des trames.
    * `driverBLE` : Streaming des archives de données vers un smartphone.
    * `driverRTC` : Gestion du temps et réveils périodiques.

## 🚀 Fonctionnalités Clés & Choix Techniques

1.  **Datalogging Robuste :** Les mesures sont stockées en FeRAM. Le système utilise un pointeur circulaire (`newest_id`) pour gérer l'écrasement des données les plus anciennes.
2.  **Transmission LoRaWAN :** Envoi périodique sur réseau privé ChirpStack (Utilisation d'un `JoinEUI` à `0000000000000000` par défaut). Format de charge utile : **Cayenne LPP**.
3.  **Optimisation des transferts :** Utilisation systématique du DMA pour les lectures FeRAM et le streaming BLE afin de ne pas bloquer le processeur et éviter les corruptions de données.
4.  **Mathématiques en Virgule Fixe :** Remplacement des `float` par la notation en virgule fixe (Q-format) sur 16 bits signés pour optimiser le temps d'exécution et réduire la charge utile radio.

## 📚 Documentation (Doxygen)

Le code est intégralement documenté via des balises Doxygen.
Pour générer la documentation HTML sur votre machine :
1. Ouvrir `Doxywizard`.
2. Charger le fichier de configuration (Doxyfile) situé à la racine du projet firmware.
3. Vérifier que l'option **"Scan recursively"** est bien cochée.
4. Cliquer sur **"Run Doxygen"**. La documentation sera disponible dans le dossier `/html`.