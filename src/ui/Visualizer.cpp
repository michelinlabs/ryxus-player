#include "ui/Visualizer.h"
#include "core/Lang.h"
#include "core/AudioEngine.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>
#include <QTimer>

#include <algorithm>
#include <cmath>
#include <complex>

namespace {

constexpr int   kFftSize    = 1024;
constexpr int   kBarCount   = 96;
constexpr float kDecay      = 0.28f;   // suavizado temporal de las barras
constexpr qreal kPi         = 3.14159265358979323846;

// FFT iterativa radix-2 in-place. Con 1024 puntos cuesta microsegundos,
// asi que no compensa arrastrar una dependencia externa.
void fft(std::vector<std::complex<float>>& data)
{
    const size_t n = data.size();
    if (n < 2)
        return;

    for (size_t i = 1, j = 0; i < n; ++i) {
        size_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j)
            std::swap(data[i], data[j]);
    }

    for (size_t len = 2; len <= n; len <<= 1) {
        const float angle = -2.0f * float(kPi) / float(len);
        const std::complex<float> step(std::cos(angle), std::sin(angle));
        for (size_t i = 0; i < n; i += len) {
            std::complex<float> w(1.0f, 0.0f);
            for (size_t k = 0; k < len / 2; ++k) {
                const std::complex<float> u = data[i + k];
                const std::complex<float> v = data[i + k + len / 2] * w;
                data[i + k] = u + v;
                data[i + k + len / 2] = u - v;
                w *= step;
            }
        }
    }
}

} // namespace

Visualizer::Visualizer(AudioEngine* engine, QWidget* parent)
    : QWidget(parent)
    , m_engine(engine)
{
    setMinimumHeight(70);
    setCursor(Qt::PointingHandCursor);
    setToolTip(Lang::tr("Clic para alternar entre onda y espectro"));

    m_samples.resize(kFftSize);
    m_bars.fill(0.0f, kBarCount);

    m_timer = new QTimer(this);
    m_timer->setInterval(33);   // ~30 fps
    connect(m_timer, &QTimer::timeout, this, &Visualizer::refresh);
}

void Visualizer::setMode(Mode mode)
{
    if (m_mode == mode)
        return;
    m_mode = mode;
    m_bars.fill(0.0f);
    update();
}

void Visualizer::setActive(bool active)
{
    if (m_active == active)
        return;
    m_active = active;
    if (active)
        m_timer->start();
    else
        m_timer->stop();
    update();
}

void Visualizer::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton)
        setMode(m_mode == Mode::Waveform ? Mode::Spectrum : Mode::Waveform);
    QWidget::mousePressEvent(event);
}

void Visualizer::refresh()
{
    if (!m_engine)
        return;

    m_engine->copyVisualSamples(m_samples.data(), m_samples.size());

    if (m_mode == Mode::Waveform)
        computeWaveform();
    else
        computeSpectrum();

    update();
}

void Visualizer::computeWaveform()
{
    const int samplesPerBar = m_samples.size() / kBarCount;

    for (int bar = 0; bar < kBarCount; ++bar) {
        float peak = 0.0f;
        const int begin = bar * samplesPerBar;
        for (int i = 0; i < samplesPerBar; ++i)
            peak = std::max(peak, std::fabs(m_samples.at(begin + i)));

        // Realce no lineal: los pasajes suaves siguen viendose.
        peak = std::pow(std::min(peak * 1.6f, 1.0f), 0.7f);

        // Ataque instantaneo, caida suave: es lo que da la sensacion de "vivo".
        m_bars[bar] = (peak > m_bars.at(bar))
                    ? peak
                    : m_bars.at(bar) * (1.0f - kDecay);
    }
}

void Visualizer::computeSpectrum()
{
    std::vector<std::complex<float>> spectrum(kFftSize);
    for (int i = 0; i < kFftSize; ++i) {
        // Ventana de Hann para no ensuciar el espectro con los bordes.
        const float window = 0.5f * (1.0f - std::cos(2.0f * float(kPi) * i / (kFftSize - 1)));
        spectrum[i] = std::complex<float>(m_samples.at(i) * window, 0.0f);
    }
    fft(spectrum);

    const int usableBins = kFftSize / 2;

    for (int bar = 0; bar < kBarCount; ++bar) {
        // Reparto logaritmico: asi el grave no ocupa una sola barra.
        const float t0 = float(bar) / kBarCount;
        const float t1 = float(bar + 1) / kBarCount;
        const int lo = std::clamp(int(std::pow(float(usableBins), t0)), 1, usableBins - 1);
        const int hi = std::clamp(int(std::pow(float(usableBins), t1)), lo + 1, usableBins);

        float magnitude = 0.0f;
        for (int bin = lo; bin < hi; ++bin)
            magnitude = std::max(magnitude, std::abs(spectrum[bin]));

        const float db = 20.0f * std::log10(std::max(magnitude / (kFftSize * 0.25f), 1e-6f));
        const float level = std::clamp((db + 62.0f) / 62.0f, 0.0f, 1.0f);

        m_bars[bar] = (level > m_bars.at(bar))
                    ? level
                    : m_bars.at(bar) * (1.0f - kDecay);
    }
}

void Visualizer::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    // Sin relleno: el panel de reproduccion ya pinto su capa translucida.

    const qreal centerY = height() / 2.0;
    const qreal barSpan = width() / qreal(kBarCount);
    const qreal barWidth = std::max<qreal>(1.0, barSpan - 1.0);
    const qreal maxHalf = centerY - 2.0;

    p.setPen(Qt::NoPen);

    for (int bar = 0; bar < kBarCount; ++bar) {
        const qreal level = m_bars.at(bar);
        const qreal half  = std::max<qreal>(0.5, level * maxHalf);
        const qreal x     = bar * barSpan;

        // El degradado replica la referencia: claro en el centro, violeta
        // hacia los extremos de la barra.
        QColor color = Theme::Wave;
        if (level < 0.25)
            color = Theme::TextFaint;
        else if (level < 0.6)
            color = Theme::AccentLight.lighter(120);

        p.setBrush(color);
        p.drawRect(QRectF(x, centerY - half, barWidth, half * 2.0));
    }

    // Linea de eje, visible cuando no hay senal.
    p.setPen(QPen(Theme::Border, 1));
    p.drawLine(QPointF(0, centerY), QPointF(width(), centerY));
}
