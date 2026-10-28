#include "ui/VideoPanel.h"

#include "core/AudioEngine.h"
#include "core/Lang.h"
#include "core/TrackInfo.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"
#include "ui/VideoMasks.h"

#include <QHBoxLayout>
#include <QTimer>

#include <QContextMenuEvent>
#include <QDateTime>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QThread>
#include "ui/VideoFrameProcessor.h"
#include <QVideoFrame>
#include <QVideoSink>
#include <QMediaPlayer>
#include <QUrl>
#include <QWheelEvent>
#include <limits>

namespace {

// Pastilla oscura detras de los botones de la barra flotante: se leen sobre
// cualquier imagen.
class OverlayBar : public QWidget {
public:
    using QWidget::QWidget;

protected:
    void paintEvent(QPaintEvent*) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(0, 0, 0, 170));
        p.drawRoundedRect(QRectF(rect()), 8, 8);
    }
};

constexpr int kScrubThreshold = 6;   // px antes de dar un clic por arrastre

QString formatPrecise(qint64 ms)
{
    // "1:02.45": con centesimas, que entre fotograma y fotograma importan.
    ms = std::max<qint64>(0, ms);
    return QStringLiteral("%1:%2.%3")
        .arg(ms / 60000)
        .arg((ms / 1000) % 60, 2, 10, QLatin1Char('0'))
        .arg((ms % 1000) / 10, 2, 10, QLatin1Char('0'));
}

} // namespace

VideoPanel::VideoPanel(AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setObjectName(QStringLiteral("videoPanel"));
    setMinimumSize(320, 180);
    setMouseTracking(true);
    // Pinta todo su rectangulo: Qt no repinta lo que hay debajo.
    setAttribute(Qt::WA_OpaquePaintEvent);
    setFocusPolicy(Qt::ClickFocus);   // un clic en la imagen le da el teclado
    setToolTip(Lang::tr("Arrastra a izquierda o derecha para retroceder o avanzar "
                        "fotogramas\nDoble clic: pantalla completa - Clic derecho: menu de video"));
    m_subtitleStyle = SubtitleStyle::load();
    buildOverlay();

    // Hilo propio para preparar la imagen: el de la interfaz solo la copia.
    m_processorThread = new QThread(this);
    m_processorThread->setObjectName(QStringLiteral("RyxusVideoFrames"));
    m_processor = new VideoFrameProcessor;
    m_processor->moveToThread(m_processorThread);
    connect(m_processorThread, &QThread::finished, m_processor, &QObject::deleteLater);
    connect(m_processor, &VideoFrameProcessor::frameReady, this, &VideoPanel::onFrameReady);
    connect(m_processor, &VideoFrameProcessor::thumbnailReady,
            this, &VideoPanel::thumbnailSourceChanged);
    m_processorThread->start();

    // Previsualizaciones: reproductor aparte, sin sonido y siempre en pausa.
    m_previewPlayer = new QMediaPlayer(this);
    m_previewSink = new QVideoSink(this);
    m_previewPlayer->setVideoSink(m_previewSink);
    connect(m_previewPlayer, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
        // En pausa, cada salto entrega su fotograma; parado, no.
        if (status == QMediaPlayer::LoadedMedia)
            m_previewPlayer->pause();
    });
    // Directa: el fotograma se reduce en el hilo de FFmpeg y llega ya
    // pequeno al de la interfaz.
    connect(m_previewSink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
        if (!frame.isValid())
            return;
        QImage image = frame.toImage();
        if (image.isNull())
            return;
        image = image.scaled(640, 360, Qt::KeepAspectRatio, Qt::SmoothTransformation)
                    .convertToFormat(QImage::Format_RGB32);
        QMetaObject::invokeMethod(this, [this, image]() mutable {
            VideoMasks::apply(image, m_mask);
            onPreviewFrame(image);
        }, Qt::QueuedConnection);
    }, Qt::DirectConnection);

    m_wheelCommit = new QTimer(this);
    m_wheelCommit->setSingleShot(true);
    m_wheelCommit->setInterval(450);
    connect(m_wheelCommit, &QTimer::timeout, this, [this]() {
        m_wheelActive = false;
        m_engine->seekMs(m_wheelTargetMs);
        m_seekLabelUntil = QDateTime::currentMSecsSinceEpoch() + 900;
        m_osdTimer->start(950);
        hidePreview();
    });

    m_osdTimer = new QTimer(this);
    m_osdTimer->setSingleShot(true);
    connect(m_osdTimer, &QTimer::timeout, this, qOverload<>(&QWidget::update));

    if (QVideoSink* sink = engine->videoSink()) {
        // Directa: el fotograma pasa al procesador desde el hilo de FFmpeg sin
        // hacer cola en el de la interfaz.
        connect(sink, &QVideoSink::videoFrameChanged, this, [this](const QVideoFrame& frame) {
            if (m_framesWanted.load(std::memory_order_relaxed) && frame.isValid())
                m_processor->submit(frame);
        }, Qt::DirectConnection);
        connect(sink, &QVideoSink::subtitleTextChanged, this, [this](const QString& text) {
            m_embeddedText = text;
            update();
        });
    }
}

VideoPanel::~VideoPanel()
{
    m_previewPlayer->stop();
    m_processorThread->quit();
    m_processorThread->wait();
}

void VideoPanel::setThumbnailsWanted(bool wanted)
{
    m_processor->setThumbnailsWanted(wanted);
}

void VideoPanel::updateTargetSize()
{
    // En pixeles de pantalla: la imagen sale ya a la medida y se pinta 1:1.
    m_processor->setTargetSize((QSizeF(size()) * devicePixelRatioF()).toSize());
}

void VideoPanel::showEvent(QShowEvent* event)
{
    QWidget::showEvent(event);
    updateTargetSize();
    m_framesWanted.store(true, std::memory_order_relaxed);
    m_processor->reprocess();
}

void VideoPanel::hideEvent(QHideEvent* event)
{
    // Tapado por otro panel o minimizado: no se gasta en imagenes que nadie ve.
    m_framesWanted.store(false, std::memory_order_relaxed);
    QWidget::hideEvent(event);
}

void VideoPanel::onFrameReady(const QImage& image)
{
    if (!m_framesWanted.load(std::memory_order_relaxed))
        return;
    ++m_framesReceived;
    m_frame = image;
    m_frame.setDevicePixelRatio(devicePixelRatioF());
    update();
}

bool VideoPanel::loadSubtitles(const QString& path)
{
    const bool ok = m_subtitles.load(path);
    update();
    return ok;
}

void VideoPanel::setSubtitleStyle(const SubtitleStyle& style)
{
    m_subtitleStyle = style;
    update();
}

void VideoPanel::clearSubtitles()
{
    m_subtitles.clear();
    update();
}

void VideoPanel::setMask(int mask)
{
    m_mask = mask;
    m_processor->setMask(mask);
    m_processor->reprocess();   // en pausa tambien se ve el cambio
    if (m_maskButton) {
        m_maskButton->setText(Lang::tr("Mascara: %1").arg(VideoMasks::name(mask)));
        m_maskButton->adjustSize();
        placeOverlay();
    }
    update();
}

// --------------------------------------------------------------- barra flotante

void VideoPanel::buildOverlay()
{
    // Al mover el raton sobre el video: los filtros a la vista, sin tener que
    // buscarlos en un menu. Se esconde sola al dejar de moverlo.
    m_overlay = new OverlayBar(this);
    auto* layout = new QHBoxLayout(m_overlay);
    layout->setContentsMargins(8, 4, 8, 4);
    layout->setSpacing(6);

    const auto makeButton = [this](Icons::Icon icon, const QString& text) {
        auto* button = new FlatButton(text, m_overlay);
        button->setIconId(icon);
        button->setGlyphSize(14);
        button->setFixedHeight(28);
        button->setColors(QColor(235, 235, 240), Theme::AccentBright, Theme::AccentBright);
        button->setCursor(Qt::PointingHandCursor);
        return button;
    };

    FlatButton* filters = makeButton(Icons::Image, Lang::tr("Filtros"));
    filters->setToolTip(Lang::tr("Mostrar u ocultar la paleta de filtros de video"));
    connect(filters, &FlatButton::clicked, this, &VideoPanel::filtersRequested);
    layout->addWidget(filters);

    m_maskButton = makeButton(Icons::Equalizer, Lang::tr("Mascara: %1").arg(VideoMasks::name(m_mask)));
    m_maskButton->setToolTip(Lang::tr("Pasar a la mascara siguiente (Alt+M)"));
    connect(m_maskButton, &FlatButton::clicked, this, &VideoPanel::nextMaskRequested);
    layout->addWidget(m_maskButton);

    FlatButton* fullscreen = makeButton(Icons::WinMaximize, Lang::tr("Pantalla completa"));
    connect(fullscreen, &FlatButton::clicked, this, &VideoPanel::fullscreenRequested);
    layout->addWidget(fullscreen);

    m_overlay->adjustSize();
    m_overlay->hide();

    m_overlayTimer = new QTimer(this);
    m_overlayTimer->setSingleShot(true);
    m_overlayTimer->setInterval(2500);
    connect(m_overlayTimer, &QTimer::timeout, this, [this]() {
        // Si el raton esta encima de la propia barra, se queda.
        if (!m_overlay->underMouse())
            m_overlay->hide();
        else
            m_overlayTimer->start();
    });
}

void VideoPanel::placeOverlay()
{
    if (!m_overlay)
        return;
    m_overlay->adjustSize();
    m_overlay->move(width() - m_overlay->width() - 14, height() - m_overlay->height() - 14);
}

void VideoPanel::showOverlay()
{
    if (m_frame.isNull() || m_gesture != GestureNone)
        return;
    placeOverlay();
    m_overlay->show();
    m_overlay->raise();
    m_overlayTimer->start();
}

void VideoPanel::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    placeOverlay();
    updateTargetSize();
    m_processor->reprocess();
}

void VideoPanel::leaveEvent(QEvent* event)
{
    QWidget::leaveEvent(event);
    if (m_overlay && !m_overlay->underMouse())
        m_overlay->hide();
}

// ---------------------------------------------------------------- gestos

void VideoPanel::setGestures(bool seekDrag, bool volumeDrag, bool wheel)
{
    m_seekDragOn = seekDrag;
    m_volumeDragOn = volumeDrag;
    m_wheelOn = wheel;
}

void VideoPanel::mousePressEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton || m_frame.isNull()) {
        QWidget::mousePressEvent(event);
        return;
    }
    m_pressed = true;
    m_gesture = GestureNone;
    m_pressPos = event->position();
    m_pressMs = m_engine->positionMs();
    m_pressVolume = m_engine->volume();
    m_scrubFrames = 0;
}

void VideoPanel::mouseMoveEvent(QMouseEvent* event)
{
    if (!m_pressed) {
        showOverlay();
        return;
    }

    const QPointF delta = event->position() - m_pressPos;
    if (m_gesture == GestureNone) {
        // Unos pixeles de margen, y el eje que domine decide el gesto: un clic
        // con pulso no mueve nada, y un arrastre algo torcido no hace los dos.
        if (std::max(std::abs(delta.x()), std::abs(delta.y())) < kScrubThreshold)
            return;
        const bool horizontal = std::abs(delta.x()) >= std::abs(delta.y());
        if (horizontal && m_seekDragOn) {
            m_gesture = GestureSeek;
            m_wasPlaying = m_engine->isPlaying();
            if (m_wasPlaying)
                m_engine->pause();
            setCursor(Qt::SizeHorCursor);
        } else if (!horizontal && m_volumeDragOn) {
            m_gesture = GestureVolume;
            setCursor(Qt::SizeVerCursor);
        } else {
            m_pressed = false;   // gesto desactivado en el menu: no se hace nada
            return;
        }
        m_overlay->hide();
    }

    if (m_gesture == GestureVolume) {
        // Arriba sube, abajo baja: tres cuartos del alto del panel es de 0 a 100 %.
        const float volume = std::clamp(
            m_pressVolume - float(delta.y() / (height() * 0.75)), 0.0f, 1.0f);
        emit volumeRequested(volume);
        showVolumeOsd(volume);
        return;
    }

    // A la derecha avanza, a la izquierda retrocede; cuanto mas lejos, mas
    // fotogramas, segun la sensibilidad elegida en el menu de Video. El video
    // no se mueve hasta soltar: mientras, la previsualizacion ensena adonde.
    const double dx = delta.x();
    const double travel = dx - std::copysign(double(kScrubThreshold), dx);
    const int frames = int(std::lround(travel * m_scrubPer100 / 100.0));
    m_previewX = event->position().x();
    if (frames == m_scrubFrames && m_previewShown) {
        update();
        return;
    }
    m_scrubFrames = frames;

    const qint64 duration = m_engine->durationMs();
    qint64 target = m_pressMs + qint64(std::llround(frames * m_engine->videoFrameMs()));
    target = std::clamp<qint64>(target, 0, duration > 0 ? duration - 1 : target);
    m_scrubTargetMs = target;

    const QString arrow = frames >= 0 ? QStringLiteral("\u25B6\u25B6") : QStringLiteral("\u25C0\u25C0");
    m_seekLabel = QStringLiteral("%1  %2%3 %4  \u00B7  %5")
        .arg(arrow, frames > 0 ? QStringLiteral("+") : QString())
        .arg(frames)
        .arg(Lang::tr("fotogramas"), formatPrecise(target));
    m_seekLabelUntil = std::numeric_limits<qint64>::max();
    showPreviewAt(m_previewX, target, target - m_pressMs);
}

void VideoPanel::mouseReleaseEvent(QMouseEvent* event)
{
    if (event->button() != Qt::LeftButton) {
        QWidget::mouseReleaseEvent(event);
        return;
    }
    const Gesture gesture = m_gesture;
    m_pressed = false;
    m_gesture = GestureNone;
    unsetCursor();

    if (gesture == GestureSeek) {
        m_engine->seekMs(m_scrubTargetMs);
        if (m_wasPlaying)
            m_engine->play();
        m_seekLabelUntil = QDateTime::currentMSecsSinceEpoch() + 900;
        m_osdTimer->start(950);
        hidePreview();
    }
}

void VideoPanel::wheelEvent(QWheelEvent* event)
{
    if (!m_wheelOn || m_frame.isNull()) {
        QWidget::wheelEvent(event);
        return;
    }

    // Cada paso de la rueda mueve lo mismo que una flecha del teclado. Los
    // pasos se acumulan con la previsualizacion a la vista y el salto se hace
    // al dejar de girar, no a cada muesca.
    if (!m_wheelActive) {
        m_wheelActive = true;
        m_wheelBaseMs = m_engine->positionMs();
        m_wheelTargetMs = m_wheelBaseMs;
        m_wheelNotches = 0.0;
    }
    m_wheelNotches += event->angleDelta().y() / 120.0;
    const qint64 duration = m_engine->durationMs();
    qint64 target = m_wheelBaseMs + qint64(std::llround(m_wheelNotches * m_seekStepMs));
    target = std::clamp<qint64>(target, 0, duration > 0 ? duration - 1 : target);
    m_wheelTargetMs = target;

    const qint64 delta = target - m_wheelBaseMs;
    m_seekLabel = QStringLiteral("%1  %2%3 s  \u00B7  %4")
        .arg(delta >= 0 ? QStringLiteral("\u25B6\u25B6") : QStringLiteral("\u25C0\u25C0"),
             delta > 0 ? QStringLiteral("+") : QString())
        .arg(QString::number(delta / 1000.0, 'f', delta % 1000 == 0 ? 0 : 1))
        .arg(formatPrecise(target));
    m_seekLabelUntil = std::numeric_limits<qint64>::max();
    showPreviewAt(event->position().x(), target, delta);
    m_wheelCommit->start();
    event->accept();
}

// ------------------------------------------- previsualizacion del destino

void VideoPanel::setPreviewSource(const QString& path)
{
    if (path == m_previewPath)
        return;
    m_previewPath = path;
    m_previewImage = QImage();
    m_previewBusy = false;
    m_previewPending = -1;
    m_previewPlayer->setSource(path.isEmpty() ? QUrl() : QUrl::fromLocalFile(path));
}

void VideoPanel::requestPreview(qint64 ms)
{
    if (m_previewPath.isEmpty())
        return;
    // Un salto a la vez: si el anterior no ha devuelto su fotograma, este
    // espera y se queda solo el ultimo pedido. Si tarda demasiado (un
    // fotograma que nunca llega), se pide de nuevo.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (m_previewBusy && now - m_previewAskedAt < 300) {
        m_previewPending = ms;
        return;
    }
    m_previewBusy = true;
    m_previewAskedAt = now;
    m_previewPending = -1;
    m_previewPlayer->setPosition(ms);
}

void VideoPanel::onPreviewFrame(const QImage& image)
{
    m_previewImage = image;
    m_previewBusy = false;
    if (m_previewPending >= 0) {
        const qint64 next = m_previewPending;
        m_previewPending = -1;
        requestPreview(next);
    }
    if (m_previewShown)
        update();
}

void VideoPanel::showPreviewAt(double x, qint64 targetMs, qint64 deltaMs)
{
    m_previewShown = true;
    m_previewX = x;
    m_previewDeltaMs = deltaMs;
    if (targetMs != m_previewMs || m_previewImage.isNull()) {
        m_previewMs = targetMs;
        requestPreview(targetMs);
    }
    update();
}

void VideoPanel::hidePreview()
{
    m_previewShown = false;
    update();
}

void VideoPanel::paintPreview(QPainter& p)
{
    // Una miniatura del punto de llegada, sobre el cursor y hacia abajo del
    // panel, con su instante debajo.
    const int w = m_previewWidth;
    const double aspect = m_previewImage.isNull()
        ? 9.0 / 16.0 : double(m_previewImage.height()) / m_previewImage.width();
    const int h = int(w * aspect);
    const int labelH = 26;
    const int x = std::clamp(int(m_previewX) - w / 2, 8, std::max(8, width() - w - 8));
    const int y = std::max(8, height() - h - labelH - 70);
    const QRect box(x, y, w, h);

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 200));
    p.drawRoundedRect(box.adjusted(-4, -4, 4, labelH + 4), 8, 8);

    if (m_previewImage.isNull()) {
        p.fillRect(box, QColor(20, 20, 24));
        p.setPen(Theme::TextFaint);
        p.setFont(Theme::uiFont(9));
        p.drawText(box, Qt::AlignCenter, QStringLiteral("\u2026"));
    } else {
        p.drawImage(box, m_previewImage);
    }
    p.setPen(QPen(Theme::AccentBright, 2.0));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(box).adjusted(-1, -1, 1, 1), 4, 4);

    const qint64 d = m_previewDeltaMs;
    const QString label = QStringLiteral("%1   (%2%3)")
        .arg(formatPrecise(m_previewMs), d >= 0 ? QStringLiteral("+") : QStringLiteral("-"),
             formatPrecise(std::abs(d)));
    p.setPen(Qt::white);
    p.setFont(Theme::uiFont(9, QFont::DemiBold));
    p.drawText(QRect(x, y + h + 2, w, labelH), Qt::AlignCenter, label);
    p.restore();
}

// ------------------------------------------------------- avisos en pantalla

QString VideoPanel::volumeShapeName(int shape)
{
    switch (shape) {
    case VolumeVertical:   return Lang::tr("Barra vertical");
    case VolumeHorizontal: return Lang::tr("Barra horizontal");
    case VolumeRing:       return Lang::tr("Anillo");
    case VolumeSegments:   return Lang::tr("Segmentos");
    }
    return QString();
}

void VideoPanel::setVolumeBarStyle(int shape, const QColor& color)
{
    m_volumeShape = std::clamp(shape, 0, VolumeShapeCount - 1);
    m_volumeColor = color;
    update();
}

void VideoPanel::showVolumeOsd(float volume)
{
    m_osdVolume = volume;
    m_volumeOsdUntil = QDateTime::currentMSecsSinceEpoch() + 1200;
    m_osdTimer->start(1250);
    update();
}

void VideoPanel::showSeekOsd(qint64 deltaMs, qint64 targetMs)
{
    m_seekLabel = QStringLiteral("%1  %2%3 s  \u00B7  %4")
        .arg(deltaMs >= 0 ? QStringLiteral("\u25B6\u25B6") : QStringLiteral("\u25C0\u25C0"),
             deltaMs > 0 ? QStringLiteral("+") : QString())
        .arg(QString::number(deltaMs / 1000.0, 'f', deltaMs % 1000 == 0 ? 0 : 1))
        .arg(formatPrecise(targetMs));
    m_seekLabelUntil = QDateTime::currentMSecsSinceEpoch() + 900;
    m_osdTimer->start(950);
    update();
}

void VideoPanel::paintLabel(QPainter& p, const QString& text)
{
    QFont font = Theme::uiFont(12, QFont::DemiBold);
    p.setFont(font);
    const QFontMetrics metrics(font);
    const QRect box(QPoint(0, 0), QSize(metrics.horizontalAdvance(text) + 36,
                                        metrics.height() + 18));
    const QRect at = box.translated((width() - box.width()) / 2, 22);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(0, 0, 0, 180));
    p.drawRoundedRect(at, 10, 10);
    p.setPen(Qt::white);
    p.drawText(at, Qt::AlignCenter, text);
}

void VideoPanel::paintVolumeOsd(QPainter& p)
{
    const QColor color = m_volumeColor.isValid() ? m_volumeColor : Theme::AccentBright;
    const QColor track(255, 255, 255, 55);
    const QColor plate(0, 0, 0, 170);
    const double v = std::clamp(double(m_osdVolume), 0.0, 1.0);
    const QString percent = QStringLiteral("%1 %").arg(qRound(v * 100));

    p.save();
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setFont(Theme::uiFont(10, QFont::DemiBold));

    switch (m_volumeShape) {
    case VolumeHorizontal: {
        const int w = std::min(420, int(width() * 0.45));
        const QRect plateRect((width() - w) / 2 - 18, int(height() * 0.78) - 16, w + 36, 58);
        p.setPen(Qt::NoPen);
        p.setBrush(plate);
        p.drawRoundedRect(plateRect, 12, 12);
        const QRectF bar(plateRect.left() + 18, plateRect.top() + 34, w, 10);
        p.setBrush(track);
        p.drawRoundedRect(bar, 5, 5);
        p.setBrush(color);
        p.drawRoundedRect(QRectF(bar.left(), bar.top(), bar.width() * v, bar.height()), 5, 5);
        p.setPen(Qt::white);
        p.drawText(QRect(plateRect.left(), plateRect.top() + 6, plateRect.width(), 24),
                   Qt::AlignCenter, Lang::tr("Volumen %1").arg(percent));
        break;
    }
    case VolumeRing: {
        const int r = 62;
        const QPointF c(width() / 2.0, height() / 2.0);
        p.setPen(Qt::NoPen);
        p.setBrush(plate);
        p.drawEllipse(c, r + 18, r + 18);
        const QRectF arc(c.x() - r, c.y() - r, 2 * r, 2 * r);
        p.setBrush(Qt::NoBrush);
        p.setPen(QPen(track, 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 0, 360 * 16);
        p.setPen(QPen(color, 10, Qt::SolidLine, Qt::RoundCap));
        p.drawArc(arc, 90 * 16, -int(360 * 16 * v));   // desde arriba, en horario
        p.setPen(Qt::white);
        p.setFont(Theme::uiFont(16, QFont::DemiBold));
        p.drawText(arc, Qt::AlignCenter, percent);
        break;
    }
    case VolumeSegments: {
        const int count = 10, segW = 22, segH = 30, gap = 6;
        const int total = count * segW + (count - 1) * gap;
        const QRect plateRect((width() - total) / 2 - 18, int(height() * 0.5) - 40,
                              total + 36, segH + 62);
        p.setPen(Qt::NoPen);
        p.setBrush(plate);
        p.drawRoundedRect(plateRect, 12, 12);
        const int lit = int(std::lround(v * count));
        for (int i = 0; i < count; ++i) {
            const QRectF seg(plateRect.left() + 18 + i * (segW + gap),
                             plateRect.top() + 46 + (segH - segH * (i + 4) / (count + 3)),
                             segW, segH * (i + 4) / double(count + 3));
            p.setBrush(i < lit ? color : track);
            p.drawRoundedRect(seg, 3, 3);
        }
        p.setPen(Qt::white);
        p.drawText(QRect(plateRect.left(), plateRect.top() + 8, plateRect.width(), 26),
                   Qt::AlignCenter, Lang::tr("Volumen %1").arg(percent));
        break;
    }
    case VolumeVertical:
    default: {
        const int h = std::min(300, int(height() * 0.5));
        const QRect plateRect(width() - 92, (height() - h) / 2 - 40, 64, h + 80);
        p.setPen(Qt::NoPen);
        p.setBrush(plate);
        p.drawRoundedRect(plateRect, 12, 12);
        const QRectF bar(plateRect.center().x() - 6, plateRect.top() + 46, 12, h);
        p.setBrush(track);
        p.drawRoundedRect(bar, 6, 6);
        p.setBrush(color);
        const double filled = bar.height() * v;
        p.drawRoundedRect(QRectF(bar.left(), bar.bottom() - filled, bar.width(), filled), 6, 6);
        p.setPen(Qt::white);
        p.drawText(QRect(plateRect.left(), plateRect.top() + 10, plateRect.width(), 26),
                   Qt::AlignCenter, percent);
        break;
    }
    }
    p.restore();
}

void VideoPanel::clear()
{
    m_processor->clear();
    m_frame = QImage();
    m_embeddedText.clear();
    update();
}

void VideoPanel::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.fillRect(rect(), Qt::black);

    if (m_frame.isNull()) {
        p.setPen(Theme::TextFaint);
        p.setFont(Theme::uiFont(10));
        p.drawText(rect(), Qt::AlignCenter,
                   Lang::tr("Abre un video para verlo aqui\n(clic derecho: menu de video)"));
        return;
    }

    // La imagen llega ya escalada a la medida del panel: se copia 1:1,
    // centrada. Si el panel acaba de cambiar de tamano y aun no ha llegado la
    // nueva, se estira esta vez sin suavizado (es un instante).
    const QSizeF logical = QSizeF(m_frame.size()) / m_frame.devicePixelRatio();
    const QSizeF scaled = logical.scaled(QSizeF(size()), Qt::KeepAspectRatio);
    const QRectF target(QPointF((width() - scaled.width()) / 2.0,
                                (height() - scaled.height()) / 2.0), scaled);
    p.drawImage(target, m_frame);
    ++m_framesPainted;

    // Modo cine: franjas negras arriba y abajo hasta dejar la imagen en 2,39:1,
    // como en una sala. Si ya es mas panoramica, no se toca.
    QRectF picture = target;
    if (VideoMasks::letterboxes(m_mask)) {
        const double band = target.width() / VideoMasks::kCinemaAspect;
        if (band < target.height()) {
            const double bar = (target.height() - band) / 2.0;
            p.fillRect(QRectF(target.left(), target.top(), target.width(), bar), Qt::black);
            p.fillRect(QRectF(target.left(), target.bottom() - bar, target.width(), bar),
                       Qt::black);
            picture = target.adjusted(0, bar, 0, -bar);
        }
    }

    // Los de archivo aparte mandan; si no hay, los que traiga el video.
    const QString text = m_subtitles.isEmpty() ? m_embeddedText
                                               : m_subtitles.textAt(m_engine->positionMs());
    if (!text.isEmpty())
        m_subtitleStyle.paint(p, picture, text);

    // Avisos: salto (arrastre, rueda o teclado), volumen y previsualizacion.
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (!m_seekLabel.isEmpty() && now < m_seekLabelUntil)
        paintLabel(p, m_seekLabel);
    if (now < m_volumeOsdUntil)
        paintVolumeOsd(p);
    if (m_previewShown)
        paintPreview(p);
}

void VideoPanel::contextMenuEvent(QContextMenuEvent* event)
{
    emit menuRequested(event->globalPos());
}

void VideoPanel::mouseDoubleClickEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        emit fullscreenRequested();
}
