#include "ui/BackgroundHost.h"

#include "core/AudioEngine.h"
#include "core/Lang.h"
#include "ui/Theme.h"

#include <QImageReader>
#include <QLinearGradient>
#include <QMovie>
#include <QPainter>
#include <QPainterPath>
#include <QResizeEvent>
#include <QTimer>

#include <algorithm>
#include <cmath>

namespace {

// Barras del vumetro. Suficientes para que se lea como un espectro y no como
// un ecualizador de juguete, sin llegar a parecer una linea.
constexpr int kBarCount = 96;

// Caida por fotograma del espectro. El ataque es instantaneo.
constexpr float kBarDecay = 0.09f;

// Puntos y salto del osciloscopio. 1024 muestras seguidas serian medio ciclo de
// un grave; con salto se abarca una ventana con forma reconocible.
constexpr int kWavePoints = 512;
constexpr int kWaveStride = 4;

} // namespace

BackgroundHost::BackgroundHost(QWidget* parent)
    : QWidget(parent)
    , m_spectrum(kBarCount)
    , m_bars(kBarCount, 0.0f)
{
    setObjectName(QStringLiteral("centralRoot"));
    // El widget pinta su fondo por completo: Qt puede saltarse el borrado.
    setAttribute(Qt::WA_OpaquePaintEvent, true);

    m_spectrum.setSampleRate(AudioEngine::kDeviceSampleRate);
    m_spectrum.setDecay(0.18f);
    m_spectrum.setFloorDb(-72.0f);
    m_samples.resize(SpectrumAnalyzer::kFftSize);

    m_wave.assign(kWavePoints, 0.0f);
    m_clock.start();

    m_visualTimer = new QTimer(this);
    m_visualTimer->setInterval(33);   // ~30 fps
    connect(m_visualTimer, &QTimer::timeout, this, [this]() {
        if (!m_engine)
            return;
        m_engine->copyVisualSamples(m_samples.data(), int(m_samples.size()));
        m_spectrum.update(m_samples.data(), int(m_samples.size()));

        // La cola del buffer es lo mas reciente: es lo que dibuja el
        // osciloscopio. Se submuestrea al numero de puntos que se pintan.
        const int total = int(m_samples.size());
        for (int i = 0; i < kWavePoints; ++i) {
            const int index = total - kWavePoints * kWaveStride
                            + i * kWaveStride;
            m_wave[size_t(i)] = (index >= 0 && index < total) ? m_samples[size_t(index)] : 0.0f;
        }

        update();
    });
}

BackgroundHost::~BackgroundHost() = default;

QString BackgroundHost::modeName(Mode mode)
{
    switch (mode) {
    case Mode::Cover:   return Lang::tr("Cubrir");
    case Mode::Fit:     return Lang::tr("Ajustar");
    case Mode::Stretch: return Lang::tr("Estirar");
    case Mode::Tile:    return Lang::tr("Mosaico");
    }
    return QString();
}

QString BackgroundHost::imageFilter()
{
    return Lang::tr("Imagenes y animaciones (*.jpg *.jpeg *.png *.bmp *.webp *.gif)");
}

// ------------------------------------------------------------------- fuente

void BackgroundHost::clearSource()
{
    if (m_movie) {
        m_movie->stop();
        m_movie->deleteLater();
        m_movie = nullptr;
    }
    m_source = QImage();
    m_scaled = QPixmap();
}

bool BackgroundHost::setImagePath(const QString& path)
{
    if (path.isEmpty()) {
        clearSource();
        m_path.clear();
        update();
        return true;
    }

    QImageReader reader(path);
    reader.setAutoTransform(true);   // respeta la orientacion EXIF

    // Un GIF (o un WEBP animado) se reproduce en vez de congelarse en su primer
    // fotograma. QMovie va decodificando sobre la marcha, asi que una animacion
    // larga no se carga entera en memoria.
    if (reader.supportsAnimation() && reader.imageCount() > 1) {
        auto* movie = new QMovie(path, QByteArray(), this);
        if (movie->isValid()) {
            clearSource();
            m_path  = path;
            m_movie = movie;
            m_movie->setCacheMode(QMovie::CacheNone);
            connect(m_movie, &QMovie::frameChanged, this, [this]() { update(); });
            rescaleMovie();
            m_movie->start();
            update();
            return true;
        }
        movie->deleteLater();
    }

    const QImage image = reader.read();
    if (image.isNull())
        return false;

    clearSource();
    m_path   = path;
    m_source = image;
    rebuildScaled();
    update();
    return true;
}

void BackgroundHost::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    rebuildScaled();
    rescaleMovie();
    update();
}

void BackgroundHost::setDarkening(int percent)
{
    percent = qBound(0, percent, 90);
    if (m_darkening == percent)
        return;
    m_darkening = percent;
    update();
}

// ------------------------------------------------------------------ vumetro

void BackgroundHost::setEngine(AudioEngine* engine)
{
    m_engine = engine;
    refreshVisualizerTimer();
}

void BackgroundHost::setImageOpacity(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_imageOpacity == percent)
        return;
    m_imageOpacity = percent;
    update();
}

void BackgroundHost::setVisualization(int index)
{
    if (index >= Visualizations::count())
        index = Visualizations::count() - 1;
    if (index < 0)
        index = -1;
    if (m_visualization == index)
        return;

    m_visualization = index;

    if (index < 0) {
        m_spectrum.clear();
        std::fill(m_bars.begin(), m_bars.end(), 0.0f);
        std::fill(m_wave.begin(), m_wave.end(), 0.0f);
    }

    refreshVisualizerTimer();
    update();
}

void BackgroundHost::setVisualOpacity(int percent)
{
    percent = qBound(0, percent, 100);
    if (m_visualOpacity == percent)
        return;
    m_visualOpacity = percent;
    update();
}

void BackgroundHost::refreshVisualizerTimer()
{
    // Solo consume FFT cuando de verdad hay algo que dibujar.
    if (m_visualization >= 0 && m_engine)
        m_visualTimer->start();
    else
        m_visualTimer->stop();
}

// ------------------------------------------------------------------ escalado

void BackgroundHost::resizeEvent(QResizeEvent* event)
{
    rebuildScaled();
    rescaleMovie();
    QWidget::resizeEvent(event);
}

void BackgroundHost::rescaleMovie()
{
    if (!m_movie || width() <= 0 || height() <= 0)
        return;

    const QSize frame = m_movie->currentImage().size().isEmpty()
                      ? m_movie->scaledSize()
                      : m_movie->currentImage().size();

    if (m_mode == Mode::Tile || frame.isEmpty()) {
        m_movie->setScaledSize(QSize());   // tamano original
        return;
    }

    QSize target = frame;
    switch (m_mode) {
    case Mode::Cover:   target = frame.scaled(size(), Qt::KeepAspectRatioByExpanding); break;
    case Mode::Fit:     target = frame.scaled(size(), Qt::KeepAspectRatio);            break;
    case Mode::Stretch: target = size();                                               break;
    case Mode::Tile:    break;
    }
    m_movie->setScaledSize(target);
}

void BackgroundHost::rebuildScaled()
{
    if (m_source.isNull() || width() <= 0 || height() <= 0) {
        m_scaled = QPixmap();
        return;
    }

    // El escalado se hace una vez por cambio de tamano, no en cada repintado:
    // con una foto grande y 30 fps de vumetro seria carisimo.
    const qreal dpr = devicePixelRatioF();
    const QSize target = (size() * dpr);

    QImage rendered;
    switch (m_mode) {
    case Mode::Cover:
        rendered = m_source.scaled(target, Qt::KeepAspectRatioByExpanding,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Fit:
        rendered = m_source.scaled(target, Qt::KeepAspectRatio,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Stretch:
        rendered = m_source.scaled(target, Qt::IgnoreAspectRatio,
                                   Qt::SmoothTransformation);
        break;
    case Mode::Tile:
        rendered = m_source;   // se repite en el paintEvent
        break;
    }

    m_scaled = QPixmap::fromImage(rendered);
    m_scaled.setDevicePixelRatio(dpr);
}

// ------------------------------------------------------------------- dibujo

void BackgroundHost::drawLiveLayer(QPainter& p)
{
    const std::vector<float>& levels = m_spectrum.levels();
    if (levels.empty() || m_bars.size() != levels.size())
        return;

    // Ataque instantaneo y caida suave: sin esto el espectro parpadea en vez
    // de moverse.
    bool anyLevel = false;
    for (size_t i = 0; i < m_bars.size(); ++i) {
        m_bars[i] = std::max(levels[i], m_bars[i] - kBarDecay);
        if (m_bars[i] > 0.002f)
            anyLevel = true;
    }
    if (!anyLevel)
        return;

    // Energia por franjas: las visualizaciones que no dibujan el espectro
    // entero -- anillos, nebulosa -- se mueven con estas tres.
    const auto average = [this](double from, double to) {
        const size_t first = size_t(from * (m_bars.size() - 1));
        const size_t last  = size_t(to   * (m_bars.size() - 1));
        float sum = 0.0f;
        for (size_t i = first; i <= last; ++i)
            sum += m_bars[i];
        return sum / float(std::max<size_t>(1, last - first + 1));
    };

    m_frame.spectrum = &m_bars;
    m_frame.wave     = &m_wave;
    m_frame.bass     = average(0.00, 0.18);
    m_frame.mid      = average(0.18, 0.55);
    m_frame.treble   = average(0.55, 1.00);
    m_frame.level    = average(0.00, 1.00);
    m_frame.seconds  = m_clock.elapsed() / 1000.0;

    Visualizations::paint(p, QRectF(rect()), m_visualization, m_frame);
}

void BackgroundHost::paintEvent(QPaintEvent*)
{
    QPainter p(this);

    // 1) color base del skin: es lo que se ve cuando no hay nada mas, y lo que
    //    rellena los bordes en el modo "Ajustar".
    p.fillRect(rect(), Theme::Chrome);

    // 2) capa de imagen (fija o animada), con su propia opacidad.
    if (hasImage() && m_imageOpacity > 0) {
        p.setRenderHint(QPainter::SmoothPixmapTransform, true);
        p.setOpacity(m_imageOpacity / 100.0);

        if (m_movie) {
            const QPixmap frame = m_movie->currentPixmap();
            if (!frame.isNull()) {
                if (m_mode == Mode::Tile) {
                    p.drawTiledPixmap(rect(), frame);
                } else {
                    const QPoint origin(rect().center().x() - frame.width() / 2,
                                        rect().center().y() - frame.height() / 2);
                    p.drawPixmap(origin, frame);
                }
            }
        } else if (!m_scaled.isNull()) {
            if (m_mode == Mode::Tile) {
                p.drawTiledPixmap(rect(), m_scaled);
            } else {
                const QSize logical = m_scaled.size() / m_scaled.devicePixelRatio();
                const QPoint origin(rect().center().x() - logical.width() / 2,
                                    rect().center().y() - logical.height() / 2);
                p.drawPixmap(QRect(origin, logical), m_scaled);
            }
        }

        p.setOpacity(1.0);
    }

    // 3) velo oscuro sobre la imagen: mantiene el contraste del texto sobre
    //    fondos claros. No se aplica a la capa viva, que ya es oscura de suyo.
    if (m_darkening > 0 && hasImage())
        p.fillRect(rect(), QColor(0, 0, 0, m_darkening * 255 / 100));

    // 4) capa viva encima, con su opacidad. Las dos capas conviven: se puede
    //    tener foto al 40 % y mandala al 80 % a la vez.
    if (m_visualization >= 0 && m_visualOpacity > 0) {
        p.setOpacity(m_visualOpacity / 100.0);
        drawLiveLayer(p);
        p.setOpacity(1.0);
    }
}
