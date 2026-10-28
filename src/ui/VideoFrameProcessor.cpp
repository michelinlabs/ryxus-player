#include "ui/VideoFrameProcessor.h"

#include "ui/VideoMasks.h"

#include <QDateTime>
#include <QMutexLocker>

VideoFrameProcessor::VideoFrameProcessor(QObject* parent)
    : QObject(parent)
{
}

void VideoFrameProcessor::submit(const QVideoFrame& frame)
{
    {
        QMutexLocker lock(&m_mutex);
        m_pending = frame;   // el anterior, si no llego a procesarse, se pierde
    }
    schedule();
}

void VideoFrameProcessor::setTargetSize(const QSize& devicePixels)
{
    QMutexLocker lock(&m_mutex);
    m_target = devicePixels;
}

void VideoFrameProcessor::setMask(int mask)
{
    QMutexLocker lock(&m_mutex);
    m_mask = mask;
}

void VideoFrameProcessor::setThumbnailsWanted(bool wanted)
{
    QMutexLocker lock(&m_mutex);
    m_thumbnailsWanted = wanted;
}

void VideoFrameProcessor::reprocess()
{
    {
        QMutexLocker lock(&m_mutex);
        if (!m_pending.isValid())
            m_pending = m_last;
    }
    schedule();
}

void VideoFrameProcessor::clear()
{
    QMutexLocker lock(&m_mutex);
    m_pending = QVideoFrame();
    m_last = QVideoFrame();
}

void VideoFrameProcessor::schedule()
{
    // Una sola tarea en cola a la vez: si ya hay una, recogera el fotograma
    // mas reciente cuando le toque.
    {
        QMutexLocker lock(&m_mutex);
        if (m_scheduled || !m_pending.isValid())
            return;
        m_scheduled = true;
    }
    QMetaObject::invokeMethod(this, &VideoFrameProcessor::process, Qt::QueuedConnection);
}

void VideoFrameProcessor::process()
{
    QVideoFrame frame;
    QSize target;
    int mask = 0;
    bool thumbnails = false;
    {
        QMutexLocker lock(&m_mutex);
        frame = m_pending;
        m_pending = QVideoFrame();
        m_scheduled = false;
        target = m_target;
        mask = m_mask;
        thumbnails = m_thumbnailsWanted;
        if (frame.isValid())
            m_last = frame;
    }
    if (!frame.isValid() || target.isEmpty())
        return;

    QImage image = frame.toImage();
    if (image.isNull())
        return;

    // Miniaturas: un par de veces por segundo, del fotograma sin mascara.
    if (thumbnails) {
        const qint64 now = QDateTime::currentMSecsSinceEpoch();
        if (now - m_lastThumbnailMs >= 400) {
            m_lastThumbnailMs = now;
            emit thumbnailReady(image.scaled(480, 270, Qt::KeepAspectRatio,
                                             Qt::FastTransformation));
        }
    }

    // Al tamano exacto en pantalla. La mascara va sobre la mas pequena de
    // las dos: en una ventana normal se reduce el 1080p antes de colorearlo,
    // y es la mitad de pixeles que tocar.
    const QSize fitted = image.size().scaled(target, Qt::KeepAspectRatio);
    if (fitted.width() * fitted.height() < image.width() * image.height()) {
        image = image.scaled(fitted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB32);
        VideoMasks::apply(image, mask);
    } else {
        image = image.convertToFormat(QImage::Format_RGB32);
        VideoMasks::apply(image, mask);
        if (fitted != image.size())
            image = image.scaled(fitted, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    emit frameReady(image);
}
