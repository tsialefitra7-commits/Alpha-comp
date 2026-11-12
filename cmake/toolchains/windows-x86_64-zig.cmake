# Toolchain de cross-compilation Windows x86_64 (VST3) via Zig.
#
# Zig (paquet pypi « ziglang » ou binaire zig dans le PATH) fournit un
# compilateur clang avec le sysroot mingw-w64 embarqué, plus un compilateur
# de ressources (`zig rc`, compatible Microsoft RC) — aucun MinGW système
# n'est requis.
#
# Usage :
#   cmake -S . -B build-win \
#     -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/windows-x86_64-zig.cmake \
#     -DALPHACOMP_PLUGIN_FORMATS=VST3 \
#     -DALPHACOMP_FETCH_JUCE=ON
#   cmake --build build-win
#
# Requis : pip install ziglang (ou zig dans le PATH).

set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)

set(_alphacomp_zig_bin "${CMAKE_CURRENT_LIST_DIR}/bin")

set(CMAKE_C_COMPILER   "${_alphacomp_zig_bin}/zig-cc"     CACHE FILEPATH "" FORCE)
set(CMAKE_CXX_COMPILER "${_alphacomp_zig_bin}/zig-cxx"    CACHE FILEPATH "" FORCE)
# Le shim se nomme « windres » pour que CMake utilise la règle
# Platform/Windows-windres ; il traduit vers `zig rc`.
set(CMAKE_RC_COMPILER  "${_alphacomp_zig_bin}/windres"    CACHE FILEPATH "" FORCE)
set(CMAKE_AR           "${_alphacomp_zig_bin}/zig-ar"     CACHE FILEPATH "" FORCE)
set(CMAKE_RANLIB       "${_alphacomp_zig_bin}/zig-ranlib" CACHE FILEPATH "" FORCE)

# Les exécutables Windows ne tournent pas sur l'hôte de compilation.
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE BOTH)
