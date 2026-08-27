#pragma once

#include "core/TrackInfo.h"

#include <QImage>
#include <QString>

// Lectura y escritura de etiquetas sobre el archivo real, via TagLib.
// Todas las funciones son reentrantes: se usan tanto desde el hilo de UI
// como desde los hilos de escaneo de biblioteca.
namespace MetadataService {

// Lee etiquetas + propiedades tecnicas. `withCover == false` omite la
// caratula, que es lo caro; se usa al poblar listados largos.
TrackInfo read(const QString& path, bool withCover = true);

QImage readCover(const QString& path);

// Escribe de vuelta las etiquetas editables de `info` en `info.path`.
bool write(const TrackInfo& info, QString* error = nullptr);

// Reemplaza (o elimina, si `cover` es nula) la caratula incrustada.
bool writeCover(const QString& path, const QImage& cover, QString* error = nullptr);

bool isWritable(const QString& path);

} // namespace MetadataService
