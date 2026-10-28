#pragma once

#include <QImage>
#include <QMutex>
#include <QObject>
#include <QSize>
#include <QVideoFrame>

// Prepara los fotogramas del video fuera del hilo de la interfaz: los pasa a
// RGB, los escala al tamano exacto en pantalla y les aplica la mascara. La
// interfaz solo tiene que copiar el resultado, sin escalar nada.
//
// Si llegan fotogramas mas deprisa de lo que se procesan, se trabaja siempre
// con el ultimo y los atrasados se descartan: el video puede saltarse algun
// fotograma en un equipo lento, pero nunca se acumula retraso ni se bloquea
// la interfaz (ni el audio, que antes compartia hilo con todo esto).
class VideoFrameProcessor : public QObject {
    Q_OBJECT

public:
    explicit VideoFrameProcessor(QObject* parent = nullptr);

    // Se pueden llamar desde cualquier hilo.
    void submit(const QVideoFrame& frame);
    void setTargetSize(const QSize& devicePixels);   // area del panel
    void setMask(int mask);
    void setThumbnailsWanted(bool wanted);
    // Vuelve a preparar el ultimo fotograma (tras cambiar mascara o tamano
    // con el video en pausa).
    void reprocess();
    void clear();

signals:
    // Imagen lista para pintar tal cual, ya escalada y con la mascara.
    void frameReady(const QImage& image);
    // Copia reducida y sin mascara, para las miniaturas de la paleta.
    void thumbnailReady(const QImage& image);

private:
    void schedule();
    void process();

    QMutex      m_mutex;
    QVideoFrame m_pending;          // ultimo fotograma sin procesar
    QVideoFrame m_last;             // ultimo procesado, para reprocess()
    bool        m_scheduled = false;
    QSize       m_target;
    int         m_mask = 0;
    bool        m_thumbnailsWanted = false;
    qint64      m_lastThumbnailMs = 0;
};
