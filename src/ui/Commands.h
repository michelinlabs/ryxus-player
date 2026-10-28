#pragma once

#include <QKeySequence>
#include <QString>

// Ordenes de la aplicacion que se pueden lanzar con el teclado.
//
// Aqui solo estan el nombre, el grupo y el atajo de cada una; lo que hacen lo
// decide MainWindow::runCommand(). Asi la ventana de Configuracion puede
// listarlas y dejar cambiar sus atajos sin conocer la ventana principal, y el
// menu muestra el atajo vigente de cada opcion.
namespace Commands {

enum Id {
    // --- reproduccion ------------------------------------------------------
    PlayPause = 0,
    Stop,
    Next,
    Previous,
    SeekForward,
    SeekBackward,
    VolumeUp,
    VolumeDown,
    Mute,
    Shuffle,
    Repeat,
    AbLoop,

    // --- archivos ----------------------------------------------------------
    OpenFiles,
    OpenFolder,
    EditTags,

    // --- ecualizador -------------------------------------------------------
    EqualizerPanel,
    EqualizerOnOff,

    // --- paneles -----------------------------------------------------------
    PanelNowPlaying,
    PanelFiles,
    PanelPlaylist,
    ShowAllPanels,
    HideAllPanels,

    // --- disposiciones -----------------------------------------------------
    NextLayout,
    PreviousLayout,
    LayoutClassic,
    LayoutLibrary,
    LayoutListening,
    LayoutStudio,
    SaveLayout,

    // --- fondo -------------------------------------------------------------
    ChooseBackground,
    ClearBackground,
    NextVisualization,

    // --- video -------------------------------------------------------------
    VideoCinema,
    VideoWithPlaylist,
    VideoWithEqualizer,
    VideoStudio,
    NextVideoMask,
    VideoFullscreen,
    AddAudioTrack,
    AddSubtitles,
    VideoFilters,

    // --- ventana -----------------------------------------------------------
    OpenSettings,
    ToggleMaximize,
    MinimizeWindow,
    Quit,

    Count
};

struct Info {
    const char* id;        // clave en los ajustes: no cambiarla entre versiones
    const char* label;     // en espanol; se traduce con Lang::tr al mostrarla
    const char* group;     // idem
    QKeySequence defaultKeys;
};

const Info& info(int command);

// Atajo vigente: el que eligio el usuario o, si no toco nada, el de fabrica.
// Puede estar vacio: la orden existe pero no tiene tecla.
QKeySequence keys(int command);
void setKeys(int command, const QKeySequence& keys);
void resetKeys(int command);

// Otra orden que ya usa `keys`, o -1. Para no dejar dos con la misma tecla.
int commandUsing(const QKeySequence& keys, int except);

// Texto para la columna de atajos del menu ("Ctrl+E").
QString keysText(int command);

} // namespace Commands
