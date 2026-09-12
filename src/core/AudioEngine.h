#pragma once

#include "core/Effects.h"
#include "core/Equalizer.h"
#include "core/Limiter.h"

#include <QObject>
#include <QString>

#include <atomic>
#include <mutex>

// Motor de reproduccion sobre miniaudio.
//
// El dispositivo se abre una sola vez con un formato fijo (f32 / 2 canales /
// 48 kHz) y el decodificador se configura para convertir a ese formato, de
// modo que cambiar de pista no reinicia el dispositivo (sin cortes ni clics).
//
// Cadena de proceso en el callback de audio:
//     decodificador -> ecualizador (12 bandas) -> efectos -> limitador ->
//     volumen -> visualizador
class AudioEngine : public QObject {
    Q_OBJECT

public:
    enum class State { Stopped, Playing, Paused };
    Q_ENUM(State)

    static constexpr int      kDeviceSampleRate = 48000;
    static constexpr int      kDeviceChannels   = 2;
    static constexpr unsigned kVisualBufferSize = 65536;

    explicit AudioEngine(QObject* parent = nullptr);
    ~AudioEngine() override;

    bool isInitialized() const { return m_deviceReady; }
    QString lastError() const  { return m_lastError; }

    bool open(const QString& path);
    void close();

    void play();
    void pause();
    void stop();
    void togglePlayPause();

    State  state() const   { return m_state.load(std::memory_order_relaxed); }
    bool   isPlaying() const { return state() == State::Playing; }
    QString currentPath() const { return m_currentPath; }

    qint64 positionMs() const;
    qint64 durationMs() const;
    void   seekMs(qint64 ms);
    void   seekFraction(double fraction);

    void  setVolume(float linear);         // 0.0 .. 1.0
    float volume() const { return m_volume.load(std::memory_order_relaxed); }
    void  setMuted(bool muted);
    bool  isMuted() const { return m_muted.load(std::memory_order_relaxed); }

    Equalizer& equalizer() { return m_equalizer; }
    Effects&   effects()   { return m_effects; }

    // Copia las ultimas `count` muestras mono para el visualizador.
    void copyVisualSamples(float* out, int count) const;

    // Alimenta el buffer del visualizador desde fuera del motor.
    //
    // Lo usa la captura del audio del sistema: asi el espectro del ecualizador,
    // la onda del panel izquierdo y las visualizaciones del fondo reaccionan a
    // lo que entra por el cable virtual, no solo a lo que reproduce el
    // programa. Se ignora mientras el motor este sonando, para que las dos
    // fuentes no se pisen.
    void pushVisualSamples(const float* interleaved, unsigned frameCount, int channels);

    // Devuelve true una sola vez, cuando la pista llego al final.
    bool takeFinishedFlag();

    // Llamada exclusivamente desde el hilo de audio de miniaudio.
    void render(float* output, unsigned frameCount);

signals:
    void stateChanged(AudioEngine::State state);
    void trackOpened(const QString& path, qint64 durationMs);

private:
    void setState(State state);
    void releaseDecoderLocked();

    void* m_device  = nullptr;   // ma_device*
    void* m_decoder = nullptr;   // ma_decoder*

    mutable std::mutex m_decoderMutex;

    bool    m_deviceReady = false;
    QString m_lastError;
    QString m_currentPath;

    Equalizer m_equalizer;
    Effects   m_effects;
    Limiter   m_limiter;

    std::atomic<State>  m_state{State::Stopped};
    std::atomic<float>  m_volume{0.8f};
    std::atomic<bool>   m_muted{false};
    std::atomic<bool>   m_finished{false};
    std::atomic<qint64> m_positionFrames{0};
    std::atomic<qint64> m_totalFrames{0};

    // Buffer circular de muestras mono para el visualizador. La carrera con
    // el hilo de audio es benigna: solo alimenta el dibujo.
    mutable float m_visual[kVisualBufferSize] = {};
    std::atomic<unsigned> m_visualWrite{0};

    // Rampa de volumen para evitar chasquidos al mover el deslizador.
    float m_smoothedVolume = 0.8f;
};
