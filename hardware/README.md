# 🛠️ Conception Matérielle (Hardware)

Ce dossier contient l'intégralité du projet de conception de la carte mère de la **Station d'Analyse de la Qualité de l'Eau**. La carte est architecturée autour d'un microcontrôleur ultra-basse consommation **STM32U083RC**.

## 📌 Caractéristiques Techniques

* **Microcontrôleur :** STM32U083RC (ARM Cortex-M0+, optimisation Low-Power).
* **Mémoire non volatile :** FeRAM (Ferroelectric RAM) via bus SPI pour le datalogging.
* **Connectivité Radio :**
    * **LoRaWAN :** Module radio intégré pour la transmission vers réseau ChirpStack.
    * **Bluetooth Low Energy (BLE) :** Pour la récupération locale des données en cas de problème avec le LoRa.

## 📂 Architecture du dossier KiCad

* `*.kicad_sch` : Schémas électriques hiérarchiques.
* `*.kicad_pcb` : Routage de la carte.
* `/production` : Fichiers de fabrication générés pour JLCPCB.
* `/3D_Models` : Fichiers STEP pour le rendu 3D.