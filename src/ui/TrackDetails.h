#pragma once

#include "core/TrackInfo.h"

#include <QVector>
#include <QWidget>

class FlatButton;
class QLabel;

// Ficha de datos bajo el analizador. Recoge lo que antes ensanchaba la lista
// central (artista, album, duracion, tamano...) y lo muestra en la columna
// izquierda, donde el skin de referencia deja hueco libre.
class TrackDetails : public QWidget {
    Q_OBJECT

public:
    explicit TrackDetails(QWidget* parent = nullptr);

    void setTrack(const TrackInfo& info);

signals:
    void editRequested();
    void revealRequested(const QString& path);

private:
    struct Row {
        QLabel* key   = nullptr;
        QLabel* value = nullptr;
    };

    void setRow(const Row& row, const QString& value);

    QVector<Row> m_rows;
    Row m_artist, m_album, m_albumArtist, m_genre, m_year;
    Row m_track, m_duration, m_size, m_sampleRate, m_path;

    QLabel*     m_header = nullptr;
    TrackInfo   m_track_;
};
