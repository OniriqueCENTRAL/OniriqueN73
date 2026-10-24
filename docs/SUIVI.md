# ONIRIQUE N73 — suivi

Dépôt public OniriqueCENTRAL/OniriqueN73 : chaque push sur main est construit (Mac + Windows) et publié.

## Fait
- Design validé dans Claude Design (https://claude.ai/artifact/LkFHWH3Hkn13PCZZPp6K4R), boutons rendus en 3D par
  `tools/KnobRender.cs` ; les mêmes rendus servent au plugin (`tools/render-knobs.ps1` → `Resources/knobs`).
- Moteur d'après les schémas Neve d'origine, la fiche AMS et les mesures SOS : voir `docs/MODELE.md`.
- Plugin : paramètres automatisables, état, presets d'usine et utilisateur, A / B, LEVEL MATCH, CAL, qualité,
  bypass aligné, mono / stéréo, interface du design (redimensionnable, clavier, accessibilité, molette, double-clic).
- Tests : banc DSP (`N73Bench`), tests fonctionnels (`N73PluginTest`), hôte d'essai (chargement / déchargement ×3),
  capture de l'interface (`ONIRIQUE_UI_PNG`). Résultats : `docs/MESURES.md`.

## Reste à faire
1. Validation contre un vrai 1073 si des mesures ou des prises de référence deviennent disponibles.

Publié : dépôt public OniriqueCENTRAL/OniriqueN73, slug et licence `onirique-n73`, fiche CENTRAL n° 26 (89,99 €,
9,99 € / mois), démo de la boutique faite avec `ONIRIQUE_UI_TEST=nolicense,demo` et onirique-probe.

## Décisions
- Calibration : 0 dBFS = +26 dBu par défaut (alignement UAD) ; MIC = signal ligne dans l'entrée micro (comme UAD) ;
  OUTPUT = volume après le module (ne change pas la saturation) ; LEVEL MATCH = suivi du volume d'entrée.
- Suréchantillonnage ×2 à 44,1 / 48 kHz (4 éch. de latence), aucun à 96 kHz ; ×4 en qualité haute.
- EQ en circuit à plat : −0,4 dB (propriété du réseau), conservé ; gains calés EQ hors circuit (conditions AMS).
- EQL ne met pas le HIGH PASS hors circuit (S7 ne commute que l'EQ sur le schéma EH10023).
- Préampli comme le manuel UAD : gains MIC et LINE séparés (clic sur MIC / LINE, dernière position de chaque entrée
  gardée dans le projet), OFF = traitement arrêté (son tel quel), PAD et MIC Z grisés en LINE, impédances 1,2 k /
  600 / 300 Ω, fader avant l'ampli de sortie. Plus de témoin de saturation en bas du module (demande de Nov).
