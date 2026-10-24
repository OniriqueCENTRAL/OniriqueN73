# ONIRIQUE N73 — mesures, CPU, latence

Toutes ces valeurs sortent des programmes de test du dépôt, exécutés sur la machine de développement (Windows 11,
MSVC 2022, Release). Sortie complète : `docs/banc-sortie.txt`. Pour les refaire :

```
build-bench\N73Bench_artefacts\Release\N73Bench.exe all
build\N73PluginTest_artefacts\Release\N73PluginTest.exe
```

## Comparaison aux données publiées

| Mesure | N73 (banc) | Référence |
|---|---|---|
| Gains (EQ hors circuit) | toutes positions à l'étiquette | AMS : 20–80 / −10…+20 ; SOS : à 1,5 dB près |
| DHT 1 kHz, +20 dBu | 0,038 % (H3 0,037 %, H2 0,003 %) | AMS : ≤ 0,07 % |
| DHT 50 Hz, +20 dBu | 0,050 % | AMS : ≤ 0,07 % |
| DHT 1 kHz, +4 dBu | 0,0012 %, H3 ≥ H2 | SOS : 0,003 % DHT+B, H3 dominante |
| Écrêtage (3 %) | +28,5 dBu à 1 kHz, +28,0 dBu à 50 Hz | SOS : +28 dBu ; AMS : > +26 dBu |
| Grave, LINE | −3 dB à 6 Hz, +0,5 dB à 15 Hz | SOS : −3 dB vers 6 Hz, +0,5 dB à 15 Hz |
| Grave, MIC | −3 dB vers 6,3 Hz, +0,4 dB à 15 Hz | SOS : −3 dB vers 7 Hz, +0,25 dB |
| Aigu, LINE / MIC | +0,02 dB / +0,5 dB à 20 kHz | AMS : ±0,5 dB ; SOS : résonance MIC de +1,5 dB à 45 kHz |
| MID à fond | ±17,7 dB | AMS : ±18 dB |
| HIGH à fond | +17,8 dB à 20 kHz, +17 dB à 12 kHz | AMS : ±16 dB ; NYAN mesuré : +18 dB à 20 kHz |
| LOW à fond (60 Hz) | +20,1 dB à 40 Hz, creux −1,2 dB à 1 kHz | NYAN mesuré : +20,2 dB, creux −1,2 dB |
| HIGH PASS −3 dB | 39 / 65 / 138 / 274 Hz, 18 dB/oct | gravés 50 / 80 / 160 / 300 Hz |

## Latence (déclarée à l'hôte, vérifiée par le retard de phase à 1 kHz)

| Fréquence | Normale | Qualité haute |
|---|---|---|
| 44,1 / 48 kHz | 4 échantillons (0,08–0,09 ms) | 6 échantillons |
| 88,2 / 96 kHz | 0 | 4 échantillons |

Elle vient seulement des filtres du suréchantillonnage (demi-bande IIR, latence entière). Aucun lookahead.
Le bypass et POWER sont alignés sur cette latence (test : écart nul).

## CPU (un cœur, 20 s stéréo, blocs de 128, EQ actif, HPF 80, MIC 60)

| Fréquence | Normale | Qualité haute |
|---|---|---|
| 44,1 kHz | 2,6 % | 5,2 % |
| 48 kHz | 2,9 % | 5,7 % |
| 96 kHz | 2,8 % | 5,7 % |

Potentiomètres en mouvement permanent (recalcul des réseaux) : 3,0 % à 48 kHz.
Coût par composant (96 kHz, un canal) : transformateur 21 ns, étage de sortie + LO1166 33 ns, réseau HIGH / LOW
12 ns, MID 12 ns, module complet 139 ns par échantillon.

## Repliement (cas extrême : MIC 80, sortie visée +30 dBu, sinus de 9 973 Hz)

Pire raie non harmonique entre 20 Hz et 20 kHz : −55 à −57 dB en qualité normale, −94 à −99 dB en qualité haute.
Aux niveaux habituels (sous +24 dBu), la distorsion reste sous 0,2 % et le repliement est négligeable en ×2.

## Robustesse et tests fonctionnels

- 42 / 42 configurations (22,05 à 192 kHz, blocs de 1 à 2048, réglages au hasard, NaN / inf en entrée) : aucune
  valeur non finie.
- Changements de commutateurs pendant la lecture : fondus de 20 ms, pas de clic (la seule variation mesurée est
  le changement de niveau voulu).
- Plugin : sauvegarde / restauration de l'état, A / B, 7 presets d'usine, automation de tous les paramètres sur
  2 000 blocs de tailles variables, bypass, mono, latence — tout passe.
- Hôte d'essai (chargement, interface, suppression, déchargement, 3 tours) : sans plantage.
- **Pas encore fait : l'essai dans FL Studio** (à faire par Nov).
