cmake_minimum_required(VERSION 3.21)
project(RyxusPlayer VERSION 1.2.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

if(MSVC)
    add_compile_options(/utf-8 /MP /permissive- /W3)
    add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN UNICODE _UNICODE
                            _CRT_SECURE_NO_WARNINGS)
endif()

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)
find_package(taglib CONFIG REQUIRED)

qt_standard_project_setup()

set(RYXUS_SOURCES
    src/main.cpp

    src/core/Effects.h              src/core/Effects.cpp
    src/core/Equalizer.h            src/core/Equalizer.cpp
    src/core/Lang.h                 src/core/Lang.cpp
    src/core/AudioEngine.h          src/core/AudioEngine.cpp
    src/core/TrackInfo.h            src/core/TrackInfo.cpp
    src/core/UiScale.h              src/core/UiScale.cpp
    src/core/MetadataService.h      src/core/MetadataService.cpp
    src/core/PlaylistModel.h        src/core/PlaylistModel.cpp
    src/core/WaveformWorker.h       src/core/WaveformWorker.cpp
    src/core/LibraryScanner.h       src/core/LibraryScanner.cpp
    src/core/Settings.h             src/core/Settings.cpp
    src/core/ShellUtils.h           src/core/ShellUtils.cpp
    src/core/SpectrumAnalyzer.h     src/core/SpectrumAnalyzer.cpp

    src/ui/Theme.h                  src/ui/Theme.cpp
    src/ui/Icons.h                  src/ui/Icons.cpp
    src/ui/BackgroundHost.h         src/ui/BackgroundHost.cpp
    src/ui/MainWindow.h             src/ui/MainWindow.cpp
    src/ui/TitleBar.h               src/ui/TitleBar.cpp
    src/ui/NowPlayingPanel.h        src/ui/NowPlayingPanel.cpp
    src/ui/CoverArtView.h           src/ui/CoverArtView.cpp
    src/ui/CoverCropDialog.h        src/ui/CoverCropDialog.cpp
    src/ui/MetadataEditor.h         src/ui/MetadataEditor.cpp
    src/ui/NowPlayingHeader.h       src/ui/NowPlayingHeader.cpp
    src/ui/RatingBar.h              src/ui/RatingBar.cpp
    src/ui/TrackDetails.h           src/ui/TrackDetails.cpp
    src/ui/Visualizer.h             src/ui/Visualizer.cpp
    src/ui/LibraryPanel.h           src/ui/LibraryPanel.cpp
    src/ui/FileListPanel.h          src/ui/FileListPanel.cpp
    src/ui/PlaylistPanel.h          src/ui/PlaylistPanel.cpp
    src/ui/PlaylistDelegate.h       src/ui/PlaylistDelegate.cpp
    src/ui/SeekBar.h                src/ui/SeekBar.cpp
    src/ui/PlayerBar.h              src/ui/PlayerBar.cpp
    src/ui/EffectsRack.h            src/ui/EffectsRack.cpp
    src/ui/EqCurveEditor.h          src/ui/EqCurveEditor.cpp
    src/ui/EqualizerPanel.h         src/ui/EqualizerPanel.cpp
    src/ui/EqSlider.h               src/ui/EqSlider.cpp
    src/ui/FlatButton.h             src/ui/FlatButton.cpp
    src/ui/RibbonTabBar.h           src/ui/RibbonTabBar.cpp
    src/ui/SettingsDialog.h         src/ui/SettingsDialog.cpp

    third_party/miniaudio_impl.cpp
)

# Recurso de Windows: icono del ejecutable y metadatos de version.
if(WIN32)
    list(APPEND RYXUS_SOURCES res/ryxus.rc)
endif()

qt_add_executable(RyxusPlayer WIN32 ${RYXUS_SOURCES})

# La hoja de estilos se empaqueta con qt_add_resources y NO dejandola caer en
# la lista de fuentes: qt_standard_project_setup() activa AUTOMOC y AUTOUIC,
# pero no AUTORCC, asi que un .qrc suelto se compilaria... nunca. Ademas asi
# CMake sabe que editar el .qss obliga a reconstruir el recurso.
qt_add_resources(RyxusPlayer "ryxus_assets"
    PREFIX "/res"
    BASE   "res"
    FILES  res/ryxus.qss
           res/ryxus.ico)

target_include_directories(RyxusPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

# La version sale del propio proyecto: escrita a mano en el codigo se quedaba
# atras en cada release.
target_compile_definitions(RyxusPlayer PRIVATE
    RYXUS_VERSION="${PROJECT_VERSION}")

target_link_libraries(RyxusPlayer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# API del shell (SHOpenFolderAndSelectItems) para el boton "Ubicacion".
if(WIN32)
    target_link_libraries(RyxusPlayer PRIVATE shell32 ole32)
endif()

# TagLib exports different target names across 1.x / 2.x packagings.
if(TARGET TagLib::tag)
    target_link_libraries(RyxusPlayer PRIVATE TagLib::tag)
elseif(TARGET TagLib::TagLib)
    target_link_libraries(RyxusPlayer PRIVATE TagLib::TagLib)
elseif(TARGET tag)
    target_link_libraries(RyxusPlayer PRIVATE tag)
else()
    message(FATAL_ERROR "TagLib fue encontrado pero no expone un target conocido")
endif()

set_target_properties(RyxusPlayer PROPERTIES
    OUTPUT_NAME "RyxusPlayer"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- despliegue de DLLs de Qt junto al ejecutable -------------------------
if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${_qt_bin}")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET RyxusPlayer POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-system-d3d-compiler
                    --no-opengl-sw --no-compiler-runtime
                    "$<TARGET_FILE:RyxusPlayer>"
            COMMENT "windeployqt -> bin/")
    endif()
endif()

# ===========================================================================
#  Instalacion y empaquetado
# ===========================================================================
#
# `cmake --install build --prefix <dir>` deja un arbol completo y autonomo:
# ejecutable, bibliotecas de Qt, complementos, runtime de MSVC y licencias.
# El instalador de Inno Setup empaqueta ese arbol tal cual.

# Se instala el contenido de bin/, que windeployqt ya dejo completo tras la
# compilacion (ejecutable + Qt + complementos).
#
# No se usa qt_generate_deploy_app_script(): el script que genera Qt 6.8 no
# entrecomilla la ruta del ejecutable, asi que falla si el proyecto vive en una
# carpeta con espacios. Se excluyen los compiladores de shaders de Direct3D,
# que windeployqt copia por dependencia de Qt6Gui y una aplicacion de QtWidgets
# no llega a usar (15 MB).
install(DIRECTORY "${CMAKE_BINARY_DIR}/bin/"
        DESTINATION .
        USE_SOURCE_PERMISSIONS
        PATTERN "dxcompiler.dll" EXCLUDE
        PATTERN "dxil.dll" EXCLUDE)

install(FILES
        "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE"
        "${CMAKE_CURRENT_SOURCE_DIR}/THIRD-PARTY-NOTICES.md"
        "${CMAKE_CURRENT_SOURCE_DIR}/README.md"
        DESTINATION .)

# Runtime de MSVC junto al ejecutable: asi el instalador no depende de que la
# maquina tenga el redistribuible de Visual C++.
if(MSVC)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_DESTINATION .)
    set(CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS_SKIP TRUE)
    include(InstallRequiredSystemLibraries)
    install(PROGRAMS ${CMAKE_INSTALL_SYSTEM_RUNTIME_LIBS} DESTINATION .)
endif()

# --- instalador de Windows (Inno Setup) ------------------------------------
#
# Se genera el script .iss desde la plantilla con la version y las rutas
# reales, y `cmake --build build --target installer` lo compila.
if(WIN32)
    find_program(ISCC_EXE ISCC
        HINTS "$ENV{LOCALAPPDATA}/Programs/Inno Setup 6"
              "$ENV{ProgramFiles\(x86\)}/Inno Setup 6"
              "$ENV{ProgramFiles}/Inno Setup 6")

    set(RYXUS_STAGE_DIR "${CMAKE_BINARY_DIR}/stage")
    set(RYXUS_INSTALLER_DIR "${CMAKE_BINARY_DIR}/installer")

    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/installer/RyxusPlayer.iss.in"
                   "${CMAKE_BINARY_DIR}/RyxusPlayer.iss" @ONLY)

    if(ISCC_EXE)
        add_custom_target(installer
            COMMAND ${CMAKE_COMMAND} -E rm -rf "${RYXUS_STAGE_DIR}"
            COMMAND ${CMAKE_COMMAND} --install "${CMAKE_BINARY_DIR}"
                                     --prefix "${RYXUS_STAGE_DIR}"
                                     --config $<CONFIG>
            COMMAND "${ISCC_EXE}" /Q "${CMAKE_BINARY_DIR}/RyxusPlayer.iss"
            DEPENDS RyxusPlayer
            COMMENT "Construyendo el instalador de Windows"
            VERBATIM)
    else()
        message(STATUS "Inno Setup no encontrado: el target 'installer' no estara disponible")
    endif()
endif()
