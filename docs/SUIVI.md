# ONIRIQUE N73 — suivi

Dépôt local, sans remote. Aucun push, aucune publication sans accord explicite de Nov.

## Fait
- Design validé dans Claude Design (https://claude.ai/artifact/LkFHWH3Hkn13PCZZPp6K4R), boutons rendus en 3D par
  `tools/KnobRender.cs` ; les mêmes rendus servent au plugin (`tools/render-knobs.ps1` → `Resources/knobs`).
- Moteur d'après les schémas Neve d'origine, la fiche AMS et les mesures SOS : voir `docs/MODELE.md`.
- Plugin : paramètres automatisables, état, presets d'usine et utilisateur, A / B, LEVEL MATCH, CAL, qualité,
  bypass aligné, mono / stéréo, interface du design (redimensionnable, clavier, accessibilité, molette, double-clic).
- Tests : banc DSP (`N73Bench`), tests fonctionnels (`N73PluginTest`), hôte d'essai (chargement / déchargement ×3),
  capture de l'interface (`ONIRIQUE_UI_PNG`). Résultats : `docs/MESURES.md`.

## Reste à faire
1. Essai dans FL Studio par Nov (build local sans licence : `-DN73_LICENCE=OFF`).
2. Identifiant de licence dans ONIRIQUE CENTRAL (« n73 » provisoire dans `PluginProcessor.h`), build avec licence.
3. Build Mac (AU / VST3) et pipeline de publication — seulement avec l'accord de Nov.
4. Validation contre un vrai 1073 si des mesures ou des prises de référence deviennent disponibles.

## Décisions
- Calibration : 0 dBFS = +18 dBu par défaut ; MIC lu 60 dB sous le niveau ligne (MIC 60 ≈ gain unité).
- Suréchantillonnage ×2 à 44,1 / 48 kHz (4 éch. de latence), aucun à 96 kHz ; ×4 en qualité haute.
- EQ en circuit à plat : −0,4 dB (propriété du réseau), conservé ; gains calés EQ hors circuit (conditions AMS).
- EQL ne met pas le HIGH PASS hors circuit (S7 ne commute que l'EQ sur le schéma EH10023).
