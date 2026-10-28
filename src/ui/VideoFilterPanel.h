#pragma once

#include <QImage>
#include <QVector>
#include <QWidget>

class FilterTile;
class QLabel;

// Paleta de filtros de video: una miniatura por mascara, hecha con el
// fotograma que se esta viendo, para elegir el aspecto viendolo antes.
//
// Al pasar el raton por una miniatura sale una vista grande del efecto junto
// al cursor, que se sigue actualizando con el video. Un clic la aplica.
class VideoFilterPanel : public QWidget {
    Q_OBJECT

public:
    explicit VideoFilterPanel(QWidget* parent = nullptr);
    ~VideoFilterPanel() override;

    // Fotograma reducido, sin mascara, del que salen las miniaturas. Las
    // miniaturas se hacen con los primeros que llegan y luego se quedan
    // quietas; solo la que tiene el raton encima sigue al video.
    void setSource(const QImage& frame);
    // Video nuevo: las miniaturas se vuelven a hacer con sus fotogramas.
    void resetSource();
    void setCurrentMask(int mask);

signals:
    void maskSelected(int mask);

protected:
    bool eventFilter(QObject* watched, QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void rebuildThumbnails();
    void refreshTile(int mask);
    void relayoutTiles();
    void showPreview(int mask, const QPoint& globalPos);
    void hidePreview();

    QVector<FilterTile*> m_tiles;
    QWidget* m_grid = nullptr;
    QImage   m_source;            // ultimo fotograma reducido (o la muestra)
    int      m_current = 0;
    // Cuantas entregas mas rehacen todas las miniaturas. Unas pocas, no una:
    // el primer fotograma de un video suele ser negro.
    int      m_fullRefreshes = 3;

    // Vista grande al pasar el raton: ventana emergente sin marco.
    QLabel*  m_preview = nullptr;
    int      m_previewMask = -1;
    QPoint   m_previewPos;
};
