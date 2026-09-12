#include "core/AudioEngine.h"
#include "core/Lang.h"

#include <QDir>

#include <algorithm>
#include <cstring>
#include <vector>

#include "miniaudio.h"

namespace {

inline ma_device*  device(void* p)  { return static_cast<ma_device*>(p); }
inline ma_decoder* decoder(void* p) { return static_cast<ma_decoder*>(p); }

void dataCallback(ma_device* dev, void* output, const void* /*input*/, ma_uint32 frameCount)
{
    if (auto* engine = static_cast<AudioEngine*>(dev->pUserData))
        engine->render(static_cast<float*>(output), frameCount);
}

} // namespace

AudioEngine::AudioEngine(QObject* parent)
    : QObject(parent)
{
    m_equalizer.prepare(kDeviceSampleRate);
    m_effects.prepare(kDeviceSampleRate);
    m_limiter.prepare(kDeviceSampleRate, kDeviceChannels);

    auto* dev = new ma_device();
    ma_device_config config = ma_device_config_init(ma_device_type_playback);
    config.playback.format   = ma_format_f32;
    config.playback.channels = kDeviceChannels;
    config.sampleRate        = kDeviceSampleRate;
    config.dataCallback      = &dataCallback;
    config.pUserData         = this;

    if (ma_device_init(nullptr, &config, dev) != MA_SUCCESS) {
        delete dev;
        m_lastError = Lang::tr("No se pudo abrir el dispositivo de audio.");
        return;
    }

    if (ma_device_start(dev) != MA_SUCCESS) {
        ma_device_uninit(dev);
        delete dev;
        m_lastError = Lang::tr("No se pudo iniciar el dispositivo de audio.");
        return;
    }

    m_device      = dev;
    m_deviceReady = true;
}

AudioEngine::~AudioEngine()
{
    if (m_device) {
        ma_device_uninit(device(m_device));
        delete device(m_device);
        m_device = nullptr;
    }
    std::lock_guard<std::mutex> lock(m_decoderMutex);
    releaseDecoderLocked();
}

void AudioEngine::releaseDecoderLocked()
{
    if (!m_decoder)
        return;
    ma_decoder_uninit(decoder(m_decoder));
    delete decoder(m_decoder);
    m_decoder = nullptr;
}

bool AudioEngine::open(const QString& path)
{
    if (!m_deviceReady) {
        m_lastError = Lang::tr("El dispositivo de audio no esta disponible.");
        return false;
    }

    // El decodificador convierte directamente al formato del dispositivo, de
    // modo que no hay que reiniciarlo entre pistas.
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, kDeviceChannels,
                                                      kDeviceSampleRate);

    auto* dec = new ma_decoder();
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();

    if (ma_decoder_init_file_w(native.c_str(), &config, dec) != MA_SUCCESS) {
        delete dec;
        m_lastError = Lang::tr("No se pudo decodificar el archivo: %1").arg(path);
        return false;
    }

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(dec, &totalFrames);

    {
        std::lock_guard<std::mutex> lock(m_decoderMutex);
        releaseDecoderLocked();
        m_decoder = dec;
        m_equalizer.prepare(kDeviceSampleRate);
        m_effects.prepare(kDeviceSampleRate);
        m_limiter.prepare(kDeviceSampleRate, kDeviceChannels);
    }

    m_currentPath = path;
    m_totalFrames.store(static_cast<qint64>(totalFrames), std::memory_order_relaxed);
    m_positionFrames.store(0, std::memory_order_relaxed);
    m_finished.store(false, std::memory_order_relaxed);
    m_lastError.clear();

    setState(State::Paused);
    emit trackOpened(path, durationMs());
    return true;
}

void AudioEngine::close()
{
    setState(State::Stopped);
    {
        std::lock_guard<std::mutex> lock(m_decoderMutex);
        releaseDecoderLocked();
    }
    m_currentPath.clear();
    m_totalFrames.store(0, std::memory_order_relaxed);
    m_positionFrames.store(0, std::memory_order_relaxed);
    std::fill(std::begin(m_visual), std::end(m_visual), 0.0f);
}

void AudioEngine::play()
{
    if (!m_decoder)
        return;
    m_finished.store(false, std::memory_order_relaxed);
    setState(State::Playing);
}

void AudioEngine::pause()
{
    if (state() == State::Playing)
        setState(State::Paused);
}

void AudioEngine::stop()
{
    if (!m_decoder) {
        setState(State::Stopped);
        return;
    }
    seekMs(0);
    setState(State::Stopped);
    std::fill(std::begin(m_visual), std::end(m_visual), 0.0f);
}

void AudioEngine::togglePlayPause()
{
    if (state() == State::Playing)
        pause();
    else
        play();
}

qint64 AudioEngine::positionMs() const
{
    const qint64 frames = m_positionFrames.load(std::memory_order_relaxed);
    return frames * 1000 / kDeviceSampleRate;
}

qint64 AudioEngine::durationMs() const
{
    const qint64 frames = m_totalFrames.load(std::memory_order_relaxed);
    return frames * 1000 / kDeviceSampleRate;
}

void AudioEngine::seekMs(qint64 ms)
{
    const qint64 total = m_totalFrames.load(std::memory_order_relaxed);
    if (total <= 0)
        return;

    qint64 frame = std::clamp<qint64>(ms * kDeviceSampleRate / 1000, 0, total);

    std::lock_guard<std::mutex> lock(m_decoderMutex);
    if (!m_decoder)
        return;
    if (ma_decoder_seek_to_pcm_frame(decoder(m_decoder),
                                     static_cast<ma_uint64>(frame)) == MA_SUCCESS) {
        m_positionFrames.store(frame, std::memory_order_relaxed);
        m_finished.store(false, std::memory_order_relaxed);
    }
}

void AudioEngine::seekFraction(double fraction)
{
    seekMs(static_cast<qint64>(std::clamp(fraction, 0.0, 1.0) * double(durationMs())));
}

void AudioEngine::setVolume(float linear)
{
    m_volume.store(std::clamp(linear, 0.0f, 1.0f), std::memory_order_relaxed);
}

void AudioEngine::setMuted(bool muted)
{
    m_muted.store(muted, std::memory_order_relaxed);
}

void AudioEngine::setState(State next)
{
    if (m_state.exchange(next, std::memory_order_relaxed) != next)
        emit stateChanged(next);
}

bool AudioEngine::takeFinishedFlag()
{
    return m_finished.exchange(false, std::memory_order_relaxed);
}

void AudioEngine::pushVisualSamples(const float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || frameCount == 0 || channels <= 0)
        return;

    // Lo que suena en el propio reproductor manda: si esta reproduciendo, el
    // visualizador ya se esta alimentando desde render() y meter aqui otra
    // fuente solo mezclaria dos cosas distintas.
    if (state() == State::Playing)
        return;

    unsigned write = m_visualWrite.load(std::memory_order_relaxed);
    for (unsigned f = 0; f < frameCount; ++f) {
        float mono = 0.0f;
        for (int c = 0; c < channels; ++c)
            mono += interleaved[size_t(f) * size_t(channels) + size_t(c)];
        m_visual[write] = mono / float(channels);
        write = (write + 1) % kVisualBufferSize;
    }
    m_visualWrite.store(write, std::memory_order_relaxed);
}

void AudioEngine::copyVisualSamples(float* out, int count) const
{
    if (!out || count <= 0)
        return;

    const unsigned write = m_visualWrite.load(std::memory_order_relaxed);
    const int n = std::min<int>(count, static_cast<int>(kVisualBufferSize));

    for (int i = 0; i < n; ++i) {
        const unsigned index = (write + kVisualBufferSize - static_cast<unsigned>(n - i))
                             % kVisualBufferSize;
        out[i] = m_visual[index];
    }
    for (int i = n; i < count; ++i)
        out[i] = 0.0f;
}

void AudioEngine::render(float* output, unsigned frameCount)
{
    const size_t sampleCount = size_t(frameCount) * kDeviceChannels;
    std::memset(output, 0, sampleCount * sizeof(float));

    if (state() != State::Playing)
        return;

    // try_lock: si la UI esta cambiando de pista preferimos un instante de
    // silencio antes que bloquear el hilo de audio.
    std::unique_lock<std::mutex> lock(m_decoderMutex, std::try_to_lock);
    if (!lock.owns_lock() || !m_decoder)
        return;

    ma_uint64 framesRead = 0;
    const ma_result result =
        ma_decoder_read_pcm_frames(decoder(m_decoder), output, frameCount, &framesRead);

    if (framesRead == 0) {
        m_finished.store(true, std::memory_order_relaxed);
        m_state.store(State::Stopped, std::memory_order_relaxed);
        return;
    }

    m_positionFrames.fetch_add(static_cast<qint64>(framesRead), std::memory_order_relaxed);

    // --- ecualizador de 12 bandas -----------------------------------------
    m_equalizer.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

    // --- rack de efectos ---------------------------------------------------
    m_effects.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

    // --- limitador ---------------------------------------------------------
    // Ultimo paso antes del volumen: garantiza que nada salga por encima de
    // 0 dBFS sin meter distorsion cuando la senal ya venia dentro de rango.
    m_limiter.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

    // --- alimenta el visualizador (mezcla a mono) -------------------------
    // Se toma ANTES del volumen: el analizador debe mostrar el efecto del
    // ecualizador, no la posicion del deslizador de volumen.
    {
        unsigned write = m_visualWrite.load(std::memory_order_relaxed);
        for (ma_uint64 f = 0; f < framesRead; ++f) {
            float mono = 0.0f;
            for (int c = 0; c < kDeviceChannels; ++c)
                mono += output[f * kDeviceChannels + c];
            m_visual[write] = mono / float(kDeviceChannels);
            write = (write + 1) % kVisualBufferSize;
        }
        m_visualWrite.store(write, std::memory_order_relaxed);
    }

    // --- volumen con rampa lineal -----------------------------------------
    const float target = m_muted.load(std::memory_order_relaxed)
                       ? 0.0f
                       : m_volume.load(std::memory_order_relaxed);
    const float step = (target - m_smoothedVolume) / float(std::max<ma_uint64>(framesRead, 1));

    for (ma_uint64 f = 0; f < framesRead; ++f) {
        m_smoothedVolume += step;
        for (int c = 0; c < kDeviceChannels; ++c)
            output[f * kDeviceChannels + c] *= m_smoothedVolume;
    }
    m_smoothedVolume = target;

    // El final de la pista lo dice el decodificador con MA_AT_END. Deducirlo
    // de "entrego menos frames de los pedidos" era erroneo: eso tambien pasa
    // en lecturas parciales normales, y hacia saltar de pista a mitad del
    // tema.
    if (result == MA_AT_END)
        m_finished.store(true, std::memory_order_relaxed);
}
