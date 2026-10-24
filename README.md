# ONIRIQUE N73

Préampli-égaliseur d'après le Neve 1073 (VST3 Windows, AU Mac). Le modèle, ses sources et ses limites :
`docs/MODELE.md`. Les mesures, le CPU et la latence : `docs/MESURES.md`. Le suivi du projet : `docs/SUIVI.md`.

## Compiler (Windows, Visual Studio 2022, CMake ≥ 3.22)

```
cmake -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release --target OniriqueN73_VST3
```

Le VST3 : `build\OniriqueN73_artefacts\Release\VST3\ONIRIQUE N73.vst3`.

- `-DN73_LICENCE=OFF` : build d'essai local sans la licence ONIRIQUE (sans bruit de démo). **Jamais publié.**
- `-DFETCHCONTENT_SOURCE_DIR_JUCE=<dossier>` : réutilise un JUCE 8.0.6 déjà téléchargé.
- Les images des boutons (`Resources/knobs`) se refont avec `tools\render-knobs.ps1`.

## Tests

```
cmake -B build-bench -DN73_PLUGIN=OFF && cmake --build build-bench --config Release --target N73Bench
build-bench\N73Bench_artefacts\Release\N73Bench.exe all
cmake --build build --config Release --target N73PluginTest
build\N73PluginTest_artefacts\Release\N73PluginTest.exe
```

## Installer pour un essai

Copier le dossier `ONIRIQUE N73.vst3` dans `C:\Program Files\Common Files\VST3`, puis relancer l'analyse des
plugins dans FL Studio (Options → Manage plugins → Find more plugins).
