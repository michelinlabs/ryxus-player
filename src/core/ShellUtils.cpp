#include "core/ShellUtils.h"

#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QUrl>

#ifdef Q_OS_WIN
#  include <QProcess>
#  include <shlobj.h>
#endif

namespace ShellUtils {

void revealInFileManager(const QString& path)
{
    if (path.isEmpty())
        return;

    const QFileInfo fi(path);
    const QString folder = fi.absolutePath();

    // Si el archivo ya no esta se abre al menos su carpeta. Antes la funcion
    // se iba en silencio y el boton parecia no hacer nada.
    if (!fi.exists()) {
        if (!folder.isEmpty() && QFileInfo::exists(folder))
            QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
        return;
    }

    const QString native = QDir::toNativeSeparators(fi.absoluteFilePath());

#ifdef Q_OS_WIN
    // La via buena es la API del shell.
    //
    // Lanzar explorer.exe con QProcess y {"/select,<ruta>"} NO vale: Qt
    // construye la linea de comandos entrecomillando cualquier argumento que
    // lleve espacios, asi que explorer recibe "/select,C:\Mi musica\tema.mp3"
    // en un solo bloque entre comillas, no reconoce el modificador y abre su
    // carpeta por defecto -- Documentos. Ese era el motivo de que "Ubicacion"
    // acabara siempre en Mis Documentos.
    if (PIDLIST_ABSOLUTE pidl =
            ILCreateFromPathW(reinterpret_cast<const wchar_t*>(native.utf16()))) {
        const HRESULT hr = SHOpenFolderAndSelectItems(pidl, 0, nullptr, 0);
        ILFree(pidl);
        if (SUCCEEDED(hr))
            return;
    }

    // Reserva: linea de comandos cruda, sin que Qt reescriba las comillas.
    QProcess explorer;
    explorer.setProgram(QStringLiteral("explorer.exe"));
    explorer.setNativeArguments(QStringLiteral("/select,\"%1\"").arg(native));
    if (explorer.startDetached())
        return;
#endif

    // Ultimo recurso (y camino normal fuera de Windows): abrir la carpeta.
    QDesktopServices::openUrl(QUrl::fromLocalFile(folder));
}

} // namespace ShellUtils
