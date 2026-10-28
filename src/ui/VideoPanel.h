#pragma once

#include "core/Subtitles.h"
#include "ui/SubtitleStyle.h"

#include <QColor>
#include <QImage>
#include <QWidget>

#include <atomic>

class AudioEngine;
class FlatButton;
class QMediaPlayer;
class QPainter;
class QVideoSink;
class QThread;
class QTimer;
class VideoFrameProcessor;

// Panel de video: dibuja la imagen de lo que reproduce AudioEngine cuando es
// un video, con la mascara elegida (ver VideoMasks) y sin deformarla.
//
// El clic derecho pide el menu de Video y el doble clic, la pantalla
// completa: los dos los resuelve MainWindow. Gestos sobre la imagen:
// arrastrar a los lados (o la rueda) se mueve por el video con una
// previsualizacion del punto de llegada, y arrastrar arriba o abajo cambia el
// volumen con una barra en pantalla. Al mover el raton aparece una barra con
// los filtros a mano.
class VideoPanel : public QWidget {
    Q_OBJECT

public:
    explicit VideoPanel(AudioEngine* engine, QWidget* parent = nullptr);
    ~VideoPanel() override;

    int  mask() const { return m_mask; }
    void setMask(int mask);

    // Fotogramas que se mueve el video por cada 100 px de arrastre.
    void setScrubFramesPer100(int frames) { m_scrubPer100 = std::max(1, frames); }
    // Lo que avanza cada paso de la rueda (el mismo que las flechas).
    void setSeekStepMs(int ms) { m_seekStepMs = std::max(100, ms); }
    void setGestures(bool seekDrag, bool volumeDrag, bool wheel);
    void setPreviewWidth(int px) { m_previewWidth = std::clamp(px, 120, 640); }

    // Barra de volumen en pantalla.
    enum VolumeShape { VolumeVertical = 0, VolumeHorizontal, VolumeRing, VolumeSegments,
                       VolumeShapeCount };
    static QString volumeShapeName(int shape);
    void setVolumeBarStyle(int shape, const QColor& color);   // color invalido: acento
    void showVolumeOsd(float volume);
    // Aviso de un salto hecho con el teclado ("+5 s  1:23.40").
    void showSeekOsd(qint64 deltaMs, qint64 targetMs);
    // Un aviso de texto arriba (la mascara puesta, por ejemplo).
    void showNotice(const QString& text, int ms = 2500);

    // Archivo del que salen las previsualizaciones (el video en curso).
    void setPreviewSource(const QString& path);

    // Sin video: vuelve al aviso de "abre un video".
    void clear();

    // Fotogramas que llegaron y que se llegaron a pintar desde la ultima
    // llamada (para medir el rendimiento).
    void takeFrameStats(int* received, int* painted)
    {
        *received = m_framesReceived;
        *painted = m_framesPainted;
        m_framesReceived = m_framesPainted = 0;
    }

    // Subtitulos de un archivo aparte. Mientras haya, mandan sobre los que
    // traiga el video (que conviene desactivar para no ver los dos).
    bool loadSubtitles(const QString& path);
    void clearSubtitles();
    QString subtitlesPath() const { return m_subtitles.path(); }

    // Letra, posicion y fondo de los subtitulos (ver SubtitleStyle).
    void setSubtitleStyle(const SubtitleStyle& style);

    // Mientras alguien quiera miniaturas (la paleta de filtros), cada poco se
    // entrega una copia reducida del fotograma, sin mascara.
    void setThumbnailsWanted(bool wanted);

    // En pantalla completa el cursor se esconde tras unos segundos quieto y
    // vuelve al mover el raton.
    void setCursorAutoHide(bool on);

signals:
    void menuRequested(const QPoint& globalPos);
    void fullscreenRequested();
    void playPauseRequested();     // un clic sobre la imagen
    void filtersRequested();       // boton "Filtros" de la barra flotante
    void nextMaskRequested();      // boton de la mascara
    void volumeRequested(float volume);   // arrastre vertical
    void thumbnailSourceChanged(const QImage& frame);

protected:
    bool event(QEvent* event) override;
    void paintEvent(QPaintEvent* event) override;
    void contextMenuEvent(QContextMenuEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void leaveEvent(QEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void showEvent(QShowEvent* event) override;
    void hideEvent(QHideEvent* event) override;

private:
    void onFrameReady(const QImage& image);
    void updateTargetSize();

    AudioEngine* m_engine = nullptr;
    QImage    m_frame;           // ultimo fotograma: escalado, con la mascara

    // La conversion, el escalado y la mascara van en un hilo propio.
    VideoFrameProcessor* m_processor = nullptr;
    QThread*             m_processorThread = nullptr;
    std::atomic<bool>    m_framesWanted{false};   // visible: se procesan
    int       m_mask = 0;
    int       m_framesReceived = 0;
    int       m_framesPainted = 0;
    QString   m_embeddedText;    // subtitulo incorporado en curso
    Subtitles m_subtitles;       // los de archivo aparte
    SubtitleStyle m_subtitleStyle;

    // --- barra flotante ----------------------------------------------------
    void buildOverlay();
    void showOverlay();
    void placeOverlay();
    QWidget*    m_overlay = nullptr;
    FlatButton* m_maskButton = nullptr;
    QTimer*     m_overlayTimer = nullptr;

    // Un clic es pausa / play, pero espera un poco por si es un doble clic
    // (pantalla completa), que no debe pausar.
    QTimer*     m_clickTimer = nullptr;
    bool        m_clickToggled = false;   // el clic ya actuo antes del doble

    QTimer*     m_cursorTimer = nullptr;
    bool        m_cursorAutoHide = false;
    void wakeCursor();

    // --- gestos ---------------------------------------------------------------
    enum Gesture { GestureNone, GestureSeek, GestureVolume };
    int     m_scrubPer100 = 60;
    int     m_seekStepMs = 5000;
    bool    m_seekDragOn = true;
    bool    m_volumeDragOn = true;
    bool    m_wheelOn = true;
    bool    m_pressed = false;
    Gesture m_gesture = GestureNone;
    bool    m_wasPlaying = false;
    QPointF m_pressPos;
    qint64  m_pressMs = 0;
    float   m_pressVolume = 0.0f;
    int     m_scrubFrames = 0;
    qint64  m_scrubTargetMs = 0;

    // Rueda: los pasos se acumulan y el salto se hace al dejar de girarla.
    QTimer* m_wheelCommit = nullptr;
    bool    m_wheelActive = false;
    qint64  m_wheelBaseMs = 0;
    qint64  m_wheelTargetMs = 0;
    double  m_wheelNotches = 0.0;

    // --- previsualizacion del punto de llegada --------------------------------
    // Un segundo reproductor, mudo y en pausa, sobre el mismo archivo: se
    // salta en el y su fotograma es la miniatura, sin tocar el video real.
    void requestPreview(qint64 ms);
    void onPreviewFrame(const QImage& image);
    void showPreviewAt(double x, qint64 targetMs, qint64 deltaMs);
    void hidePreview();
    void paintPreview(QPainter& p);
    QMediaPlayer* m_previewPlayer = nullptr;
    QVideoSink*   m_previewSink = nullptr;
    QString m_previewPath;
    QImage  m_previewImage;
    bool    m_previewShown = false;
    double  m_previewX = 0.0;
    qint64  m_previewMs = 0;
    qint64  m_previewDeltaMs = 0;
    bool    m_previewBusy = false;
    qint64  m_previewPending = -1;
    qint64  m_previewAskedAt = 0;
    int     m_previewWidth = 260;

    // --- avisos en pantalla -------------------------------------------------
    void paintVolumeOsd(QPainter& p);
    void paintLabel(QPainter& p, const QString& text);
    QTimer* m_osdTimer = nullptr;
    float   m_osdVolume = 0.0f;
    qint64  m_volumeOsdUntil = 0;
    QString m_seekLabel;
    qint64  m_seekLabelUntil = 0;
    int     m_volumeShape = VolumeVertical;
    QColor  m_volumeColor;
};
