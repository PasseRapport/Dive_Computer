# Dive Watch — application Android et montre simulée

Première version de développement, personnelle et hors ligne : apnée et bouteille à l’air, carnet, profils, événements, sorties annotées, sauvegarde réimportable et manuel embarqué. Les données simulées restent identifiées.

## Organisation

- `android/` : application Kotlin / Jetpack Compose, Room et connexion Nordic NUS.
- `core/` : protocole binaire, validation, statistiques, reprise persistante et archives ; tests JVM sans téléphone.
- `simulator/` : périphérique BLE réel sur Linux/BlueZ et implémentation Python indépendante.
- `protocol/SPEC.txt` : contrat effectivement implémenté et fichiers de référence.
- `tools/` : commandes de vérification et de lancement.

Les dossiers `V1/` et `V2/` ne sont pas modifiés. Les échanges de carnet ne servent pas aux mises à jour de firmware ; MCUmgr/SMP est prévu ultérieurement.

## Compiler

Prérequis : JDK 17, SDK Android 35 et Build Tools 35.0.0. Dans ce dossier, créer `local.properties` avec `sdk.dir=/chemin/vers/le/sdk` si `ANDROID_HOME` n’est pas défini. Les dépendances sont téléchargées à la première compilation.

```sh
./gradlew :core:test :android:assembleDebug :android:lintDebug
python3 -m unittest discover -s simulator -v
```

APK : `android/build/outputs/apk/debug/android-debug.apk`. Android 8.0/API 26 minimum proposé pour ce premier développement ; aucune application Play Store n’est publiée. La clé de débogage n’est pas destinée à une distribution définitive.

## Consulter les plongées

Connecter la montre par Bluetooth puis synchroniser. Les données restent consultables après déconnexion et redémarrage. Le mode de démonstration intégré a été retiré de l’interface ; les tests utilisent toujours des données fictives indépendantes du téléphone.

Après synchronisation complète, **Vider la mémoire** supprime les plongées terminées de la montre après confirmation, tout en conservant le carnet du téléphone. Le firmware doit annoncer cette capacité. Le simulateur la prend en charge ; son redémarrage recrée ses données fictives.

## Simuler la montre sur le PC

Sur Fedora, les dépendances sont `python3`, `python3-dbus`, `python3-gobject` et BlueZ. L’adaptateur doit être allumé et exposer les interfaces GATT et advertising. Aucun changement permanent de configuration Bluetooth n’est requis.

```sh
./tools/run-simulator.sh
./tools/run-simulator.sh --scenario gap
./tools/run-simulator.sh --scenario corrupt-block
./tools/run-simulator.sh --scenario disconnect
```

Scénarios : `normal`, `gap`, `unknown-time`, `unknown-alert`, `active-dive`, `corrupt-block`, `disconnect`. L’option `--fragment-size 20` impose de petits fragments ; `--fragment-size 244` utilise jusqu’à 244 octets, sans dépasser la MTU observée. `--duration 10` permet un essai court de publication BlueZ. Le scénario `disconnect` coupe une fois après une réponse de bloc, puis accepte une reprise dans le même processus.

Le PC annonce **DiveWatch-SIM**. Sur le téléphone : Montre → Rechercher une montre → sélectionner le PC → Synchroniser. Le simulateur initial est ouvert et destiné aux données fictives ; nom et UUID ne constituent pas une authentification.

## Ce qui doit encore être validé

Consulter `VALIDATION.txt` pour les résultats réellement obtenus. La compilation et les tests de fichiers ne prouvent pas la connexion Android ↔ PC, ni les performances, la consommation ou la compatibilité avec la future montre Nordic. Le firmware existant n’est pas changé.

Le manuel embarqué explique uniquement les fonctions implémentées et distingue les procédures matérielles encore non validées. Aucun calcul de décompression n’est effectué.
