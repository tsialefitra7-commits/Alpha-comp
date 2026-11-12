# alpha-comp

Compresseur audio temps réel inspiré du **FabFilter Pro-C 2** : design
épuré anthracite, courbe de transfert interactive en temps réel,
réduction de gain animée à 60 fps.

Architecture **découplée** (même philosophie que le drum pad) :

```
DAW ──audio──▶ Plugin (adaptateur) ──▶ Core (DSP C++ pur) ──▶ DAW
                    ▲                        │
                    │                        │ Meter Bridge (atomiques)
                    └────── UI (JUCE) ◀──────┘
```

- **Core** : 100 % C++ pur, **zéro JUCE**. Testable et compilable partout,
  sans DAW ni framework graphique.
- **UI** : composants JUCE style Pro-C 2. Elle ne fait que *lire* le Core
  (courbe + métriques) et *écrire* des paramètres (APVTS).
- **Plugin** : le pont DAW ↔ Core ↔ UI (VST3 / AU / Standalone via JUCE).

---

## Arborescence

```
alpha-comp/
├── CMakeLists.txt                    # CMake principal (Core, UI, Plugin)
├── Makefile                          # Secours : tests/démo du Core sans CMake
├── .gitignore
│
├── Core/                             # 1. CŒUR AUDIO (C++ pur, zéro JUCE)
│   ├── CMakeLists.txt
│   ├── Include/Core/
│   │   ├── CompressorDSP.h           # Processeur stéréo lié + Meter Bridge
│   │   ├── EnvelopeDetector.h        # Peak / RMS / Lookahead
│   │   └── GainComputer.h            # Courbe de transfert (seuil, ratio, knee)
│   └── Source/
│       ├── CompressorDSP.cpp
│       ├── EnvelopeDetector.cpp
│       └── GainComputer.cpp
│
├── UI/                               # 2. INTERFACE (style Pro-C 2, JUCE)
│   ├── CMakeLists.txt
│   ├── Assets/                       # (vide : polices système, icônes en code)
│   ├── Include/UI/
│   │   ├── ProC2Theme.h              # Design System anthracite + Look&Feel
│   │   ├── CompressionCurve.h        # Courbe de transfert temps réel
│   │   ├── GainReductionMeter.h      # Vumètre GR vertical LED
│   │   └── KnobRotary.h              # Knob précis + valeur exacte
│   └── Source/
│       ├── ProC2Theme.cpp
│       ├── KnobRotary.cpp
│       ├── CompressionCurve.cpp
│       └── GainReductionMeter.cpp
│
├── Plugin/                           # 3. ADAPTATEUR DAW <-> Core <-> UI
│   ├── CMakeLists.txt                # juce_add_plugin (VST3/AU/Standalone)
│   └── Source/
│       ├── PluginProcessor.h/.cpp    # AudioProcessor + APVTS + Core
│       └── PluginEditor.h/.cpp       # Assemblage de l'UI
│
├── Tests/                            # Tests du Core (sans JUCE)
│   └── CoreTests.cpp                 # 47 vérifications, aucun framework
└── Demo/                             # Démo console du Core (sans JUCE)
    └── ConsoleDemo.cpp
```

---

## Le Core DSP

### `GainComputer` — la courbe de transfert

```
over = entrée − seuil
over < −knee/2   :  GR = 0
over > +knee/2   :  GR = over × (1/ratio − 1)          (knee dur)
sinon (knee)     :  GR = (1/ratio − 1) × (over + knee/2)² / (2×knee)
sortie = entrée + GR + makeup
```

Formule continue en valeur **et** en dérivée aux bords du knee.
L'UI dessine la courbe avec *exactement* ces fonctions : ce que l'on
voit est ce que le DSP applique.

### `EnvelopeDetector` — Peak / RMS / Lookahead

- **Peak** : suit `|x|` (one-pole, coefficient attack si le signal monte,
  release s'il descend).
- **RMS** : intègre `x²` (fenêtre ~10 ms) puis `sqrt()`, puis même follower.
  Sur un sinus, RMS ≈ amplitude/√2 < Peak.
- **Lookahead** : ring buffer qui retarde l'audio de `lookaheadMs` pendant
  que la détection voit le signal *non* retardé. La latence introduite
  (`getLookaheadSamples()`) est déclarée au DAW via `setLatencySamples()`.

Coefficients : `a = exp(−1 / (sampleRate × τ))`, τ = temps en secondes.

### `CompressorDSP` — chaîne de traitement (lien stéréo)

Par échantillon : enveloppe liée = max des canaux → GR cible (GainComputer)
→ lissage attack/release du gain → gain appliqué au signal **retardé**
+ makeup → mix parallèle `out = dry×(1−mix) + wet×mix`.

**Meter Bridge thread-safe** : réduction de gain, niveaux d'entrée/sortie
écrits dans des `std::atomic<float>` en fin de bloc. L'UI lit un snapshot
(`getMeterState()`) à 60 fps **sans lock ni allocation** : le thread audio
n'est jamais bloqué.

**Paramètres thread-safe** : stockés en atomiques, relus et appliqués aux
détecteurs au début de chaque bloc sur le thread audio. `setParams()` peut
donc être appelé depuis l'UI sans risque.

### Paramètres

| Paramètre  | Plage        | Défaut | Unité |
|------------|--------------|--------|-------|
| Threshold  | −60 … +20    | −18    | dB    |
| Ratio      | 1 … 20       | 4      | :1    |
| Attack     | 0,01 … 300   | 10     | ms    |
| Release    | 10 … 1000    | 100    | ms    |
| Knee       | 0 … 40       | 12     | dB    |
| Makeup     | −24 … +24    | 0      | dB    |
| Mix        | 0 … 100      | 100    | %     |
| Detector   | Peak / RMS   | Peak   | —     |
| Lookahead  | 0 … 20       | 0      | ms    |
| Bypass     | off/on       | off    | —     |

---

## L'UI (style Pro-C 2)

- **`CompressionCurve`** — la signature du Pro-C 2. Axes entrée/sortie en dB
  (−60 … +20), diagonale 1:1, courbe de compression (remplissage bleu),
  zone de réduction de gain (orange), bande du knee, ligne de seuil avec
  point lumineux, **historique du signal** (points entrée→sortie qui
  s'estompent) et **point live** avec halo. **Glisser la courbe** :
  horizontal = Threshold, vertical = Ratio (via l'APVTS, donc automatisable).
  Timer à ~60 fps sur le Meter Bridge.
- **`GainReductionMeter`** — barre verticale LED 0 … −24 dB
  (vert → orange → rouge), lissage ballistique, marqueur de pic,
  lecture numérique.
- **`KnobRotary`** — arc fin bleu, corps anthracite, **valeur exacte
  affichée sous le knob**, double-clic = valeur par défaut.
- **`ProC2Theme`** — palette anthracite (`#17181a` / `#1f2023`),
  accents bleu `#4da3ff` / orange `#ff9f43`, police fine.

Disposition de l'éditeur (~780×460, redimensionnable) : titre + Detector +
Bypass en haut, courbe + vumètre GR au centre, 8 knobs en bas.

---

## Compilation

### Core seul (sans JUCE, sans DAW) — toujours possible

```bash
# Option A : CMake
cmake -S . -B build -DALPHACOMP_BUILD_PLUGIN=OFF
cmake --build build
ctest --test-dir build --output-on-failure
./build/Demo/alpha_comp_demo

# Option B : Makefile de secours (g++ uniquement)
make test
make demo
```

### Plugin JUCE (VST3 / AU / Standalone)

Il faut JUCE 7 ou 8 (C++20 requis par JUCE 8). Trois façons de le fournir :

```bash
# 1. Dépôt JUCE déjà cloné
cmake -S . -B build -DJUCE_SOURCE_DIR=/chemin/vers/JUCE
cmake --build build --config Release

# 2. Téléchargement automatique (internet requis, clone shallow de JUCE 8.0.0)
cmake -S . -B build -DALPHACOMP_FETCH_JUCE=ON
cmake --build build --config Release

# 3. JUCE installé avec son CMake config (find_package)
cmake -S . -B build -DJUCE_DIR=/chemin/vers/juce-cmake-config
cmake --build build --config Release
```

> Sous Linux, la compilation des modules GUI JUCE requiert aussi les
> en-têtes X11 (`libx11-dev`, `libxrandr-dev`, `libxinerama-dev`,
> `libxcursor-dev`, … selon votre distribution). Sans JUCE, le CMake
> racine construit automatiquement Core/Tests/Demo uniquement.

Les artefacts (`.vst3`, `.component`, Standalone) sont générés dans
`build/Plugin/alpha-comp_artefacts/`.

### Options avancées

```bash
# Formats de plugin sélectionnables (défaut : "VST3 AU Standalone")
cmake -S . -B build -DALPHACOMP_PLUGIN_FORMATS=VST3

# Systèmes minimaux sans pkg-config ni en-têtes X11/freetype système :
# fournir un sysroot (build-sysroot/ par défaut) contenant include/ et lib/
# + un exécutable pkg-config compatible (shim) dans build-sysroot/bin/.
# Voir ALPHACOMP_SYSROOT dans le CMakeLists racine.
cmake -S . -B build -DALPHACOMP_SYSROOT=/chemin/vers/sysroot ...
```

Sous Linux, les extensions X11 optionnelles (Xrandr/Xinerama/XShm/Xrender/
Xcursor) sont désactivables par defines ; JUCE charge X11 via `dlopen`, seuls
les en-têtes de base (`libx11-dev`) sont requis à la compilation.

---

## Tests

`Tests/CoreTests.cpp` — 47 vérifications sans framework externe :

- **GainComputer** : conversions dB, sous/au-dessus du seuil, ratio 1:1,
  makeup, continuité et monotonie du knee doux.
- **EnvelopeDetector** : convergence Peak, attack rapide vs lente,
  RMS ≈ amplitude/√2, lookahead (impulsion retardée de exactement
  480 échantillons à 10 ms / 48 kHz), silence.
- **CompressorDSP** : silence, signal fort/faible, makeup, mix 0 %,
  bypass, lien stéréo (un canal fort réduit l'autre), courbe statique
  == GainComputer, sanitization des paramètres, latence lookahead.

```bash
make test        # ou : ctest --test-dir build
```

## Démo console

`Demo/ConsoleDemo.cpp` compresse un sinus 440 Hz et affiche RMS
entrée/sortie, GR finale et la courbe de transfert — pour valider le
Core sans DAW :

```bash
make demo        # ou : ./build/Demo/alpha_comp_demo
```

---

## Notes d'implémentation

- Le Core est **mono-thread audio** : `processBlock()` ne doit pas être
  appelé simultanément depuis plusieurs threads. Les setters de paramètres
  et `getMeterState()` sont thread-safe.
- Avec `mix < 100 %`, le point live de la courbe représente le chemin
  *wet* (enveloppe + GR + makeup), pas le blend parallèle.
- Changer le lookahead pendant la lecture change la latence déclarée
  seulement au prochain `prepareToPlay()` (comportement standard JUCE).
- `numChannels` doit être ≤ `maxChannels` passé à `prepare()` ; les
  canaux supplémentaires éventuels sont ignorés.
