#include "core/SystemAudioTap.h"

#include "core/AudioEngine.h"
#include "core/Lang.h"
#include "core/Settings.h"

#include <QTimer>

#include "miniaudio.h"

#include <cstring>
#include <vector>

namespace {

constexpr ma_format kFormat   = ma_format_f32;
constexpr int       kChannels = AudioEngine::kDeviceChannels;
constexpr int       kRate     = AudioEngine::kDeviceSampleRate;

// Medio segundo de colchon entre la captura y la salida. Sobra para absorber
// el desfase entre los dos dispositivos sin que se note el retardo.
constexpr ma_uint32 kRingFrames = kRate / 2;

ma_device*  dev(void* p) { return static_cast<ma_device*>(p); }
ma_pcm_rb*  ring(void* p) { return static_cast<ma_pcm_rb*>(p); }

// Nombres que delatan una salida que no es un altavoz de verdad. Sirve para
// no proponer "sacar el sonido" por otro dispositivo virtual, que no se oiria.
bool looksVirtual(const QString& name)
{
    static const char* const kHints[] = {
        "CABLE", "VB-Audio", "Voicemeeter", "Virtual", "mirroring",
        "Mixed Reality", "Oculus", "Steam Streaming", "NVIDIA Virtual",
    };
    for (const char* hint : kHints) {
        if (name.contains(QLatin1String(hint), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

// Solo los cables pensados para encaminar audio entre programas.
bool looksLikeCable(const QString& name)
{
    static const char* const kHints[] = {"CABLE", "VB-Audio", "Voicemeeter"};
    for (const char* hint : kHints) {
        if (name.contains(QLatin1String(hint), Qt::CaseInsensitive))
            return true;
    }
    return false;
}

QByteArray toBytes(const ma_device_id& id)
{
    return QByteArray(reinterpret_cast<const char*>(&id), sizeof(ma_device_id));
}

bool fromBytes(const QByteArray& bytes, ma_device_id* out)
{
    if (bytes.size() != int(sizeof(ma_device_id)))
        return false;
    std::memcpy(out, bytes.constData(), sizeof(ma_device_id));
    return true;
}

} // namespace

void systemTapCaptureCallback(void* device, void* /*output*/, const void* input, unsigned frames)
{
    auto* tap = static_cast<SystemAudioTap*>(dev(device)->pUserData);
    if (tap)
        tap->onCaptured(input, frames);
}

void systemTapPlaybackCallback(void* device, void* output, const void* /*input*/, unsigned frames)
{
    auto* tap = static_cast<SystemAudioTap*>(dev(device)->pUserData);
    if (tap)
        tap->onPlayback(output, frames);
}

namespace {

void captureThunk(ma_device* device, void* output, const void* input, ma_uint32 frames)
{
    systemTapCaptureCallback(device, output, input, frames);
}

void playbackThunk(ma_device* device, void* output, const void* input, ma_uint32 frames)
{
    systemTapPlaybackCallback(device, output, input, frames);
}

} // namespace

SystemAudioTap::SystemAudioTap(QObject* parent)
    : QObject(parent)
{
    m_sync = new QTimer(this);
    m_sync->setInterval(250);
    connect(m_sync, &QTimer::timeout, this, &SystemAudioTap::applySettings);
}

SystemAudioTap::~SystemAudioTap()
{
    stop();
}

QList<SystemAudioTap::Device> SystemAudioTap::outputDevices()
{
    QList<Device> devices;

    ma_context context;
    if (ma_context_init(nullptr, 0, nullptr, &context) != MA_SUCCESS)
        return devices;

    ma_device_info* playback = nullptr;
    ma_uint32 playbackCount = 0;
    if (ma_context_get_devices(&context, &playback, &playbackCount, nullptr, nullptr) == MA_SUCCESS) {
        for (ma_uint32 i = 0; i < playbackCount; ++i) {
            Device device;
            device.id        = toBytes(playback[i].id);
            device.name      = QString::fromUtf8(playback[i].name);
            device.isDefault = playback[i].isDefault != 0;
            device.isVirtual = looksVirtual(device.name);
            device.isCable   = looksLikeCable(device.name);
            devices.append(device);
        }
    }

    ma_context_uninit(&context);
    return devices;
}

void SystemAudioTap::suggestRouting(QByteArray* source, QByteArray* output)
{
    if (source) source->clear();
    if (output) output->clear();

    const QList<Device> devices = outputDevices();

    // Origen: el cable virtual. Es donde Windows tiene que mandar el sonido.
    QByteArray cable;
    for (const Device& device : devices) {
        if (device.isCable) {
            cable = device.id;
            break;
        }
    }
    if (cable.isEmpty())
        return;   // sin cable no hay reparto que valga

    // Destino: unos altavoces de verdad. Se prefiere el predeterminado actual
    // si todavia no es el cable; si no, el primero que no sea virtual.
    QByteArray speakers;
    for (const Device& device : devices) {
        if (device.isDefault && !device.isVirtual) {
            speakers = device.id;
            break;
        }
    }
    if (speakers.isEmpty()) {
        for (const Device& device : devices) {
            if (!device.isVirtual) {
                speakers = device.id;
                break;
            }
        }
    }
    if (speakers.isEmpty())
        return;

    if (source) *source = cable;
    if (output) *output = speakers;
}

bool SystemAudioTap::start(const QByteArray& sourceId, const QByteArray& outputId, QString* error)
{
    const auto fail = [&](const QString& message) {
        m_error = message;
        if (error)
            *error = message;
        stop();
        return false;
    };

    stop();

    // Capturar y devolver por la misma salida se realimenta: lo que sale vuelve
    // a entrar y sube sin parar. Se rechaza antes de abrir nada.
    if (!sourceId.isEmpty() && sourceId == outputId) {
        return fail(Lang::tr("La salida procesada tiene que ser distinta de la que se captura: "
                             "si no, el sonido se realimenta."));
    }

    m_ring = new ma_pcm_rb;
    if (ma_pcm_rb_init(kFormat, kChannels, kRingFrames, nullptr, nullptr, ring(m_ring)) != MA_SUCCESS)
        return fail(Lang::tr("No se pudo preparar el buffer de audio."));

    // --- captura de la mezcla del sistema ---------------------------------
    ma_device_id captureId{};
    const bool haveCaptureId = fromBytes(sourceId, &captureId);

    ma_device_config captureConfig = ma_device_config_init(ma_device_type_loopback);
    captureConfig.capture.pDeviceID = haveCaptureId ? &captureId : nullptr;
    captureConfig.capture.format    = kFormat;
    captureConfig.capture.channels  = kChannels;
    captureConfig.sampleRate        = kRate;
    captureConfig.dataCallback      = captureThunk;
    captureConfig.pUserData         = this;

    m_capture = new ma_device;
    if (ma_device_init(nullptr, &captureConfig, dev(m_capture)) != MA_SUCCESS) {
        return fail(Lang::tr("Windows no dejo capturar la mezcla del sistema."));
    }

    // --- salida ------------------------------------------------------------
    ma_device_id playbackId{};
    const bool havePlaybackId = fromBytes(outputId, &playbackId);

    ma_device_config playbackConfig = ma_device_config_init(ma_device_type_playback);
    playbackConfig.playback.pDeviceID = havePlaybackId ? &playbackId : nullptr;
    playbackConfig.playback.format    = kFormat;
    playbackConfig.playback.channels  = kChannels;
    playbackConfig.sampleRate         = kRate;
    playbackConfig.dataCallback       = playbackThunk;
    playbackConfig.pUserData          = this;

    m_playback = new ma_device;
    if (ma_device_init(nullptr, &playbackConfig, dev(m_playback)) != MA_SUCCESS)
        return fail(Lang::tr("No se pudo abrir la salida elegida."));

    m_equalizer.prepare(kRate);
    m_effects.prepare(kRate);
    m_limiter.prepare(kRate, kChannels);
    applySettings();

    if (ma_device_start(dev(m_capture)) != MA_SUCCESS)
        return fail(Lang::tr("Windows no dejo capturar la mezcla del sistema."));
    if (ma_device_start(dev(m_playback)) != MA_SUCCESS)
        return fail(Lang::tr("No se pudo abrir la salida elegida."));

    m_running = true;
    m_error.clear();
    m_sync->start();
    return true;
}

void SystemAudioTap::stop()
{
    m_sync->stop();
    m_running = false;

    if (m_capture) {
        ma_device_uninit(dev(m_capture));
        delete dev(m_capture);
        m_capture = nullptr;
    }
    if (m_playback) {
        ma_device_uninit(dev(m_playback));
        delete dev(m_playback);
        m_playback = nullptr;
    }
    if (m_ring) {
        ma_pcm_rb_uninit(ring(m_ring));
        delete ring(m_ring);
        m_ring = nullptr;
    }
}

void SystemAudioTap::applySettings()
{
    // Los ajustes son la fuente de verdad: el panel del ecualizador y el rack
    // de efectos escriben ahi cada cambio, asi que copiarlos de aqui evita
    // duplicar el cableado de toda la interfaz.
    m_equalizer.setEnabled(Settings::eqEnabled());
    m_equalizer.setPreamp(Settings::eqPreamp());

    const QVector<float> gains = Settings::eqGains();
    const QVector<float> freqs = Settings::eqFrequencies();
    const QVector<float> qs    = Settings::eqQs();
    for (int band = 0; band < Equalizer::kBands; ++band) {
        m_equalizer.setGain(band, band < gains.size() ? gains.at(band) : 0.0f);
        m_equalizer.setFrequency(band, band < freqs.size()
                                           ? freqs.at(band)
                                           : Equalizer::defaultFrequencies()[band]);
        m_equalizer.setQ(band, band < qs.size() ? qs.at(band) : Equalizer::kDefaultQ);
    }

    for (int device = 0; device < Effects::DeviceCount; ++device) {
        const Effects::DeviceInfo& info = Effects::info(device);
        const QString id = QString::fromLatin1(info.id);
        m_effects.setDeviceEnabled(device, Settings::effectEnabled(id));
        for (int p = 0; p < info.paramCount; ++p) {
            m_effects.setParam(device, p,
                Settings::effectParam(id, QString::fromLatin1(info.params[p].name),
                                      info.params[p].defaultValue));
        }
    }
}

// ------------------------------------------------------------ hilos de audio

void SystemAudioTap::onCaptured(const void* input, unsigned frameCount)
{
    if (!m_ring || !input)
        return;

    ma_uint32 remaining = frameCount;
    const auto* src = static_cast<const float*>(input);

    while (remaining > 0) {
        void* region = nullptr;
        ma_uint32 chunk = remaining;
        if (ma_pcm_rb_acquire_write(ring(m_ring), &chunk, &region) != MA_SUCCESS || chunk == 0)
            break;   // el consumidor va atrasado: se descarta lo que sobra

        std::memcpy(region, src, size_t(chunk) * kChannels * sizeof(float));
        ma_pcm_rb_commit_write(ring(m_ring), chunk);

        src += size_t(chunk) * kChannels;
        remaining -= chunk;
    }
}

void SystemAudioTap::onPlayback(void* output, unsigned frameCount)
{
    auto* dst = static_cast<float*>(output);
    std::memset(dst, 0, size_t(frameCount) * kChannels * sizeof(float));
    if (!m_ring)
        return;

    ma_uint32 filled = 0;
    while (filled < frameCount) {
        void* region = nullptr;
        ma_uint32 chunk = frameCount - filled;
        if (ma_pcm_rb_acquire_read(ring(m_ring), &chunk, &region) != MA_SUCCESS || chunk == 0)
            break;   // todavia no hay audio capturado: sale silencio

        std::memcpy(dst + size_t(filled) * kChannels, region,
                    size_t(chunk) * kChannels * sizeof(float));
        ma_pcm_rb_commit_read(ring(m_ring), chunk);
        filled += chunk;
    }

    if (filled == 0)
        return;

    // Misma cadena que el reproductor, sobre el audio del sistema.
    m_equalizer.process(dst, filled, kChannels);
    m_effects.process(dst, filled, kChannels);
    m_limiter.process(dst, filled, kChannels);

    // Y al visualizador, ya procesado: lo que se ve en el espectro y en el
    // fondo es lo que de verdad esta saliendo por el altavoz.
    if (m_visualSink)
        m_visualSink->pushVisualSamples(dst, filled, kChannels);
}
