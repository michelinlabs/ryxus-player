#include "ui/Commands.h"

#include "core/Settings.h"

#include <QSettings>

namespace Commands {
namespace {

// Atajos de fabrica: los de siempre se mantienen, y los nuevos evitan las
// teclas que ya usan los campos de texto (Ctrl+C, Ctrl+V, Ctrl+A...).
const Info* table()
{
    static const Info kTable[Count] = {
        {"playPause",   "Reproducir / pausar",          "Reproduccion", QKeySequence(Qt::Key_Space)},
        {"stop",        "Detener",                      "Reproduccion", QKeySequence()},
        {"next",        "Pista siguiente",              "Reproduccion", QKeySequence(Qt::CTRL | Qt::Key_Right)},
        {"previous",    "Pista anterior",               "Reproduccion", QKeySequence(Qt::CTRL | Qt::Key_Left)},
        // Las flechas solas: mantenerlas pulsadas repite. En listas y campos
        // siguen moviendose por ellos (MainWindow::eventFilter).
        {"seekForward", "Avanzar",                      "Reproduccion", QKeySequence(Qt::Key_Right)},
        {"seekBackward", "Retroceder",                  "Reproduccion", QKeySequence(Qt::Key_Left)},
        {"volumeUp",    "Subir el volumen",             "Reproduccion", QKeySequence(Qt::Key_Up)},
        {"volumeDown",  "Bajar el volumen",             "Reproduccion", QKeySequence(Qt::Key_Down)},
        {"mute",        "Silenciar",                    "Reproduccion", QKeySequence(Qt::CTRL | Qt::Key_M)},
        {"shuffle",     "Orden aleatorio",              "Reproduccion", QKeySequence()},
        {"repeat",      "Cambiar el modo de repeticion", "Reproduccion", QKeySequence(Qt::CTRL | Qt::Key_R)},
        {"abLoop",      "Bucle A-B",                    "Reproduccion", QKeySequence()},

        {"openFiles",   "Abrir archivos...",            "Archivos", QKeySequence(Qt::CTRL | Qt::Key_O)},
        {"openFolder",  "Abrir carpeta...",             "Archivos", QKeySequence(Qt::CTRL | Qt::Key_K)},
        {"editTags",    "Editar etiquetas de la pista actual", "Archivos", QKeySequence(Qt::Key_F4)},

        {"eqPanel",     "Ecualizador y efectos",        "Ecualizador", QKeySequence(Qt::CTRL | Qt::Key_E)},
        {"eqOnOff",     "Activar / desactivar el ecualizador", "Ecualizador",
                        QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E)},

        {"panelNowPlaying", "Mostrar / ocultar Reproduciendo", "Paneles", QKeySequence(Qt::CTRL | Qt::Key_1)},
        {"panelFiles",      "Mostrar / ocultar Archivos",      "Paneles", QKeySequence(Qt::CTRL | Qt::Key_2)},
        {"panelPlaylist",   "Mostrar / ocultar Lista de reproduccion", "Paneles",
                            QKeySequence(Qt::CTRL | Qt::Key_3)},
        {"showAllPanels",   "Mostrar todos los paneles",       "Paneles",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_M)},
        {"hideAllPanels",   "Ocultar todos los paneles",       "Paneles",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_H)},

        {"nextLayout",      "Disposicion siguiente",           "Disposiciones", QKeySequence(Qt::CTRL | Qt::Key_L)},
        {"previousLayout",  "Disposicion anterior",            "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_L)},
        {"layoutClassic",   "Disposicion Clasica",             "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_1)},
        {"layoutLibrary",   "Disposicion Biblioteca",          "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_2)},
        {"layoutListening", "Disposicion Escucha",             "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_3)},
        {"layoutStudio",    "Disposicion Estudio",             "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_4)},
        {"saveLayout",      "Guardar la disposicion actual...", "Disposiciones",
                            QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S)},

        {"chooseBackground", "Elegir imagen de fondo...",      "Fondo", QKeySequence()},
        {"clearBackground",  "Quitar la imagen de fondo",      "Fondo", QKeySequence()},
        {"nextVisualization", "Visualizacion siguiente",       "Fondo",
                             QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V)},

        {"videoCinema",      "Video: solo la imagen",          "Video", QKeySequence(Qt::ALT | Qt::Key_1)},
        {"videoPlaylist",    "Video: imagen y lista",          "Video", QKeySequence(Qt::ALT | Qt::Key_2)},
        {"videoEqualizer",   "Video: imagen y banco de filtros", "Video", QKeySequence(Qt::ALT | Qt::Key_3)},
        {"videoStudio",      "Video: estudio",                 "Video", QKeySequence(Qt::ALT | Qt::Key_4)},
        {"nextVideoMask",    "Mascara de video siguiente",     "Video", QKeySequence(Qt::ALT | Qt::Key_M)},
        {"videoFullscreen",  "Video a pantalla completa",      "Video", QKeySequence(Qt::ALT | Qt::Key_Return)},
        {"addAudioTrack",    "Anadir pista de audio...",       "Video", QKeySequence()},
        {"addSubtitles",     "Anadir subtitulos...",           "Video", QKeySequence()},
        {"videoFilters",     "Mostrar / ocultar la paleta de filtros de video", "Video",
                             QKeySequence(Qt::ALT | Qt::Key_F)},

        {"settings",    "Configuracion...",             "Ventana", QKeySequence(Qt::CTRL | Qt::Key_Comma)},
        {"maximize",    "Maximizar / restaurar la ventana", "Ventana", QKeySequence(Qt::Key_F11)},
        {"minimize",    "Minimizar la ventana",         "Ventana", QKeySequence()},
        {"quit",        "Salir",                        "Ventana", QKeySequence(Qt::CTRL | Qt::Key_Q)},
    };
    return kTable;
}

QString settingsKey(int command)
{
    return QStringLiteral("shortcuts/") + QString::fromLatin1(info(command).id);
}

} // namespace

const Info& info(int command)
{
    return table()[qBound(0, command, Count - 1)];
}

QKeySequence keys(int command)
{
    // Si el usuario lo toco, manda lo suyo, aunque sea "sin tecla" (cadena
    // vacia). Si no, el de fabrica.
    const QString key = settingsKey(command);
    if (!Settings::store().contains(key))
        return info(command).defaultKeys;
    return QKeySequence::fromString(Settings::store().value(key).toString(),
                                    QKeySequence::PortableText);
}

void setKeys(int command, const QKeySequence& keys)
{
    Settings::store().setValue(settingsKey(command), keys.toString(QKeySequence::PortableText));
}

void resetKeys(int command)
{
    Settings::store().remove(settingsKey(command));
}

int commandUsing(const QKeySequence& sequence, int except)
{
    if (sequence.isEmpty())
        return -1;
    for (int command = 0; command < Count; ++command) {
        if (command != except && keys(command) == sequence)
            return command;
    }
    return -1;
}

QString keysText(int command)
{
    return keys(command).toString(QKeySequence::NativeText);
}

} // namespace Commands
