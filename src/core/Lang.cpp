#include "core/Lang.h"

#include <QHash>

namespace Lang {
namespace {

Id g_current = Id::Spanish;

// Tabla espanol -> ingles. Solo cadenas visibles: los nombres de objeto, las
// claves de ajustes y los identificadores de estilo se quedan fuera a
// proposito.
const QHash<QString, QString>& englishTable()
{
    static const QHash<QString, QString> table = {
        // --- barra de titulo y menu principal ---------------------------
        {QStringLiteral("Menu de la aplicacion"),      QStringLiteral("Application menu")},
        {QStringLiteral("Abrir archivos..."),          QStringLiteral("Open files...")},
        {QStringLiteral("Abrir carpeta..."),           QStringLiteral("Open folder...")},
        {QStringLiteral("Configuracion..."),           QStringLiteral("Settings...")},
        {QStringLiteral("Salir"),                      QStringLiteral("Quit")},
        {QStringLiteral("Desinstalar Roxas Player..."),
         QStringLiteral("Uninstall Roxas Player...")},
        {QStringLiteral("Desinstalar Roxas Player"),
         QStringLiteral("Uninstall Roxas Player")},
        {QStringLiteral("Se cerrara el reproductor y se abrira el desinstalador de "
                        "Windows.\n\n"
                        "Tus ajustes, temas y listas no se borran: si vuelves a "
                        "instalarlo, siguen ahi."),
         QStringLiteral("The player will close and the Windows uninstaller will "
                        "open.\n\n"
                        "Your settings, themes and playlists are kept: if you "
                        "install it again, they are still there.")},

        // --- pestanas del panel central ---------------------------------
        {QStringLiteral("Archivos locales"),           QStringLiteral("Local files")},
        {QStringLiteral("Etiquetas"),                  QStringLiteral("Tags")},
        {QStringLiteral("Marcadores"),                 QStringLiteral("Bookmarks")},
        {QStringLiteral("Mis Nubes"),                  QStringLiteral("My Clouds")},
        {QStringLiteral("Podcasts"),                   QStringLiteral("Podcasts")},

        // --- arbol de carpetas ------------------------------------------
        {QStringLiteral("Carpetas"),                   QStringLiteral("Folders")},
        {QStringLiteral("Abrir carpeta"),              QStringLiteral("Open folder")},
        {QStringLiteral("Abrir carpeta (Ctrl+K)"),     QStringLiteral("Open folder (Ctrl+K)")},
        {QStringLiteral("Abrir carpeta de musica"),    QStringLiteral("Open music folder")},
        {QStringLiteral("Abrir otra carpeta"),         QStringLiteral("Open another folder")},
        {QStringLiteral("Cerrar carpeta"),             QStringLiteral("Close folder")},
        {QStringLiteral("Actualizar"),                 QStringLiteral("Refresh")},
        {QStringLiteral("Reproducir esta carpeta"),    QStringLiteral("Play this folder")},
        {QStringLiteral("Todas las carpetas"),         QStringLiteral("All folders")},
        {QStringLiteral("Busqueda rapida"),            QStringLiteral("Quick search")},
        {QStringLiteral("No hay ninguna carpeta abierta.\n\n"
                        "Abre la carpeta donde tengas tu musica y quedara\n"
                        "aqui como raiz para explorarla."),
         QStringLiteral("No folder is open yet.\n\n"
                        "Open the folder where your music lives and it will\n"
                        "stay here as a root to browse.")},

        // --- lista central ----------------------------------------------
        {QStringLiteral("Pista N.o"),                  QStringLiteral("Track no.")},
        {QStringLiteral("Nombre de archivo"),          QStringLiteral("File name")},
        {QStringLiteral("Titulo"),                     QStringLiteral("Title")},
        {QStringLiteral("Artista"),                    QStringLiteral("Artist")},
        {QStringLiteral("Album"),                      QStringLiteral("Album")},
        {QStringLiteral("Duracion"),                   QStringLiteral("Length")},
        {QStringLiteral("Tamano"),                     QStringLiteral("Size")},
        {QStringLiteral("Columnas visibles"),          QStringLiteral("Visible columns")},
        {QStringLiteral("Leyendo..."),                 QStringLiteral("Reading...")},

        // --- menu contextual --------------------------------------------
        {QStringLiteral("Reproducir"),                 QStringLiteral("Play")},
        {QStringLiteral("Reproducir seleccionado"),    QStringLiteral("Play selection")},
        {QStringLiteral("Agregar archivos"),           QStringLiteral("Add files")},
        {QStringLiteral("Agregar a la lista de reproduccion"),
         QStringLiteral("Add to playlist")},
        {QStringLiteral("Propiedades"),                QStringLiteral("Properties")},
        {QStringLiteral("Ubicacion de archivo"),       QStringLiteral("File location")},
        {QStringLiteral("Ubicacion"),                  QStringLiteral("Location")},
        {QStringLiteral("Releer etiquetas de archivos seleccionados"),
         QStringLiteral("Reload tags of selected files")},
        {QStringLiteral("Calificacion"),               QStringLiteral("Rating")},
        {QStringLiteral("Sin calificar"),              QStringLiteral("Not rated")},
        {QStringLiteral("Etiquetas..."),               QStringLiteral("Tags...")},
        {QStringLiteral("Eliminar permanentemente los archivos seleccionados"),
         QStringLiteral("Permanently delete selected files")},
        {QStringLiteral("Eliminar seleccionados de la lista"),
         QStringLiteral("Remove selected from playlist")},
        {QStringLiteral("Eliminar archivos"),          QStringLiteral("Delete files")},

        // --- panel izquierdo --------------------------------------------
        {QStringLiteral("Sin titulo"),                 QStringLiteral("Untitled")},
        {QStringLiteral("DETALLES"),                   QStringLiteral("DETAILS")},
        {QStringLiteral("Artista alb."),               QStringLiteral("Album artist")},
        {QStringLiteral("Genero"),                     QStringLiteral("Genre")},
        {QStringLiteral("Ano"),                        QStringLiteral("Year")},
        {QStringLiteral("Pista"),                      QStringLiteral("Track")},
        {QStringLiteral("Audio"),                      QStringLiteral("Audio")},
        {QStringLiteral("Editar etiquetas"),           QStringLiteral("Edit tags")},
        {QStringLiteral("Abre la pestana de etiquetas (F4)"),
         QStringLiteral("Opens the tags tab (F4)")},
        {QStringLiteral("Doble clic para cambiar la caratula"),
         QStringLiteral("Double-click to change the cover")},
        {QStringLiteral("Calificacion (clic en la misma estrella para quitarla)"),
         QStringLiteral("Rating (click the same star to clear it)")},
        {QStringLiteral("Clic para alternar entre onda y espectro"),
         QStringLiteral("Click to switch between waveform and spectrum")},

        // --- editor de etiquetas ----------------------------------------
        {QStringLiteral("EDITAR ETIQUETAS"),           QStringLiteral("EDIT TAGS")},
        {QStringLiteral("Artista del album"),          QStringLiteral("Album artist")},
        {QStringLiteral("Compositor"),                 QStringLiteral("Composer")},
        {QStringLiteral("N.o pista"),                  QStringLiteral("Track no.")},
        {QStringLiteral("Disco"),                      QStringLiteral("Disc")},
        {QStringLiteral("Comentario"),                 QStringLiteral("Comment")},
        {QStringLiteral("Guardar cambios"),            QStringLiteral("Save changes")},
        {QStringLiteral("Revertir"),                   QStringLiteral("Revert")},
        {QStringLiteral("Cambiar caratula..."),        QStringLiteral("Change cover...")},
        {QStringLiteral("Exportar caratula..."),       QStringLiteral("Export cover...")},
        {QStringLiteral("Exportar caratula"),          QStringLiteral("Export cover")},
        {QStringLiteral("Quitar caratula"),            QStringLiteral("Remove cover")},
        {QStringLiteral("Elegir caratula"),            QStringLiteral("Choose cover")},
        {QStringLiteral("Este archivo es de solo lectura: no se puede guardar."),
         QStringLiteral("This file is read-only: changes cannot be saved.")},
        {QStringLiteral("Selecciona una pista en la lista y pulsa \"Editar etiquetas\"\n"
                        "para modificar sus datos aqui."),
         QStringLiteral("Pick a track in the list and press \"Edit tags\"\n"
                        "to change its data here.")},

        // --- panel de listas --------------------------------------------
        {QStringLiteral("Anadir archivos"),            QStringLiteral("Add files")},
        {QStringLiteral("Anadir archivos a la lista"), QStringLiteral("Add files to the playlist")},
        {QStringLiteral("Quitar seleccionados"),       QStringLiteral("Remove selected")},
        {QStringLiteral("Mas acciones"),               QStringLiteral("More actions")},
        {QStringLiteral("Ordenar"),                    QStringLiteral("Sort")},
        {QStringLiteral("Menu de la lista"),           QStringLiteral("Playlist menu")},
        {QStringLiteral("Vaciar lista"),               QStringLiteral("Clear playlist")},
        {QStringLiteral("Vaciar"),                     QStringLiteral("Clear")},
        {QStringLiteral("Marcar todo"),                QStringLiteral("Check all")},
        {QStringLiteral("Desmarcar todo"),             QStringLiteral("Uncheck all")},
        {QStringLiteral("Por titulo"),                 QStringLiteral("By title")},
        {QStringLiteral("Por artista"),                QStringLiteral("By artist")},
        {QStringLiteral("Por album"),                  QStringLiteral("By album")},
        {QStringLiteral("Por duracion"),               QStringLiteral("By length")},
        {QStringLiteral("Por nombre de archivo"),      QStringLiteral("By file name")},
        {QStringLiteral("Por N.o de pista"),           QStringLiteral("By track number")},
        {QStringLiteral("Por calificacion"),           QStringLiteral("By rating")},
        {QStringLiteral("Mezclar orden"),              QStringLiteral("Shuffle order")},
        {QStringLiteral("Nueva lista"),                QStringLiteral("New playlist")},
        {QStringLiteral("Nueva lista de reproduccion"),QStringLiteral("New playlist")},
        {QStringLiteral("Renombrar lista actual"),     QStringLiteral("Rename current playlist")},
        {QStringLiteral("Renombrar lista"),            QStringLiteral("Rename playlist")},
        {QStringLiteral("Renombrar..."),               QStringLiteral("Rename...")},
        {QStringLiteral("Cerrar lista"),               QStringLiteral("Close playlist")},
        {QStringLiteral("Lista %1"),                   QStringLiteral("Playlist %1")},
        {QStringLiteral("Nombre:"),                    QStringLiteral("Name:")},

        // --- barra de reproduccion --------------------------------------
        {QStringLiteral("Anterior"),                   QStringLiteral("Previous")},
        {QStringLiteral("Siguiente"),                  QStringLiteral("Next")},
        {QStringLiteral("Parar"),                      QStringLiteral("Stop")},
        {QStringLiteral("Pausa"),                      QStringLiteral("Pause")},
        {QStringLiteral("Silenciar"),                  QStringLiteral("Mute")},
        {QStringLiteral("Volumen"),                    QStringLiteral("Volume")},
        {QStringLiteral("Orden aleatorio"),            QStringLiteral("Shuffle")},
        {QStringLiteral("Repetir fragmento A-B"),      QStringLiteral("Repeat A-B section")},
        {QStringLiteral("Repeticion: %1"),             QStringLiteral("Repeat: %1")},
        {QStringLiteral("Repeticion: desactivada / lista / pista"),
         QStringLiteral("Repeat: off / playlist / track")},
        {QStringLiteral("desactivada"),                QStringLiteral("off")},
        {QStringLiteral("toda la lista"),              QStringLiteral("whole playlist")},
        {QStringLiteral("pista actual"),               QStringLiteral("current track")},
        {QStringLiteral("Alternar tiempo transcurrido / restante"),
         QStringLiteral("Toggle elapsed / remaining time")},

        // --- ecualizador --------------------------------------------------
        {QStringLiteral("ECUALIZADOR"),                QStringLiteral("EQUALIZER")},
        {QStringLiteral("Ecualizador y efectos"),      QStringLiteral("Equalizer and effects")},
        {QStringLiteral("Ecualizador de 12 bandas y efectos"),
         QStringLiteral("12-band equalizer and effects")},
        {QStringLiteral("Activado"),                   QStringLiteral("Enabled")},
        {QStringLiteral("Preset"),                     QStringLiteral("Preset")},
        {QStringLiteral("Guardar"),                    QStringLiteral("Save")},
        {QStringLiteral("Guardar preset"),             QStringLiteral("Save preset")},
        {QStringLiteral("Reiniciar"),                  QStringLiteral("Reset")},
        {QStringLiteral("Nombre del preset:"),         QStringLiteral("Preset name:")},
        {QStringLiteral("Mi preset"),                  QStringLiteral("My preset")},
        {QStringLiteral("Personalizado"),              QStringLiteral("Custom")},
        {QStringLiteral("Plano"),                      QStringLiteral("Flat")},
        {QStringLiteral("Clasica"),                    QStringLiteral("Classical")},
        {QStringLiteral("Electronica"),                QStringLiteral("Electronic")},
        {QStringLiteral("Vocal"),                      QStringLiteral("Vocal")},
        {QStringLiteral("Grave +"),                    QStringLiteral("Bass +")},
        {QStringLiteral("Agudo +"),                    QStringLiteral("Treble +")},
        {QStringLiteral("arrastra los nodos  ·  rueda = Q"),
         QStringLiteral("drag the nodes  ·  wheel = Q")},
        {QStringLiteral("Respuesta combinada de las 8 bandas"),
         QStringLiteral("Combined response of the 8 bands")},

        // --- rack de efectos ----------------------------------------------
        {QStringLiteral("Efectos"),                    QStringLiteral("Effects")},
        {QStringLiteral("Mostrar u ocultar el rack de efectos"),
         QStringLiteral("Show or hide the effects rack")},
        {QStringLiteral("Activar o desactivar este efecto"),
         QStringLiteral("Turn this effect on or off")},
        {QStringLiteral("Compresor"),                  QStringLiteral("Compressor")},
        {QStringLiteral("Iguala la dinamica y levanta lo que queda bajo"),
         QStringLiteral("Evens out the dynamics and lifts what stays quiet")},
        {QStringLiteral("Umbral"),                     QStringLiteral("Threshold")},
        {QStringLiteral("Ratio"),                      QStringLiteral("Ratio")},
        {QStringLiteral("Ganancia"),                   QStringLiteral("Makeup")},
        {QStringLiteral("Saturacion"),                 QStringLiteral("Saturation")},
        {QStringLiteral("Calienta la senal con distorsion suave"),
         QStringLiteral("Warms up the signal with soft distortion")},
        {QStringLiteral("Drive"),                      QStringLiteral("Drive")},
        {QStringLiteral("Mezcla"),                     QStringLiteral("Mix")},
        {QStringLiteral("Chorus"),                     QStringLiteral("Chorus")},
        {QStringLiteral("Duplica la senal y la desafina un poco"),
         QStringLiteral("Doubles the signal and detunes it slightly")},
        {QStringLiteral("Velocidad"),                  QStringLiteral("Rate")},
        {QStringLiteral("Profundo"),                   QStringLiteral("Depth")},
        {QStringLiteral("Eco"),                        QStringLiteral("Delay")},
        {QStringLiteral("Repeticiones con realimentacion"),
         QStringLiteral("Repeats with feedback")},
        {QStringLiteral("Tiempo"),                     QStringLiteral("Time")},
        {QStringLiteral("Feedback"),                   QStringLiteral("Feedback")},
        {QStringLiteral("Reverberacion"),              QStringLiteral("Reverb")},
        {QStringLiteral("Cola de sala, de cabina a nave"),
         QStringLiteral("Room tail, from booth to hall")},
        {QStringLiteral("Tamano"),                     QStringLiteral("Size")},
        {QStringLiteral("Amortigua"),                  QStringLiteral("Damping")},
        {QStringLiteral("Estereo"),                    QStringLiteral("Stereo")},
        {QStringLiteral("Abre o cierra la imagen estereo"),
         QStringLiteral("Widens or narrows the stereo image")},
        {QStringLiteral("Amplitud"),                   QStringLiteral("Width")},
        {QStringLiteral("Arrastra los nodos: horizontal = frecuencia, vertical = ganancia.\n"
                        "Rueda sobre un nodo = ancho de banda (Q). Doble clic = reiniciar.\n"
                        "Manten Shift para mover solo la ganancia."),
         QStringLiteral("Drag the nodes: horizontal = frequency, vertical = gain.\n"
                        "Wheel over a node = bandwidth (Q). Double-click = reset.\n"
                        "Hold Shift to move gain only.")},
        {QStringLiteral("Arrastra para ajustar - doble clic para 0 dB"),
         QStringLiteral("Drag to adjust - double-click for 0 dB")},

        // --- configuracion ------------------------------------------------
        {QStringLiteral("Configuracion"),              QStringLiteral("Settings")},
        {QStringLiteral("APARIENCIA"),                 QStringLiteral("APPEARANCE")},
        {QStringLiteral("FONDO"),                      QStringLiteral("BACKGROUND")},
        {QStringLiteral("IDIOMA"),                     QStringLiteral("LANGUAGE")},
        {QStringLiteral("Tema de color"),              QStringLiteral("Colour theme")},
        {QStringLiteral("Imagen de fondo"),            QStringLiteral("Background image")},
        {QStringLiteral("Elegir imagen..."),           QStringLiteral("Choose image...")},
        {QStringLiteral("Elegir imagen de fondo"),     QStringLiteral("Choose background image")},
        {QStringLiteral("Quitar imagen"),              QStringLiteral("Remove image")},
        {QStringLiteral("Sin imagen de fondo"),        QStringLiteral("No background image")},
        {QStringLiteral("Ajuste"),                     QStringLiteral("Fit")},
        {QStringLiteral("Cubrir"),                     QStringLiteral("Cover")},
        {QStringLiteral("Ajustar"),                    QStringLiteral("Contain")},
        {QStringLiteral("Estirar"),                    QStringLiteral("Stretch")},
        {QStringLiteral("Mosaico"),                    QStringLiteral("Tile")},
        {QStringLiteral("Transparencia de los paneles"),
         QStringLiteral("Panel transparency")},
        {QStringLiteral("Oscurecer la imagen"),        QStringLiteral("Darken the image")},
        {QStringLiteral("La transparencia y el oscurecido solo actuan cuando hay "
                        "una imagen de fondo."),
         QStringLiteral("Transparency and darkening only apply when a background "
                        "image is set.")},
        {QStringLiteral("El idioma se aplica al reiniciar el reproductor."),
         QStringLiteral("The language takes effect when the player restarts.")},
        {QStringLiteral("Reiniciar ahora"),            QStringLiteral("Restart now")},
        {QStringLiteral("Cerrar"),                     QStringLiteral("Close")},
        {QStringLiteral("Se reiniciara Roxas Player para aplicar el idioma.\n"
                        "Se perdera la reproduccion en curso."),
         QStringLiteral("Roxas Player will restart to apply the language.\n"
                        "Playback in progress will be lost.")},

        // --- avisos y errores ---------------------------------------------
        {QStringLiteral("No se pudo guardar"),         QStringLiteral("Could not save")},
        {QStringLiteral("No se pudo guardar la caratula"),
         QStringLiteral("Could not save the cover")},
        {QStringLiteral("No se pudo quitar la caratula"),
         QStringLiteral("Could not remove the cover")},
        {QStringLiteral("No se pudo leer la imagen."), QStringLiteral("Could not read the image.")},
        {QStringLiteral("No se pudo escribir el archivo."),
         QStringLiteral("Could not write the file.")},
        {QStringLiteral("No se pudo codificar la imagen."),
         QStringLiteral("Could not encode the image.")},
        {QStringLiteral("No se pudo decodificar el archivo: %1"),
         QStringLiteral("Could not decode the file: %1")},
        {QStringLiteral("No se pudo abrir el dispositivo de audio."),
         QStringLiteral("Could not open the audio device.")},
        {QStringLiteral("No se pudo iniciar el dispositivo de audio."),
         QStringLiteral("Could not start the audio device.")},
        {QStringLiteral("El dispositivo de audio no esta disponible."),
         QStringLiteral("The audio device is not available.")},
        {QStringLiteral("No hay archivo seleccionado."),
         QStringLiteral("No file selected.")},
        {QStringLiteral("El archivo ya no existe."),   QStringLiteral("The file no longer exists.")},
        {QStringLiteral("El archivo es de solo lectura."),
         QStringLiteral("The file is read-only.")},
        {QStringLiteral("Formato no reconocido por TagLib."),
         QStringLiteral("Format not recognised by TagLib.")},
        {QStringLiteral("TagLib no pudo escribir en el archivo."),
         QStringLiteral("TagLib could not write to the file.")},
        {QStringLiteral("TagLib no pudo escribir la caratula."),
         QStringLiteral("TagLib could not write the cover.")},
        {QStringLiteral("No se pudieron eliminar:\n%1"),
         QStringLiteral("Could not delete:\n%1")},
        {QStringLiteral("Se eliminaran %1 archivo(s) del disco de forma permanente.\n"
                        "Esta accion no se puede deshacer."),
         QStringLiteral("%1 file(s) will be permanently deleted from disk.\n"
                        "This cannot be undone.")},
        {QStringLiteral("%1\n\nLa interfaz funciona, pero no habra sonido."),
         QStringLiteral("%1\n\nThe interface works, but there will be no sound.")},
        {QStringLiteral("Archivo de solo lectura"),    QStringLiteral("Read-only file")},

        // --- textos de relleno ---------------------------------------------
        {QStringLiteral("Carpetas marcadas\n\nUsa el boton + del arbol de carpetas para anadir\n"
                        "las rutas que quieras tener siempre a mano."),
         QStringLiteral("Bookmarked folders\n\nUse the + button in the folder tree to add\n"
                        "the paths you want close at hand.")},
        {QStringLiteral("%1\n\nSeccion reservada: requiere conectar un servicio externo."),
         QStringLiteral("%1\n\nReserved section: needs an external service to be connected.")},

        // --- cadenas de datos -----------------------------------------------
        {QStringLiteral("Artista desconocido"),        QStringLiteral("Unknown artist")},
        {QStringLiteral("Mono"),                       QStringLiteral("Mono")},
        {QStringLiteral("Stereo"),                     QStringLiteral("Stereo")},
        {QStringLiteral("%1 canales"),                 QStringLiteral("%1 channels")},
        {QStringLiteral(", %1 canales"),               QStringLiteral(", %1 channels")},
        {QStringLiteral(", estereo"),                  QStringLiteral(", stereo")},
        {QStringLiteral(", mono"),                     QStringLiteral(", mono")},
        {QStringLiteral("  (disco %1)"),               QStringLiteral("  (disc %1)")},
        {QStringLiteral("Archivos de audio (%1);;Todos los archivos (*)"),
         QStringLiteral("Audio files (%1);;All files (*)")},
        {QStringLiteral("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp);;Todos los archivos (*)"),
         QStringLiteral("Images (*.jpg *.jpeg *.png *.bmp *.webp);;All files (*)")},
        {QStringLiteral("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp)"),
         QStringLiteral("Images (*.jpg *.jpeg *.png *.bmp *.webp)")},
        {QStringLiteral("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp *.gif);;Todos los archivos (*)"),
         QStringLiteral("Images (*.jpg *.jpeg *.png *.bmp *.webp *.gif);;All files (*)")},
    };
    return table;
}

} // namespace

const QVector<Entry>& available()
{
    static const QVector<Entry> entries = {
        {Id::Spanish, QStringLiteral("es"), QStringLiteral("Espanol")},
        {Id::English, QStringLiteral("en"), QStringLiteral("English")},
    };
    return entries;
}

Id   current() { return g_current; }
void setCurrent(Id id) { g_current = id; }

QString code(Id id)
{
    for (const Entry& entry : available())
        if (entry.id == id)
            return entry.code;
    return QStringLiteral("es");
}

Id fromCode(const QString& code)
{
    for (const Entry& entry : available())
        if (entry.code.compare(code, Qt::CaseInsensitive) == 0)
            return entry.id;
    return Id::Spanish;
}

QString tr(const char* spanish)
{
    const QString source = QString::fromUtf8(spanish);
    if (g_current == Id::Spanish)
        return source;

    const auto& table = englishTable();
    const auto it = table.constFind(source);
    return (it != table.constEnd()) ? it.value() : source;
}

} // namespace Lang
