#pragma once

#include <QString>
#include <QVector>

// Subtitulos de un archivo aparte (.srt, .vtt, .ass/.ssa), para los videos
// que no los traen dentro o cuando se quieren otros.
//
// Solo el texto y sus tiempos: los estilos de ASS (colores, posiciones) se
// descartan y se pinta todo igual, como los subtitulos incorporados.
class Subtitles {
public:
    static bool isSubtitleFile(const QString& path);
    static QString fileFilter();

    bool load(const QString& path);
    void clear();
    bool isEmpty() const { return m_cues.isEmpty(); }
    QString path() const { return m_path; }

    // Texto a mostrar en ese instante, o vacio. Varias lineas con '\n'.
    QString textAt(qint64 ms) const;

private:
    struct Cue {
        qint64  start = 0;
        qint64  end   = 0;
        QString text;
    };

    void parseSrtOrVtt(const QString& text);
    void parseAss(const QString& text);

    QVector<Cue> m_cues;   // ordenados por inicio
    QString      m_path;
};
