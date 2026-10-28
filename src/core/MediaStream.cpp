#include "core/MediaStream.h"

#include "core/AudioEngine.h"
#include "core/Lang.h"

#include <QAudioBufferOutput>
#include <QAudioFormat>
#include <QLocale>
#include <QMediaMetaData>
#include <QMediaPlayer>
#include <QUrl>
#include <QVideoSink>

MediaStream::MediaStream(QObject* parent)
    : QObject(parent)
{
    m_player = new QMediaPlayer(this);

    // Sin salida de audio propia: el sonido se pide ya convertido al formato
    // del dispositivo de AudioEngine y lo reproduce el.
    QAudioFormat format;
    format.setSampleRate(AudioEngine::kDeviceSampleRate);
    format.setChannelCount(AudioEngine::kDeviceChannels);
    format.setSampleFormat(QAudioFormat::Float);
    m_audio = new QAudioBufferOutput(format, this);
    m_player->setAudioBufferOutput(m_audio);

    m_sink = new QVideoSink(this);
    m_player->setVideoSink(m_sink);

    // Directa: el sonido sale del hilo de FFmpeg hacia el buffer del motor sin
    // pasar por el de la interfaz, que con video va cargado.
    connect(m_audio, &QAudioBufferOutput::audioBufferReceived, this, &MediaStream::audioReady,
            Qt::DirectConnection);
    connect(m_player, &QMediaPlayer::durationChanged, this, &MediaStream::durationChanged);
    connect(m_player, &QMediaPlayer::hasVideoChanged, this, &MediaStream::hasVideoChanged);
    connect(m_player, &QMediaPlayer::tracksChanged, this, &MediaStream::tracksChanged);
    connect(m_player, &QMediaPlayer::activeTracksChanged, this, &MediaStream::tracksChanged);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this,
            [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::LoadedMedia && m_pendingSeek >= 0) {
            m_player->setPosition(m_pendingSeek);
            if (m_pendingPlay)
                m_player->play();
            m_pendingSeek = -1;
        }
        if (status == QMediaPlayer::EndOfMedia)
            emit finished();
        else if (status == QMediaPlayer::InvalidMedia)
            emit failed(Lang::tr("No se pudo abrir el archivo: %1").arg(m_player->errorString()));
    });
    connect(m_player, &QMediaPlayer::errorOccurred, this,
            [this](QMediaPlayer::Error error, const QString& message) {
        if (error != QMediaPlayer::NoError)
            emit failed(Lang::tr("No se pudo reproducir el archivo: %1").arg(message));
    });
}

void MediaStream::open(const QString& path)
{
    m_pendingSeek = -1;
    m_player->setSource(QUrl::fromLocalFile(path));
}

void MediaStream::openAt(const QString& path, qint64 ms, bool play)
{
    m_player->setSource(QUrl::fromLocalFile(path));
    m_pendingSeek = std::max<qint64>(0, ms);
    m_pendingPlay = play;
}

void MediaStream::close()
{
    m_player->stop();
    m_player->setSource(QUrl());
}

void MediaStream::play()  { m_player->play(); }
void MediaStream::pause() { m_player->pause(); }

void MediaStream::stop()
{
    m_player->stop();
    m_player->setPosition(0);
}

void MediaStream::seekMs(qint64 ms)
{
    m_player->setPosition(ms);
}

qint64 MediaStream::positionMs() const { return m_player->position(); }
qint64 MediaStream::durationMs() const { return m_player->duration(); }
bool   MediaStream::hasVideo() const   { return m_player->hasVideo(); }

double MediaStream::frameRate() const
{
    return m_player->metaData().value(QMediaMetaData::VideoFrameRate).toDouble();
}

namespace {

// "Pista 2 - Japones (Comentarios)": el numero siempre, y el idioma y el
// titulo cuando el archivo los dice.
QStringList trackNames(const QList<QMediaMetaData>& tracks)
{
    QStringList names;
    for (int i = 0; i < tracks.size(); ++i) {
        const QMediaMetaData& meta = tracks.at(i);
        QString name = Lang::tr("Pista %1").arg(i + 1);

        const QVariant language = meta.value(QMediaMetaData::Language);
        const auto code = language.value<QLocale::Language>();
        // En su propia lengua ("Espanol", "日本語"): languageToString() las da
        // todas en ingles.
        if (language.isValid() && code != QLocale::AnyLanguage && code != QLocale::C) {
            QString native = QLocale(code).nativeLanguageName();
            if (native.isEmpty())
                native = QLocale::languageToString(code);
            if (!native.isEmpty())
                native[0] = native.at(0).toUpper();
            name += QStringLiteral(" - ") + native;
        }

        const QString title = meta.stringValue(QMediaMetaData::Title);
        if (!title.isEmpty())
            name += QStringLiteral(" (%1)").arg(title);
        names << name;
    }
    return names;
}

} // namespace

QStringList MediaStream::audioTrackNames() const    { return trackNames(m_player->audioTracks()); }
QStringList MediaStream::subtitleTrackNames() const { return trackNames(m_player->subtitleTracks()); }
int  MediaStream::activeAudioTrack() const    { return m_player->activeAudioTrack(); }
int  MediaStream::activeSubtitleTrack() const { return m_player->activeSubtitleTrack(); }
void MediaStream::setActiveAudioTrack(int index)    { m_player->setActiveAudioTrack(index); }
void MediaStream::setActiveSubtitleTrack(int index) { m_player->setActiveSubtitleTrack(index); }
