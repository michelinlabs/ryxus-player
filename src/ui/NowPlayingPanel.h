#pragma once

#include "core/TrackInfo.h"

#include <QWidget>

class AudioEngine;
class CoverArtView;
class FlatButton;
class NowPlayingHeader;
class QLabel;
class TrackDetails;
class Visualizer;

// Columna izquierda del skin: caratula, datos de la pista, analizador y la
// ficha de detalles (que recoge lo que antes ensanchaba la lista central).
class NowPlayingPanel : public QWidget {
    Q_OBJECT

public:
    explicit NowPlayingPanel(AudioEngine* engine, QWidget* parent = nullptr);

    // `playing` es la pista que suena; `selected` la que el usuario tiene
    // marcada en la lista. Los detalles siguen a la seleccion, para poder
    // inspeccionar una pista sin interrumpir la reproduccion.
    void setPlayingTrack(const TrackInfo& info);
    void setDetailsTrack(const TrackInfo& info);
    void setPlaying(bool playing);
    void clearTrack();

    CoverArtView* coverView() const { return m_cover; }

signals:
    void ratingChanged(int rating);
    void coverChangeRequested();
    void coverRemoveRequested();
    void coverExportRequested();
    void editTagsRequested();

protected:
    void paintEvent(QPaintEvent* event) override;

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    CoverArtView*     m_cover      = nullptr;
    NowPlayingHeader* m_header     = nullptr;
    Visualizer*       m_visualizer = nullptr;
    TrackDetails*     m_details    = nullptr;
    FlatButton*       m_editButton = nullptr;
    QString m_footerPath;   // sin recortar, para re-elidir al cambiar de ancho
    QLabel*           m_footer     = nullptr;
};
