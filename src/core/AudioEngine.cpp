#include "core/AudioEngine.h"
#include "core/Lang.h"
#include "core/MediaStream.h"
#include "core/TrackInfo.h"

#include <QAudioBuffer>
#include <QTimer>
#include <QDir>

#include <algorithm>
#include <cstring>
#include <vector>

#include "miniaudio.h"

namespace {

inline ma_device*  device(void* p)  { return static_cast<ma_device*>(p); }
inline ma_decoder* decoder(void* p) { return static_cast<ma_decoder*>(p); }
inline ma_pcm_rb*  ring(void* p)    { return static_cast<ma_pcm_rb*>(p); }

// Colchon entre lo que entrega FFmpeg y lo que pide el dispositivo. Medio
// segundo sobra: FFmpeg entrega al ritmo de la reproduccion, asi que en la
// practica el buffer apenas pasa de unas decenas de milisegundos.
constexpr ma_uint32 kStreamRingFrames = AudioEngine::kDeviceSampleRate / 2;

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

    // --- FFmpeg ------------------------------------------------------------
    auto* rb = new ma_pcm_rb();
    if (ma_pcm_rb_init(ma_format_f32, kDeviceChannels, kStreamRingFrames,
                       nullptr, nullptr, rb) == MA_SUCCESS) {
        m_streamRing = rb;
    } else {
        delete rb;
    }

    m_stream = new MediaStream(this);
    // Con una pista externa, el sonido del video se ignora y entra el de ella.
    // Directas: llegan desde el hilo de FFmpeg (ver MediaStream).
    connect(m_stream, &MediaStream::audioReady, this, [this](const QAudioBuffer& buffer) {
        if (!m_externalActive.load(std::memory_order_relaxed))
            onStreamAudio(buffer);
    }, Qt::DirectConnection);
    connect(m_stream, &MediaStream::tracksChanged, this, &AudioEngine::tracksChanged);

    m_external = new MediaStream(this);
    connect(m_external, &MediaStream::audioReady, this, [this](const QAudioBuffer& buffer) {
        if (m_externalActive.load(std::memory_order_relaxed))
            onStreamAudio(buffer);
    }, Qt::DirectConnection);
    connect(m_external, &MediaStream::failed, this, [this](const QString& message) {
        if (m_externalAudioPath.isEmpty())
            return;
        clearExternalAudio();
        emit mediaError(message);
    });

    // Dos reproducciones con relojes propios acaban separandose un poco: cada
    // segundo y medio se comprueba y, si pasa de 120 ms, se recoloca la pista.
    m_externalSync = new QTimer(this);
    m_externalSync->setInterval(1500);
    connect(m_externalSync, &QTimer::timeout, this, [this]() {
        if (m_externalAudioPath.isEmpty() || state() != State::Playing)
            return;
        const qint64 drift = m_external->positionMs() - m_stream->positionMs();
        if (std::abs(drift) > 120) {
            m_external->seekMs(m_stream->positionMs());
            m_streamFlush.store(true, std::memory_order_relaxed);
        }
    });
    connect(m_stream, &MediaStream::durationChanged, this, [this](qint64 ms) {
        if (isStreaming())
            emit durationChanged(ms);
    });
    connect(m_stream, &MediaStream::hasVideoChanged, this, [this](bool video) {
        const bool active = isStreaming() && video;
        if (active != m_videoActive) {
            m_videoActive = active;
            emit videoActiveChanged(active);
        }
    });
    connect(m_stream, &MediaStream::finished, this, [this]() {
        if (isStreaming())
            m_finished.store(true, std::memory_order_relaxed);
    });
    connect(m_stream, &MediaStream::failed, this, [this](const QString& message) {
        if (!isStreaming())
            return;
        m_lastError = message;
        setState(State::Stopped);
        emit mediaError(message);
    });
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
    if (m_streamRing) {
        ma_pcm_rb_uninit(ring(m_streamRing));
        delete ring(m_streamRing);
        m_streamRing = nullptr;
    }
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

    // Video, o audio que miniaudio no trae de serie: FFmpeg.
    if (needsStream(path))
        return openStream(path);

    // El decodificador convierte directamente al formato del dispositivo, de
    // modo que no hay que reiniciarlo entre pistas.
    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, kDeviceChannels,
                                                      kDeviceSampleRate);

    auto* dec = new ma_decoder();
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();

    if (ma_decoder_init_file_w(native.c_str(), &config, dec) != MA_SUCCESS) {
        // Ultimo recurso antes de rendirse: FFmpeg abre casi de todo.
        delete dec;
        return openStream(path);
    }

    closeStream();

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
    closeStream();
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
    if (!m_decoder && !isStreaming())
        return;
    m_finished.store(false, std::memory_order_relaxed);
    if (isStreaming()) {
        m_stream->play();
        if (!m_externalAudioPath.isEmpty())
            m_external->play();
    }
    setState(State::Playing);
}

void AudioEngine::pause()
{
    if (state() != State::Playing)
        return;
    if (isStreaming()) {
        m_stream->pause();
        if (!m_externalAudioPath.isEmpty())
            m_external->pause();
    }
    setState(State::Paused);
}

void AudioEngine::stop()
{
    if (isStreaming()) {
        m_stream->stop();
        if (!m_externalAudioPath.isEmpty())
            m_external->stop();
        m_streamFlush.store(true, std::memory_order_relaxed);
        setState(State::Stopped);
        std::fill(std::begin(m_visual), std::end(m_visual), 0.0f);
        return;
    }
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
    if (isStreaming())
        return m_stream->positionMs();
    const qint64 frames = m_positionFrames.load(std::memory_order_relaxed);
    return frames * 1000 / kDeviceSampleRate;
}

qint64 AudioEngine::durationMs() const
{
    if (isStreaming())
        return m_stream->durationMs();
    const qint64 frames = m_totalFrames.load(std::memory_order_relaxed);
    return frames * 1000 / kDeviceSampleRate;
}

void AudioEngine::seekMs(qint64 ms)
{
    if (isStreaming()) {
        m_stream->seekMs(std::max<qint64>(0, ms));
        if (!m_externalAudioPath.isEmpty())
            m_external->seekMs(std::max<qint64>(0, ms));
        // Lo que quedaba en el buffer es de antes del salto.
        m_streamFlush.store(true, std::memory_order_relaxed);
        m_finished.store(false, std::memory_order_relaxed);
        return;
    }
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

// ------------------------------------------------------------------ FFmpeg

bool AudioEngine::needsStream(const QString& path)
{
    return TrackInfo::isVideo(path);
}

bool AudioEngine::hasVideo() const
{
    return m_videoActive;
}

double AudioEngine::videoFrameMs() const
{
    const double fps = isStreaming() ? m_stream->frameRate() : 0.0;
    return fps > 1.0 ? 1000.0 / fps : 1000.0 / 30.0;
}

QVideoSink* AudioEngine::videoSink() const
{
    return m_stream ? m_stream->videoSink() : nullptr;
}

bool AudioEngine::openStream(const QString& path)
{
    if (!m_stream || !m_streamRing) {
        m_lastError = Lang::tr("No se pudo decodificar el archivo: %1").arg(path);
        return false;
    }

    // Se suelta lo de miniaudio: a partir de aqui el sonido llega del buffer.
    {
        std::lock_guard<std::mutex> lock(m_decoderMutex);
        releaseDecoderLocked();
        m_equalizer.prepare(kDeviceSampleRate);
        m_effects.prepare(kDeviceSampleRate);
        m_limiter.prepare(kDeviceSampleRate, kDeviceChannels);
    }

    clearExternalAudio();
    m_streamFlush.store(true, std::memory_order_relaxed);
    m_streamMode.store(true, std::memory_order_relaxed);
    m_stream->open(path);

    m_currentPath = path;
    m_totalFrames.store(0, std::memory_order_relaxed);
    m_positionFrames.store(0, std::memory_order_relaxed);
    m_finished.store(false, std::memory_order_relaxed);
    m_lastError.clear();

    setState(State::Paused);
    // La duracion real llega despues, con durationChanged().
    emit trackOpened(path, m_stream->durationMs());
    return true;
}

void AudioEngine::closeStream()
{
    if (!isStreaming())
        return;
    m_streamMode.store(false, std::memory_order_relaxed);
    clearExternalAudio();
    m_stream->close();
    m_streamFlush.store(true, std::memory_order_relaxed);
    if (m_videoActive) {
        m_videoActive = false;
        emit videoActiveChanged(false);
    }
}

QStringList AudioEngine::audioTrackNames() const
{
    return isStreaming() ? m_stream->audioTrackNames() : QStringList();
}

QStringList AudioEngine::subtitleTrackNames() const
{
    return isStreaming() ? m_stream->subtitleTrackNames() : QStringList();
}

int AudioEngine::activeAudioTrack() const
{
    return isStreaming() ? m_stream->activeAudioTrack() : -1;
}

int AudioEngine::activeSubtitleTrack() const
{
    return isStreaming() ? m_stream->activeSubtitleTrack() : -1;
}

void AudioEngine::setAudioTrack(int index)
{
    if (!isStreaming())
        return;
    clearExternalAudio();
    m_stream->setActiveAudioTrack(index);
    m_streamFlush.store(true, std::memory_order_relaxed);
}

void AudioEngine::setSubtitleTrack(int index)
{
    if (isStreaming())
        m_stream->setActiveSubtitleTrack(index);
}

bool AudioEngine::setExternalAudio(const QString& path)
{
    if (!isStreaming() || path.isEmpty())
        return false;

    m_externalAudioPath = path;
    m_externalActive.store(true, std::memory_order_relaxed);
    m_streamFlush.store(true, std::memory_order_relaxed);
    m_external->openAt(path, m_stream->positionMs(), state() == State::Playing);
    m_externalSync->start();
    emit tracksChanged();
    return true;
}

void AudioEngine::clearExternalAudio()
{
    if (m_externalAudioPath.isEmpty())
        return;
    m_externalAudioPath.clear();
    m_externalActive.store(false, std::memory_order_relaxed);
    m_externalSync->stop();
    m_external->close();
    m_streamFlush.store(true, std::memory_order_relaxed);
    emit tracksChanged();
}

void AudioEngine::onStreamAudio(const QAudioBuffer& buffer)
{
    if (!isStreaming() || !m_streamRing || !buffer.isValid())
        return;

    // Se pidio estereo en coma flotante a la frecuencia del dispositivo; si
    // por lo que sea llega otra cosa, mejor silencio que ruido.
    const QAudioFormat format = buffer.format();
    if (format.sampleFormat() != QAudioFormat::Float
        || format.channelCount() != kDeviceChannels)
        return;

    const auto* src = buffer.constData<float>();
    ma_uint32 remaining = ma_uint32(buffer.frameCount());

    // El buffer admite un solo escritor: al cambiar de pista puede quedar en
    // vuelo un trozo de la anterior mientras llega el primero de la nueva.
    std::lock_guard<std::mutex> lock(m_ringWriteMutex);

    while (remaining > 0) {
        void* region = nullptr;
        ma_uint32 chunk = remaining;
        if (ma_pcm_rb_acquire_write(ring(m_streamRing), &chunk, &region) != MA_SUCCESS
            || chunk == 0)
            break;   // el dispositivo va atrasado: se descarta lo que no cabe

        std::memcpy(region, src, size_t(chunk) * kDeviceChannels * sizeof(float));
        ma_pcm_rb_commit_write(ring(m_streamRing), chunk);
        src += size_t(chunk) * kDeviceChannels;
        remaining -= chunk;
    }
}

unsigned AudioEngine::readStream(float* output, unsigned frameCount)
{
    if (!m_streamRing)
        return 0;

    // Tras un salto, una pausa larga o una pista nueva: se tira lo que habia
    // para no oir medio segundo de lo anterior.
    if (m_streamFlush.exchange(false, std::memory_order_relaxed)) {
        for (;;) {
            void* region = nullptr;
            ma_uint32 chunk = kStreamRingFrames;
            if (ma_pcm_rb_acquire_read(ring(m_streamRing), &chunk, &region) != MA_SUCCESS
                || chunk == 0)
                break;
            ma_pcm_rb_commit_read(ring(m_streamRing), chunk);
        }
    }

    unsigned filled = 0;
    while (filled < frameCount) {
        void* region = nullptr;
        ma_uint32 chunk = frameCount - filled;
        if (ma_pcm_rb_acquire_read(ring(m_streamRing), &chunk, &region) != MA_SUCCESS
            || chunk == 0)
            break;
        std::memcpy(output + size_t(filled) * kDeviceChannels, region,
                    size_t(chunk) * kDeviceChannels * sizeof(float));
        ma_pcm_rb_commit_read(ring(m_streamRing), chunk);
        filled += chunk;
    }
    return filled;
}

void AudioEngine::setChainBypassed(bool bypassed)
{
    m_chainBypassed.store(bypassed, std::memory_order_relaxed);
}

void AudioEngine::pushVisualSamples(const float* interleaved, unsigned frameCount, int channels)
{
    if (!interleaved || frameCount == 0 || channels <= 0)
        return;

    // Lo que suena en el propio reproductor manda: si esta reproduciendo, el
    // visualizador ya se esta alimentando desde render() y meter aqui otra
    // fuente solo mezclaria dos cosas distintas. Con la cadena saltada es al
    // reves: render() no alimenta nada y la captura ya trae esta musica.
    if (state() == State::Playing && !isChainBypassed())
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
    if (!lock.owns_lock())
        return;

    ma_uint64 framesRead = 0;
    ma_result result = MA_SUCCESS;

    if (isStreaming()) {
        // Lo que entrega FFmpeg. Si aun no ha llegado nada -- al arrancar,
        // tras un salto o en un video sin sonido -- sale silencio sin dar la
        // pista por acabada: el final lo avisa MediaStream.
        framesRead = readStream(output, frameCount);
        if (framesRead < frameCount && hasVideo())
            m_streamUnderruns.fetch_add(1, std::memory_order_relaxed);
        if (framesRead == 0)
            return;
    } else {
        if (!m_decoder)
            return;
        result = ma_decoder_read_pcm_frames(decoder(m_decoder), output, frameCount, &framesRead);

        if (framesRead == 0) {
            m_finished.store(true, std::memory_order_relaxed);
            m_state.store(State::Stopped, std::memory_order_relaxed);
            return;
        }

        m_positionFrames.fetch_add(static_cast<qint64>(framesRead), std::memory_order_relaxed);
    }

    // Con la captura del sistema en marcha la cadena y el visualizador se
    // alimentan alli; aqui solo queda el volumen. Ver setChainBypassed().
    if (!isChainBypassed()) {
        // --- ecualizador de 12 bandas -------------------------------------
        m_equalizer.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

        // --- rack de efectos -----------------------------------------------
        m_effects.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

        // --- limitador -----------------------------------------------------
        // Ultimo paso antes del volumen: garantiza que nada salga por encima
        // de 0 dBFS sin meter distorsion cuando la senal ya venia dentro de
        // rango.
        m_limiter.process(output, static_cast<unsigned>(framesRead), kDeviceChannels);

        // --- alimenta el visualizador (mezcla a mono) ---------------------
        // Se toma ANTES del volumen: el analizador debe mostrar el efecto del
        // ecualizador, no la posicion del deslizador de volumen.
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
