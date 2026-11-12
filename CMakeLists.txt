cmake_minimum_required(VERSION 3.20)

project(alpha-comp
    VERSION 1.0.0
    DESCRIPTION "alpha-comp — Compresseur inspiré du FabFilter Pro-C 2 (Core C++ pur + UI JUCE + Plugin VST3/AU)"
    LANGUAGES CXX
)

# C++20 : requis par JUCE 8, compatible avec tout le code du Core (C++17).
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Les bibliothèques statiques (Core, UI) sont liées dans un module partagé
# (VST3/.so) : PIC obligatoire.
set(CMAKE_POSITION_INDEPENDENT_CODE ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(ALPHACOMP_BUILD_TESTS  "Construire les tests du Core" ON)
option(ALPHACOMP_BUILD_DEMO   "Construire la démo console du Core" ON)
option(ALPHACOMP_BUILD_PLUGIN "Construire le plugin JUCE (VST3/AU/Standalone)" ON)
option(ALPHACOMP_FETCH_JUCE   "Télécharger JUCE via FetchContent si introuvable (nécessite internet)" OFF)

# ----------------------------------------------------------------------------
# Core : toujours construit (zéro dépendance externe).
# ----------------------------------------------------------------------------
add_subdirectory(Core)

if(ALPHACOMP_BUILD_TESTS)
    enable_testing()
    add_subdirectory(Tests)
endif()

if(ALPHACOMP_BUILD_DEMO)
    add_subdirectory(Demo)
endif()

# ----------------------------------------------------------------------------
# UI + Plugin : nécessitent JUCE.
#   1. -DJUCE_SOURCE_DIR=/chemin/vers/JUCE  (dépôt JUCE cloné)
#   2. -DALPHACOMP_FETCH_JUCE=ON            (clone automatique depuis GitHub)
#   3. JUCE installé avec son CMake config  (find_package(JUCE))
# Sans JUCE, seuls Core/Tests/Demo sont construits : le projet reste
# compilable et testable partout (voir aussi le Makefile de secours).
# ----------------------------------------------------------------------------
if(ALPHACOMP_BUILD_PLUGIN)
    set(ALPHACOMP_HAS_JUCE OFF)

    # ------------------------------------------------------------------------
    # Environnement Linux minimal (sans pkg-config ni en-têtes X11 système) :
    # un sysroot d'en-têtes + bibliothèques peut être fourni via
    # ALPHACOMP_SYSROOT (défaut : build-sysroot/, voir README).
    # ------------------------------------------------------------------------
    set(ALPHACOMP_SYSROOT "${CMAKE_CURRENT_SOURCE_DIR}/build-sysroot"
        CACHE PATH "Sysroot d'en-têtes et de bibliothèques (X11, freetype2, fontconfig)")

    if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
        # X11 : JUCE charge les symboles X11 via dlopen — seuls les en-têtes de
        # base sont requis à la compilation. Les extensions optionnelles sont
        # désactivées (inutiles pour un plugin VST3).
        add_compile_definitions(
            JUCE_USE_XRANDR=0
            JUCE_USE_XINERAMA=0
            JUCE_USE_XSHM=0
            JUCE_USE_XRENDER=0
            JUCE_USE_XCURSOR=0
            JUCE_WEB_BROWSER=0
            JUCE_USE_CURL=0
            JUCE_VST3_CAN_REPLACE_VST2=0
        )
        if(EXISTS "${ALPHACOMP_SYSROOT}/include")
            include_directories(SYSTEM "${ALPHACOMP_SYSROOT}/include")
        endif()

        # JUCE exige pkg-config (find_package(PkgConfig REQUIRED)) pour créer
        # les cibles juce::pkgconfig_<nom>. _juce_create_pkgconfig_target()
        # retourne immédiatement si la cible existe déjà : on pré-créé donc ces
        # cibles avec les entrées du sysroot, ce qui dispense de pkg-config.
        macro(alphacomp_precreate_pkgconfig_target name)
            if(NOT TARGET juce::pkgconfig_${name})
                add_library(alphacomp_pkgconfig_${name} INTERFACE)
                add_library(juce::pkgconfig_${name} ALIAS alphacomp_pkgconfig_${name})
            endif()
        endmacro()
        alphacomp_precreate_pkgconfig_target(JUCE_CURL_LINUX_DEPS)
        alphacomp_precreate_pkgconfig_target(JUCE_BROWSER_LINUX_DEPS)
        alphacomp_precreate_pkgconfig_target(juce_audio_devices_LINUX_DEPS)
        alphacomp_precreate_pkgconfig_target(juce_graphics_LINUX_DEPS)
        alphacomp_precreate_pkgconfig_target(juce_opengl_LINUX_DEPS)
        if(EXISTS "${ALPHACOMP_SYSROOT}/lib/libfreetype.so")
            target_link_libraries(alphacomp_pkgconfig_juce_graphics_LINUX_DEPS INTERFACE
                "${ALPHACOMP_SYSROOT}/lib/libfreetype.so"
                "${ALPHACOMP_SYSROOT}/lib/libfontconfig.so")
        endif()
    endif()

    # Les modules JUCE sont des bibliothèques INTERFACE dont les sources sont
    # recompilées par chaque consommateur. Ce helper crée un « jumeau » sans
    # sources (en-têtes, définitions, dépendances uniquement) afin que
    # alpha_comp_ui n'en compile qu'une copie, celle de la cible plugin.
    function(alphacomp_juce_headers_only juce_name out_var)
        string(REGEX REPLACE "^juce::" "" base "${juce_name}")
        set(twin "alphacomp_hdr_${base}")
        if(NOT TARGET ${twin})
            add_library(${twin} INTERFACE)
            foreach(prop
                    INTERFACE_INCLUDE_DIRECTORIES
                    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES
                    INTERFACE_COMPILE_DEFINITIONS
                    INTERFACE_COMPILE_FEATURES
                    INTERFACE_COMPILE_OPTIONS)
                get_target_property(val ${juce_name} ${prop})
                if(val)
                    set_target_properties(${twin} PROPERTIES ${prop} "${val}")
                endif()
            endforeach()
            get_target_property(libs ${juce_name} INTERFACE_LINK_LIBRARIES)
            set(newlibs "")
            if(libs)
                foreach(dep IN LISTS libs)
                    # Un module JUCE est une cible portant la propriété
                    # INTERFACE_JUCE_MODULE_SOURCES (avec ou sans préfixe juce::).
                    set(is_juce_module OFF)
                    if(TARGET ${dep})
                        get_target_property(modsrcs ${dep} INTERFACE_JUCE_MODULE_SOURCES)
                        if(modsrcs)
                            set(is_juce_module ON)
                        endif()
                    endif()
                    if(is_juce_module)
                        alphacomp_juce_headers_only(${dep} deptwin)
                        list(APPEND newlibs ${deptwin})
                    else()
                        list(APPEND newlibs ${dep})
                    endif()
                endforeach()
            endif()
            if(newlibs)
                set_target_properties(${twin} PROPERTIES INTERFACE_LINK_LIBRARIES "${newlibs}")
            endif()
        endif()
        set(${out_var} ${twin} PARENT_SCOPE)
    endfunction()

    if(DEFINED JUCE_SOURCE_DIR)
        message(STATUS "alpha-comp: JUCE depuis JUCE_SOURCE_DIR=${JUCE_SOURCE_DIR}")
        add_subdirectory(${JUCE_SOURCE_DIR} ${CMAKE_BINARY_DIR}/juce)
        set(ALPHACOMP_HAS_JUCE ON)
    elseif(ALPHACOMP_FETCH_JUCE)
        include(FetchContent)
        message(STATUS "alpha-comp: téléchargement de JUCE via FetchContent…")
        FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG 8.0.0
            GIT_SHALLOW TRUE
        )
        FetchContent_MakeAvailable(JUCE)
        set(ALPHACOMP_HAS_JUCE ON)
    else()
        find_package(JUCE QUIET)
        if(TARGET juce::juce_core)
            message(STATUS "alpha-comp: JUCE trouvé via find_package")
            set(ALPHACOMP_HAS_JUCE ON)
        endif()
    endif()

    if(ALPHACOMP_HAS_JUCE)
        add_subdirectory(UI)
        add_subdirectory(Plugin)
    else()
        message(STATUS "alpha-comp: JUCE introuvable — construction Core/Tests/Demo uniquement.")
        message(STATUS "  Pour le plugin : -DJUCE_SOURCE_DIR=<juce> ou -DALPHACOMP_FETCH_JUCE=ON")
    endif()
endif()
