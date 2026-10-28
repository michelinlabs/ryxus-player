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
//
// Lo que miniaudio no sabe abrir -- video en cualquier formato y audio como
// AAC, WMA u Opus -- lo decodifica MediaStream con FFmpeg. Su sonido entra en
// la misma cadena por un buffer circular, en lugar del decodificador.
class MediaStream;
class QAudioBuffer;
class QTimer;
class QVideoSink;

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

    // Lo que suena viene de FFmpeg (MediaStream) y no de miniaudio, y si
    // ademas tiene imagen. La imagen se recoge de videoSink().
    bool isStreaming() const { return m_streamMode.load(std::memory_order_relaxed); }
    bool hasVideo() const;
    QVideoSink* videoSink() const;
    // Duracion de un fotograma del video en curso (33 ms si no se sabe).
    double videoFrameMs() const;

    // Formatos que se abren con FFmpeg aunque miniaudio pudiera con ellos.
    static bool needsStream(const QString& path);

    // --- pistas del video --------------------------------------------------
    // Las que trae el archivo, para elegir idioma y subtitulos.
    QStringList audioTrackNames() const;
    QStringList subtitleTrackNames() const;
    int  activeAudioTrack() const;
    int  activeSubtitleTrack() const;      // -1: desactivados
    void setAudioTrack(int index);         // deja de usar la pista externa
    void setSubtitleTrack(int index);

    // Una pista de audio de otro archivo en lugar de la del video (un doblaje,
    // la banda sonora aparte...). Va al paso del video: arranca, para y salta
    // con el, y se corrige si se desvia. Tambien pasa por el banco de filtros.
    bool setExternalAudio(const QString& path);
    void clearExternalAudio();
    QString externalAudio() const { return m_externalAudioPath; }

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

    // Salta el ecualizador, los efectos y el limitador del reproductor.
    //
    // Se activa mientras la captura del sistema esta en marcha: entonces lo
    // que suena aqui sale por la salida de Windows, la captura lo recoge y le
    // aplica la misma cadena. Procesarlo tambien aqui lo ecualizaria dos
    // veces. El visualizador pasa a alimentarse solo desde la captura.
    void setChainBypassed(bool bypassed);
    bool isChainBypassed() const { return m_chainBypassed.load(std::memory_order_relaxed); }

    // Copia las ultimas `count` muestras mono para el visualizador.
    void copyVisualSamples(float* out, int count) const;

    // Alimenta el buffer del visualizador desde fuera del motor.
    //
    // Lo usa la captura del audio del sistema: asi el espectro del ecualizador,
    // la onda del panel izquierdo y las visualizaciones del fondo reaccionan a
    // lo que entra por el cable virtual, no solo a lo que reproduce el
    // programa. Se ignora mientras el motor este sonando con su propia cadena,
    // para que las dos fuentes no se pisen.
    void pushVisualSamples(const float* interleaved, unsigned frameCount, int channels);

    // Devuelve true una sola vez, cuando la pista llego al final.
    bool takeFinishedFlag();

    // Veces que el dispositivo pidio sonido de FFmpeg y no habia bastante
    // (un corte audible) desde la ultima llamada. Para medir el rendimiento.
    quint32 takeStreamUnderruns() { return m_streamUnderruns.exchange(0); }

    // Llamada exclusivamente desde el hilo de audio de miniaudio.
    void render(float* output, unsigned frameCount);

signals:
    void stateChanged(AudioEngine::State state);
    void trackOpened(const QString& path, qint64 durationMs);

    // La duracion de lo que abre FFmpeg se conoce un poco despues de abrirlo.
    void durationChanged(qint64 durationMs);
    // Empieza o deja de haber imagen: la ventana cambia a disposicion de video.
    void videoActiveChanged(bool active);
    // Fallo al abrir o reproducir con FFmpeg (no se sabe al llamar a open()).
    void mediaError(const QString& message);
    // Cambiaron las pistas disponibles o la elegida (para rehacer menus).
    void tracksChanged();

private:
    void setState(State state);
    void releaseDecoderLocked();
    bool openStream(const QString& path);
    void closeStream();
    void onStreamAudio(const QAudioBuffer& buffer);
    unsigned readStream(float* output, unsigned frameCount);

    void* m_device  = nullptr;   // ma_device*
    void* m_decoder = nullptr;   // ma_decoder*

    mutable std::mutex m_decoderMutex;

    // --- reproduccion con FFmpeg -------------------------------------------
    MediaStream* m_stream = nullptr;
    MediaStream* m_external = nullptr;            // pista de audio aparte
    QString      m_externalAudioPath;
    std::atomic<bool> m_externalActive{false};    // lo mismo, legible desde FFmpeg
    std::mutex   m_ringWriteMutex;                // por si escriben las dos pistas
    QTimer*      m_externalSync = nullptr;        // corrige la deriva
    void*        m_streamRing = nullptr;          // ma_pcm_rb*: hilo GUI -> audio
    std::atomic<bool> m_streamMode{false};
    std::atomic<bool> m_streamFlush{false};       // tras un salto: tirar lo viejo
    std::atomic<quint32> m_streamUnderruns{0};
    bool         m_videoActive = false;

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
    std::atomic<bool>   m_chainBypassed{false};
    std::atomic<qint64> m_positionFrames{0};
    std::atomic<qint64> m_totalFrames{0};

    // Buffer circular de muestras mono para el visualizador. La carrera con
    // el hilo de audio es benigna: solo alimenta el dibujo.
    mutable float m_visual[kVisualBufferSize] = {};
    std::atomic<unsigned> m_visualWrite{0};

    // Rampa de volumen para evitar chasquidos al mover el deslizador.
    float m_smoothedVolume = 0.8f;
};
