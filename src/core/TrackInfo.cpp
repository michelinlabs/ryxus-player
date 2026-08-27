#include "core/TrackInfo.h"
#include "core/Lang.h"

#include <QFileInfo>
#include <QLocale>

QString TrackInfo::displayTitle() const
{
    if (!title.isEmpty())
        return title;
    if (!fileName.isEmpty())
        return QFileInfo(fileName).completeBaseName();
    return QFileInfo(path).completeBaseName();
}

QString TrackInfo::displayArtist() const
{
    if (!artist.isEmpty())
        return artist;
    if (!albumArtist.isEmpty())
        return albumArtist;
    return Lang::tr("Artista desconocido");
}

QString TrackInfo::displayName() const
{
    if (!artist.isEmpty() && !title.isEmpty())
        return artist + QStringLiteral(" - ") + title;
    return displayTitle();
}

QString TrackInfo::techLine() const
{
    QStringList parts;
    if (!format.isEmpty())
        parts << format;

    QStringList tech;
    if (sampleRate > 0)
        tech << QStringLiteral("%1 kHz").arg(sampleRate / 1000);
    if (bitrateKbps > 0)
        tech << QStringLiteral("%1 kbps").arg(bitrateKbps);
    if (fileSize > 0)
        tech << formatSize(fileSize);

    if (parts.isEmpty())
        return tech.join(QStringLiteral(", "));
    return parts.join(QString()) + QStringLiteral(" :: ") + tech.join(QStringLiteral(", "));
}

QString TrackInfo::formatLine() const
{
    QStringList parts;
    if (!format.isEmpty())
        parts << format;
    if (sampleRate > 0)
        parts << QStringLiteral("%1 kHz").arg(sampleRate / 1000);
    if (bitrateKbps > 0)
        parts << QStringLiteral("%1 kbps").arg(bitrateKbps);

    switch (channels) {
    case 1:  parts << Lang::tr("Mono");   break;
    case 2:  parts << Lang::tr("Stereo"); break;
    default:
        if (channels > 2)
            parts << Lang::tr("%1 canales").arg(channels);
        break;
    }
    return parts.join(QStringLiteral(", "));
}

QString TrackInfo::formatDuration(int ms, bool withHours)
{
    if (ms < 0)
        ms = 0;
    const int totalSeconds = ms / 1000;
    const int hours   = totalSeconds / 3600;
    const int minutes = (totalSeconds / 60) % 60;
    const int seconds = totalSeconds % 60;

    if (withHours || hours > 0)
        return QStringLiteral("%1:%2:%3")
            .arg(hours)
            .arg(minutes, 2, 10, QLatin1Char('0'))
            .arg(seconds, 2, 10, QLatin1Char('0'));

    return QStringLiteral("%1:%2")
        .arg(minutes)
        .arg(seconds, 2, 10, QLatin1Char('0'));
}

QString TrackInfo::formatSize(qint64 bytes)
{
    QLocale locale;
    if (bytes >= 1024LL * 1024LL * 1024LL)
        return locale.toString(bytes / double(1024LL * 1024LL * 1024LL), 'f', 2)
             + QStringLiteral(" GB");
    if (bytes >= 1024LL * 1024LL)
        return locale.toString(bytes / double(1024LL * 1024LL), 'f', 2)
             + QStringLiteral(" MB");
    if (bytes >= 1024LL)
        return locale.toString(bytes / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return QStringLiteral("%1 B").arg(bytes);
}

QStringList TrackInfo::supportedSuffixes()
{
    // Decodificadores integrados en miniaudio + los que TagLib sabe etiquetar.
    static const QStringList kSuffixes = {
        QStringLiteral("mp3"), QStringLiteral("flac"), QStringLiteral("wav"),
        QStringLiteral("ogg"), QStringLiteral("oga"), QStringLiteral("m4a"),
        QStringLiteral("aac"), QStringLiteral("wma"), QStringLiteral("opus"),
        QStringLiteral("aiff"), QStringLiteral("aif"), QStringLiteral("ape"),
        QStringLiteral("wv"),  QStringLiteral("mpc")
    };
    return kSuffixes;
}

QString TrackInfo::supportedFilter()
{
    QStringList globs;
    for (const QString& s : supportedSuffixes())
        globs << QStringLiteral("*.") + s;
    return Lang::tr("Archivos de audio (%1);;Todos los archivos (*)")
        .arg(globs.join(QLatin1Char(' ')));
}

bool TrackInfo::isSupported(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return supportedSuffixes().contains(suffix);
}
