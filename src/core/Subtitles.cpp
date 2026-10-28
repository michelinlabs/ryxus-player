#include "core/Subtitles.h"

#include "core/Lang.h"

#include <QFile>
#include <QFileInfo>
#include <QRegularExpression>
#include <QStringDecoder>

#include <algorithm>

namespace {

// "01:02:03,456", "01:02:03.456" o "1:02:03.45" (ASS, en centesimas) a ms.
qint64 parseTime(const QString& text)
{
    static const QRegularExpression re(
        QStringLiteral(R"((?:(\d+):)?(\d{1,2}):(\d{1,2})[,.](\d{1,3}))"));
    const QRegularExpressionMatch m = re.match(text.trimmed());
    if (!m.hasMatch())
        return -1;

    const qint64 h  = m.captured(1).toLongLong();
    const qint64 mi = m.captured(2).toLongLong();
    const qint64 s  = m.captured(3).toLongLong();
    QString frac = m.captured(4);
    // La fraccion puede venir en decimas, centesimas o milesimas.
    while (frac.size() < 3)
        frac.append(QLatin1Char('0'));
    return ((h * 60 + mi) * 60 + s) * 1000 + frac.toLongLong();
}

// Quita las etiquetas de formato: <i>, <font ...> (SRT/VTT) y {\...} (ASS).
QString stripTags(QString text)
{
    static const QRegularExpression html(QStringLiteral("<[^>]*>"));
    static const QRegularExpression ass(QStringLiteral(R"(\{[^}]*\})"));
    text.remove(html);
    text.remove(ass);
    text.replace(QStringLiteral("\\N"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\n"), QStringLiteral("\n"));
    text.replace(QStringLiteral("\\h"), QStringLiteral(" "));
    return text.trimmed();
}

// Casi todos vienen en UTF-8, pero los viejos suelen estar en Windows-1252:
// si el UTF-8 no cuadra, se lee como Latin-1 para no mostrar simbolos raros.
QString decode(const QByteArray& data)
{
    QStringDecoder utf8(QStringDecoder::Utf8);
    QString text = utf8(data);
    if (utf8.hasError())
        text = QString::fromLatin1(data);
    if (text.startsWith(QChar(0xFEFF)))
        text.remove(0, 1);
    return text;
}

} // namespace

bool Subtitles::isSubtitleFile(const QString& path)
{
    const QString suffix = QFileInfo(path).suffix().toLower();
    return suffix == QLatin1String("srt") || suffix == QLatin1String("vtt")
        || suffix == QLatin1String("ass") || suffix == QLatin1String("ssa");
}

QString Subtitles::fileFilter()
{
    return Lang::tr("Subtitulos (*.srt *.vtt *.ass *.ssa);;Todos los archivos (*)");
}

void Subtitles::clear()
{
    m_cues.clear();
    m_path.clear();
}

bool Subtitles::load(const QString& path)
{
    clear();

    QFile file(path);
    if (!file.open(QIODevice::ReadOnly))
        return false;
    const QString text = decode(file.readAll());

    const QString suffix = QFileInfo(path).suffix().toLower();
    if (suffix == QLatin1String("ass") || suffix == QLatin1String("ssa"))
        parseAss(text);
    else
        parseSrtOrVtt(text);

    std::sort(m_cues.begin(), m_cues.end(),
              [](const Cue& a, const Cue& b) { return a.start < b.start; });
    if (m_cues.isEmpty())
        return false;
    m_path = path;
    return true;
}

void Subtitles::parseSrtOrVtt(const QString& text)
{
    // Bloques separados por linea en blanco; en cada uno, la linea con
    // "-->" da los tiempos y lo que sigue es el texto. El numero de SRT y la
    // cabecera WEBVTT no tienen "-->" y se saltan solos.
    const QStringList lines = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
                                  .split(QLatin1Char('\n'));
    for (int i = 0; i < lines.size(); ++i) {
        const QString& line = lines.at(i);
        const int arrow = line.indexOf(QStringLiteral("-->"));
        if (arrow < 0)
            continue;

        Cue cue;
        cue.start = parseTime(line.left(arrow));
        // En VTT puede haber ajustes de posicion tras el tiempo final.
        cue.end = parseTime(line.mid(arrow + 3).trimmed().section(QLatin1Char(' '), 0, 0));
        if (cue.start < 0 || cue.end <= cue.start)
            continue;

        QStringList body;
        while (i + 1 < lines.size() && !lines.at(i + 1).trimmed().isEmpty())
            body << lines.at(++i);
        cue.text = stripTags(body.join(QLatin1Char('\n')));
        if (!cue.text.isEmpty())
            m_cues.append(cue);
    }
}

void Subtitles::parseAss(const QString& text)
{
    // "Dialogue: capa,inicio,fin,estilo,nombre,mL,mR,mV,efecto,texto": el
    // texto es el ultimo campo y puede llevar comas.
    const QStringList lines = QString(text).replace(QStringLiteral("\r\n"), QStringLiteral("\n"))
                                  .split(QLatin1Char('\n'));
    for (const QString& line : lines) {
        if (!line.startsWith(QStringLiteral("Dialogue:")))
            continue;
        const QString fields = line.mid(9);
        const QStringList parts = fields.split(QLatin1Char(','));
        if (parts.size() < 10)
            continue;

        Cue cue;
        cue.start = parseTime(parts.at(1));
        cue.end   = parseTime(parts.at(2));
        cue.text  = stripTags(parts.mid(9).join(QLatin1Char(',')));
        if (cue.start >= 0 && cue.end > cue.start && !cue.text.isEmpty())
            m_cues.append(cue);
    }
}

QString Subtitles::textAt(qint64 ms) const
{
    // Busqueda binaria del ultimo que empieza antes de `ms`, y se juntan los
    // que se solapan (dos personajes hablando a la vez, por ejemplo).
    const auto it = std::upper_bound(m_cues.cbegin(), m_cues.cend(), ms,
                                     [](qint64 t, const Cue& c) { return t < c.start; });
    QStringList active;
    for (auto c = m_cues.cbegin(); c != it; ++c) {
        if (ms < c->end)
            active << c->text;
    }
    return active.join(QLatin1Char('\n'));
}
