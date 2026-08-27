#pragma once

#include "core/TrackInfo.h"

#include <QWidget>

class QLabel;
class RatingBar;

// Bloque bajo la caratula: titulo, artista, album, linea de formato y
// calificacion. Es de solo lectura salvo las estrellas; toda la edicion vive
// en la pestana "Etiquetas" del panel central.
class NowPlayingHeader : public QWidget {
    Q_OBJECT

public:
    explicit NowPlayingHeader(QWidget* parent = nullptr);

    void setTrack(const TrackInfo& info);

signals:
    void ratingChanged(int rating);

private:
    QLabel*    m_title  = nullptr;
    QLabel*    m_artist = nullptr;
    QLabel*    m_album  = nullptr;
    QLabel*    m_format = nullptr;
    RatingBar* m_rating = nullptr;
};
