# ONIRIQUE N73 — le modèle, ses sources, ses limites

Émulation **techniquement motivée** du préampli-égaliseur Neve 1073 (version de référence : 1073 classique,
schéma EH10023 de 1973, et sa réédition AMS Neve). **Elle n'est pas validée contre un 1073 réel** : aucune mesure
du matériel d'origine n'a été faite pour ce projet. Les comparaisons ci-dessous se font avec les caractéristiques
publiées et des mesures publiées par des tiers.

## Sources

| Niveau | Source | Ce qui en est tiré |
|---|---|---|
| Officielle | Schémas Neve « 1073 fullpak » (EH10023 module, EK20033 commutateur, EX10283 BA283, BA284, D10019 B182, D10042 B205, D10048 B211), archive.org | topologie complète, ordre des étages, gains de la BA283 NV, rôle de la BA284, réseaux d'EQ et valeurs des condensateurs, source 1k2 et charge 5k1 du passe-haut, réseau 1k5 + 10 nF au secondaire du LO1166 |
| Officielle | Manuel AMS Neve 1073 / 1084, issue 5.1 | graduations de la façade, plages de gain (micro 20–80, ligne −10…+20), sortie > +26 dBu sur 600 Ω, DHT ≤ 0,07 % de 50 Hz à 10 kHz à +20 dBu, ±0,5 dB 20 Hz–20 kHz, contrôle de sortie après l'EQ |
| Mesures tierces | Sound On Sound, essai du 1073N (H. Robjohns) | écrêtage à +28 dBu, DHT+B 0,003 % à +4 dBu (H3 dominante), −3 dB vers 6–7 Hz avec bosse de +0,5 dB à 15 Hz, résonance d'entrée +1,5 dB à 45 kHz, impédances micro mesurées (1,3 kΩ / 910 Ω / 300 Ω), étage en plus au-delà de 50 dB |
| Plugin de référence | Manuel UAD « Neve 1073 Preamp & EQ » | fonctionnement des commandes : gains MIC et LINE séparés (clic sur MIC / LINE), OFF qui coupe le traitement, PAD et MIC Z réservés à l'entrée micro, impédances MIC Z (HI : 1,2 kΩ de 20 à 50, 600 Ω de 55 à 80 ; LO : 300 Ω), LEVEL (fader) avant l'ampli de sortie, OUTPUT propre |
| Secondaire | Projet NYAN-1073 (greyboxaudio, CC BY-SA 4.0) | inductances des bobines (fiches Sowter), reconstitution du réseau HIGH / LOW, mesures de leur circuit (contrôle de notre simulation, écart < 1 dB) |
| Secondaire | GroupDIY (I. Bell) | 10468 : 1:2 (HI) / 1:4 (LO), secondaire chargé par 4k8 |
| Secondaire | Carnhill (VTB1148 = LO1166) | transformateur de sortie à entrefer, 1+1:1,7+1,7 |
| Repères seulement | Braingasm Lab, comparatif de clones | tendances : distorsion qui monte dans le grave, H2 en 1/f (non utilisées comme cibles : clones, niveau inconnu, mesures à travers une DI) |

## La chaîne simulée (à 88,2 / 96 kHz au moins)

1. **Entrée** — MIC : transformateur 10468 (rapport 1:2 ou 1:4 selon MIC Z, charge du manuel UAD : 1,2 kΩ / 600 Ω
   en HI selon le gain, 300 Ω en LO ; SOS a mesuré 1,3 kΩ / 910 Ω sur un 1073N), PAD −20 dB ;
   LINE : 31267, 10 kΩ (ni PAD ni MIC Z, comme sur le plugin UAD). Modèle : inductance magnétisante non linéaire vue par la résistance de source
   (`dsp/Transformer.h`) — coupure grave et distorsion du grave qui découlent du flux.
2. **Commutateur EK20033** — 22 positions dans l'ordre de la façade. De −80 à −55 : ampli d'entrée BA284 en plus ;
   −50 / −45 : gain de la BA283 NV relevé par 120 Ω / 330 Ω (28,4 / 23,3 dB, calculés depuis le schéma) ;
   −35 à −20 : atténuateur avant la BA283 ; LINE : atténuateur. Comme sur le plugin UAD, LINE (positions 0 à 7, avec
   son OFF) et MIC (8 à 21, avec le sien) sont deux plages : le bouton ne passe pas de l'une à l'autre, un clic sur MIC
   ou LINE change d'entrée et reprend la dernière position de cette entrée (mémorisée avec le projet). Sur OFF, le
   traitement s'arrête et le son passe tel quel (plugin UAD ; le matériel, lui, coupe le son).
3. **BA283 NV** — gain 1 + 2k2 / (390 ∥ 1k8 ∥ R_T). Courbe statique (`dsp/AmpStage.h`) : courbure H2 / H3 et butées
   dissymétriques de l'émetteur suiveur TR6.
4. **EQ** — réseaux HIGH / LOW (B205) et présence (B211) **simulés comme circuits** (analyse nodale, transformée
   bilinéaire précompensée, `dsp/LinearCircuit.h`, `dsp/EqNetworks.h`) autour des amplis de la BA284 ;
   les potentiomètres agissent sur les vraies résistances, d'où les formes qui changent avec le réglage.
5. **HIGH PASS** — réseau passif C–L–C (B182, bobine T1295), source 1k2, charge 5k1 : simulé comme circuit.
6. **Fader de console** — le LEVEL du plugin UAD : avant l'ampli de sortie, de −∞ à +10 dB, gain unité à 0 ;
   au-dessus de 0 dB, il pousse l'ampli de sortie (LINE 0 à +18 dBu : 0,05 % à 0 dB, 0,8 % à +5, 10 % à +10).
7. **BA283 AM + LO1166** (`dsp/OutputStage.h`) — 2N3055 en classe A à un transistor : butée de blocage quand le
   courant demandé (charge + courant magnétisant) atteint le courant de repos, plus tôt dans le grave ; saturation de
   l'autre côté ; transformateur à entrefer polarisé (harmoniques paires dans le grave).
8. **Réponse** — coupure grave du second ordre calée sur SOS (bosse de 15 Hz), résonance du 10468 en MIC, PHASE.
9. Puis, propres et hors module : OUTPUT (volume, ±24 dB côté plugin UAD, −24 … +12 ici, sans effet sur la
   saturation : le gain règle le grain, OUTPUT le niveau), LEVEL MATCH, POWER.

## Valeurs estimées (non mesurées sur un 1073)

| Valeur | Estimation | Comment |
|---|---|---|
| Source du micro | 150 Ω | convention ; règle l'écart HI / LO (≈ 3,5 dB) et la coupure grave |
| Inductances magnétisantes | 10468 : 10 H (HI), 31267 : 20 H, LO1166 : 15 H (secondaire) | coupures sous 3 Hz, la forme du grave vient du filtre calé sur SOS |
| Flux de saturation | 10468 : 0,06 V·s ; 31267 : 0,12 ; LO1166 : 0,2 (+0,07 continu) | calés pour rester ≤ 0,07 % à 50 Hz / +20 dBu |
| Courbure des étages | H2 / H3 à +20 dBu : sortie 0,003 % / 0,03 % | calé sur la fiche AMS et la mesure SOS (H3 dominante) |
| Butées | BA283 NV +10,5 / −7,5 V ; sortie +28 dBu (blocage), saturation 15 % plus haut | SOS (+28 dBu) et schéma |
| Résistances de butée des potentiomètres | HIGH 60 Ω, LOW 900 Ω, MID 180 Ω | MID ±17,7 dB (fiche : ±18), LOW / HIGH comme les mesures NYAN |
| « 0 » des boutons HIGH / LOW | positions 0,478 / 0,512 | position la plus plate du réseau (±0,4 dB) |
| Charge de sortie | 600 Ω | conditions de la fiche AMS |
| Câblage de la broche L de la B205 | celui de NYAN | non lisible avec certitude sur le scan |

## Limites connues

- **Pas de validation contre le matériel.** Avec un 1073 (ou des enregistrements de référence), le protocole du banc
  (`Tests/N73Bench.cpp`) permet de mesurer les écarts : réponses, harmoniques selon le niveau, écrêtage.
- Étages d'amplification : courbes statiques calées, pas une simulation au transistor près ; leur distorsion ne
  dépend pas du gain choisi (simplification). Amplis de l'EQ idéaux pour la partie linéaire.
- Transformateurs : pas d'hystérésis ; inductances de fuite et capacités résumées par des filtres.
- Pas de bruit de fond (choix : le cahier des charges exclut tout bruit artificiel).
- EQ en circuit à plat : −0,4 dB à 1 kHz et ±0,4 dB de 20 Hz à 20 kHz — propriété du réseau, conservée.
- Écart possible entre les unités réelles (tolérances, révisions) : non représenté.

## Commandes modernes (hors 1073)

PAD (présentation UA, entrée micro seulement), fader de console, POWER, OUTPUT, CAL (0 dBFS = +18 … +26 dBu, +26 par défaut : l'alignement
prêté aux plugins UAD), LEVEL MATCH (le volume de sortie suit celui de l'entrée, moyennes sur 300 ms, ±30 dB),
qualité haute (×4).

**Convention de niveau** : le signal du DAW est de niveau ligne ; en MIC il entre tel quel dans l'entrée micro, comme
une source ligne branchée sur un vrai préampli micro (et comme sur le plugin UAD). Avec une voix à −18 dBFS : LINE 0
à +10 propres, LINE +15 ≈ 1 % de distorsion, LINE +20 et MIC 20 ≈ 10 %, MIC 30 ≈ 35 % (étages saturés en cascade :
BA284, BA283, sortie). Mesure : `N73Bench drive`.
