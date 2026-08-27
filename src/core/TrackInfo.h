#pragma once

#include <QImage>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>

// Descripcion completa de un archivo de audio: etiquetas editables,
// propiedades tecnicas de solo lectura y datos derivados (caratula, picos).
struct TrackInfo {
    // --- identidad ---------------------------------------------------------
    QString path;
    QString fileName;

    // --- etiquetas editables ----------------------------------------------
    QString title;
    QString artist;
    QString album;
    QString albumArtist;
    QString genre;
    QString comment;
    QString composer;
    int     year        = 0;
    int     trackNumber = 0;
    int     discNumber  = 0;
    int     rating      = 0;   // 0..5 estrellas

    // --- propiedades tecnicas (solo lectura) -------------------------------
    QString format;            // "MP3", "FLAC", "WAV", ...
    int     durationMs  = 0;
    int     bitrateKbps = 0;
    int     sampleRate  = 0;
    int     channels    = 0;
    qint64  fileSize    = 0;

    // --- derivados ---------------------------------------------------------
    QImage         cover;
    QVector<float> peaks;      // envolvente para la barra de posicion
    bool           checked = true;
    bool           tagsLoaded = false;

    bool isValid() const { return !path.isEmpty(); }

    // "Artista - Titulo", con recursos si faltan etiquetas.
    QString displayName() const;
    QString displayTitle() const;
    QString displayArtist() const;

    // "MP3 :: 44 kHz, 320 kbps, 11,06 MB" (formato del skin de referencia).
    QString techLine() const;

    // "MP3, 44 kHz, 320 kbps, Stereo" (linea del panel de reproduccion).
    QString formatLine() const;

    static QString formatDuration(int ms, bool withHours = false);
    static QString formatSize(qint64 bytes);
    static QStringList supportedSuffixes();
    static QString supportedFilter();
    static bool isSupported(const QString& path);
};

Q_DECLARE_METATYPE(TrackInfo)
