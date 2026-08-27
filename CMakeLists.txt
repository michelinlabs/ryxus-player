cmake_minimum_required(VERSION 3.21)
project(RoxasPlayer VERSION 1.0.0 LANGUAGES CXX)

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

    src/core/Equalizer.h            src/core/Equalizer.cpp
    src/core/Lang.h                 src/core/Lang.cpp
    src/core/AudioEngine.h          src/core/AudioEngine.cpp
    src/core/TrackInfo.h            src/core/TrackInfo.cpp
    src/core/MetadataService.h      src/core/MetadataService.cpp
    src/core/PlaylistModel.h        src/core/PlaylistModel.cpp
    src/core/WaveformWorker.h       src/core/WaveformWorker.cpp
    src/core/LibraryScanner.h       src/core/LibraryScanner.cpp
    src/core/Settings.h             src/core/Settings.cpp
    src/core/SpectrumAnalyzer.h     src/core/SpectrumAnalyzer.cpp

    src/ui/Theme.h                  src/ui/Theme.cpp
    src/ui/Icons.h                  src/ui/Icons.cpp
    src/ui/BackgroundHost.h         src/ui/BackgroundHost.cpp
    src/ui/MainWindow.h             src/ui/MainWindow.cpp
    src/ui/TitleBar.h               src/ui/TitleBar.cpp
    src/ui/NowPlayingPanel.h        src/ui/NowPlayingPanel.cpp
    src/ui/CoverArtView.h           src/ui/CoverArtView.cpp
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
    src/ui/EqCurveEditor.h          src/ui/EqCurveEditor.cpp
    src/ui/EqualizerPanel.h         src/ui/EqualizerPanel.cpp
    src/ui/EqSlider.h               src/ui/EqSlider.cpp
    src/ui/FlatButton.h             src/ui/FlatButton.cpp
    src/ui/RibbonTabBar.h           src/ui/RibbonTabBar.cpp
    src/ui/SettingsDialog.h         src/ui/SettingsDialog.cpp

    third_party/miniaudio_impl.cpp
)

qt_add_executable(RoxasPlayer WIN32 ${RYXUS_SOURCES})

# La hoja de estilos se empaqueta con qt_add_resources y NO dejandola caer en
# la lista de fuentes: qt_standard_project_setup() activa AUTOMOC y AUTOUIC,
# pero no AUTORCC, asi que un .qrc suelto se compilaria... nunca. Ademas asi
# CMake sabe que editar el .qss obliga a reconstruir el recurso.
qt_add_resources(RoxasPlayer "roxas_assets"
    PREFIX "/res"
    BASE   "res"
    FILES  res/roxas.qss)

target_include_directories(RoxasPlayer PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party)

target_link_libraries(RoxasPlayer PRIVATE Qt6::Core Qt6::Gui Qt6::Widgets)

# TagLib exports different target names across 1.x / 2.x packagings.
if(TARGET TagLib::tag)
    target_link_libraries(RoxasPlayer PRIVATE TagLib::tag)
elseif(TARGET TagLib::TagLib)
    target_link_libraries(RoxasPlayer PRIVATE TagLib::TagLib)
elseif(TARGET tag)
    target_link_libraries(RoxasPlayer PRIVATE tag)
else()
    message(FATAL_ERROR "TagLib fue encontrado pero no expone un target conocido")
endif()

set_target_properties(RoxasPlayer PROPERTIES
    OUTPUT_NAME "RoxasPlayer"
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# --- despliegue de DLLs de Qt junto al ejecutable -------------------------
if(WIN32)
    get_target_property(_qt_qmake Qt6::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin "${_qt_qmake}" DIRECTORY)
    find_program(WINDEPLOYQT_EXE windeployqt HINTS "${_qt_bin}")
    if(WINDEPLOYQT_EXE)
        add_custom_command(TARGET RoxasPlayer POST_BUILD
            COMMAND "${WINDEPLOYQT_EXE}" --no-translations --no-system-d3d-compiler
                    --no-opengl-sw --no-compiler-runtime
                    "$<TARGET_FILE:RoxasPlayer>"
            COMMENT "windeployqt -> bin/")
    endif()
endif()
