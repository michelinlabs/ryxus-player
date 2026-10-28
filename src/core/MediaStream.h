#pragma once

#include <QObject>
#include <QString>
#include <QStringList>

class QAudioBuffer;
class QAudioBufferOutput;
class QMediaPlayer;
class QVideoSink;

// Reproduccion con FFmpeg (el motor de Qt Multimedia) para lo que miniaudio
// no abre: video en cualquier formato y audio comprimido que no trae de serie
// (AAC, WMA, Opus, APE...).
//
// El sonido no sale por aqui. Se entrega ya decodificado, en el formato del
// dispositivo, a AudioEngine, que le pasa la misma cadena que a todo lo demas
// (ecualizador, efectos, limitador y volumen). Asi el banco de filtros se
// aplica tambien a la pista de audio de un video. La imagen va a videoSink(),
// de donde la toma el panel de video.
class MediaStream : public QObject {
    Q_OBJECT

public:
    explicit MediaStream(QObject* parent = nullptr);

    void open(const QString& path);
    // Abre y, en cuanto este cargado, salta a `ms` y arranca si `play`. Para
    // una pista de audio aparte que tiene que ir al paso de un video.
    void openAt(const QString& path, qint64 ms, bool play);
    void close();

    void play();
    void pause();
    void stop();
    void seekMs(qint64 ms);

    qint64 positionMs() const;
    qint64 durationMs() const;
    bool   hasVideo() const;
    double frameRate() const;      // fotogramas por segundo; 0 si no se sabe

    // Pistas que trae el archivo (idiomas de audio, subtitulos), con nombre
    // legible. Los subtitulos admiten -1: desactivados.
    QStringList audioTrackNames() const;
    QStringList subtitleTrackNames() const;
    int  activeAudioTrack() const;
    int  activeSubtitleTrack() const;
    void setActiveAudioTrack(int index);
    void setActiveSubtitleTrack(int index);

    QVideoSink* videoSink() const { return m_sink; }

signals:
    // Estereo, en coma flotante y a la frecuencia del dispositivo.
    void audioReady(const QAudioBuffer& buffer);
    void durationChanged(qint64 ms);
    void hasVideoChanged(bool hasVideo);
    void tracksChanged();
    void finished();
    void failed(const QString& message);

private:
    QMediaPlayer*       m_player = nullptr;
    QAudioBufferOutput* m_audio  = nullptr;
    QVideoSink*         m_sink   = nullptr;

    qint64 m_pendingSeek = -1;     // ver openAt()
    bool   m_pendingPlay = false;
};
