#!/bin/sh
# build-env.sh — environnement de build pour les systèmes minimaux sans
# pkg-config ni en-têtes X11/freetype système (voir README, section
# « Options avancées »). À sourcer ou à préfixer à une commande cmake :
#
#   ./scripts/build-env.sh cmake --build build-win
#   . ./scripts/build-env.sh && cmake -S . -B build-win ...
#
# Si build-sysroot/ est absent (système complet), la commande passe sans
# modification.

REPO_DIR=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
SYSROOT="$REPO_DIR/build-sysroot"

if [ -d "$SYSROOT/include" ]; then
    # Le shim pkg-config sert à CMake/JUCE et au configure imbriqué de juceaide.
    PATH="$SYSROOT/bin:$PATH"
    export PATH
    # En-têtes X11/freetype/fontconfig du sysroot + désactivation des
    # extensions X11 optionnelles (chargées via dlopen par JUCE).
    SYSROOT_FLAGS="-isystem $SYSROOT/include -DJUCE_USE_XRANDR=0 -DJUCE_USE_XINERAMA=0 -DJUCE_USE_XSHM=0 -DJUCE_USE_XRENDER=0 -DJUCE_USE_XCURSOR=0"
    CXXFLAGS="$SYSROOT_FLAGS $CXXFLAGS"
    CFLAGS="$SYSROOT_FLAGS $CFLAGS"
    export CXXFLAGS CFLAGS
fi

if [ $# -gt 0 ]; then
    exec "$@"
fi
