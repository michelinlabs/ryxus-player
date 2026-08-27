#pragma once

#include "core/TrackInfo.h"

#include <QObject>
#include <QString>
#include <QVector>

#include <atomic>

// Lee las etiquetas de todos los archivos de audio de una carpeta en un hilo
// aparte y los va entregando por lotes, para que la lista central se llene
// progresivamente en vez de bloquearse.
class LibraryScanner : public QObject {
    Q_OBJECT

public:
    explicit LibraryScanner(QObject* parent = nullptr);

    // Invalida el escaneo en curso desde cualquier hilo.
    void requestStop() { m_generation.fetch_add(1); }

public slots:
    void scanFolder(const QString& folder, bool recursive, quint64 requestId);

signals:
    void batchReady(const QVector<TrackInfo>& tracks, quint64 requestId);
    void scanFinished(const QString& folder, int totalFiles, quint64 requestId);

private:
    std::atomic<quint64> m_generation{0};
};
