#pragma once

#include <QString>

// Envoltorio del explorador de archivos del sistema.
//
// Vive en su propio archivo para que <shlobj.h> (y con el <windows.h> entero)
// no se cuele en las unidades de compilacion de la interfaz, donde choca con
// las cabeceras de Qt.
namespace ShellUtils {

// Abre la carpeta que contiene `path` con el archivo ya seleccionado. Si el
// archivo ya no esta, abre la carpeta a secas.
void revealInFileManager(const QString& path);

} // namespace ShellUtils
