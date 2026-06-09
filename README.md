# Station d'Analyse de la Qualité de l'Eau 💧

Station autonome pour la surveillance de la qualité de l'eau. Ce dépôt contient l'intégralité de la conception matérielle (schémas et routage PCB) ainsi que le firmware d'acquisition.

## 🎯 Objectif du projet

Ce système a pour but de collecter, traiter, transmettre et enregistrer des données environnementales issues de différentes sondes. Il est conçu pour être autonome en énergie et low-cost.

**Paramètres mesurés :**
*  Température de l'eau
*  Température et Humidité de l'air
*  Conductivité / TDS
*  Turbidité
*  Débit/Hauteur d'eau

## 📁 Architecture du projet

Ce dépôt est organisé en deux parties distinctes :

* 🛠️ **`/hardware`** : Fichiers de conception de la carte électronique réalisés sous KiCad (Schémas, PCB, nomenclatures).
* 💻 **`/firmware`** : Code source en C gérant l'acquisition , le traitement des signaux capteurs et la logique système.

*Pour plus de détails techniques, veuillez consulter les fichiers `README.md` présents dans chacun de ces répertoires.*