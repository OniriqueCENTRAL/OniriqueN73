# ONIRIQUE N73 — suivi

Dépôt local, sans remote. Aucun push, aucune publication sans accord explicite.

## Fait
- Design validé dans Claude Design (https://claude.ai/artifact/LkFHWH3Hkn13PCZZPp6K4R) : boutons rendus en 3D par
  `tools/KnobRender.cs`.
- Données : schémas Neve d'origine (EH10023, EK20033, BA283, B182, B205, B211 — « 1073 fullpak », archive.org),
  manuel AMS Neve 1073/1084 issue 5.1, mesures SOS du 1073N, réseau d'EQ reconstitué par NYAN-1073 (CC BY-SA),
  mesures de clones (Braingasm, repères relatifs seulement).
- Moteur (`Source/dsp`, `Source/N73Engine.h`) : transformateurs (10468, 31267, LO1166 polarisé), BA283 NV / AM,
  BA284, EQ simulé comme circuit, passe-haut passif 1k2 / 5k1, suréchantillonnage ×2 (×4 en qualité haute).
- Banc `Tests/N73Bench.cpp` (`N73Bench all|eq|gain|freq|thd|clip|alias|latency|cpu|robust|clicks|prof`) :
  - gains : toutes positions à l'étiquette avec EQ hors circuit (−0,5 dB EQ en circuit à plat : propriété du réseau) ;
  - DHT 0,038 % à +20 dBu 1 kHz (H3 dominante), 0,050 % à 50 Hz ; 1 % à +26,5 dBu, 3 % à +28,5 dBu ;
  - réponse : −3 dB vers 6–7 Hz, bosse +0,4 dB à 15 Hz, plat à 20 kHz (ligne), +0,5 dB à 20 kHz (micro) ;
  - latence 4 éch. à 44,1/48 kHz (6 en qualité haute), 0 à 96 kHz ;
  - CPU 2,6–2,9 % d'un cœur par instance (5,2–5,7 % en qualité haute) ;
  - repliement en écrêtage extrême : −55 dB (×2), −94 dB (×4) ;
  - robustesse 42/42 configurations ; pas de clic aux commutateurs (fondus de 20 ms).

## En cours
- `tools/KnobRender.cs` : ajout de la rotation des lobes et du plan d'ombre (`Floor`) — pas encore compilé.
  Reste à écrire `RenderAssets` : gain_00..21, hpf_0..4, high_base, high_cap_00..11, ring, midlow_cap_00..11,
  out_00..11 dans `Resources/knobs/` (×2), pointeurs dessinés par le code.

## Reste à faire
1. Images des boutons, puis `PluginProcessor` (paramètres, état, presets, A/B, latence, licence) et
   `PluginEditor` (design validé, redimensionnable).
2. Essai de chargement / déchargement (HoteEssai), build VST3 local, essai dans FL Studio par Nov.
3. Docs : `MODELE.md` (sources, estimations, limites), rapport de mesures, instructions de build.

## Estimations à documenter (non mesurées sur un vrai 1073)
Source de 150 Ω pour MIC Z, inductances et flux de saturation des transformateurs, rails et courbures des
étages, résistances de butée des potentiomètres, câblage exact de la broche L de la B205 (topologie NYAN gardée).
