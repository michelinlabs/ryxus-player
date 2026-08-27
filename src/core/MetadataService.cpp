#include "core/MetadataService.h"
#include "core/Lang.h"

#include <QBuffer>
#include <QDir>
#include <QFileInfo>
#include <QRegularExpression>

#include <taglib/audioproperties.h>
#include <taglib/fileref.h>
#include <taglib/tbytevector.h>
#include <taglib/tfile.h>
#include <taglib/tlist.h>
#include <taglib/tpropertymap.h>
#include <taglib/tstring.h>
#include <taglib/tstringlist.h>
#include <taglib/tvariant.h>

namespace {

TagLib::String toTag(const QString& s)
{
    return TagLib::String(s.toStdWString());
}

QString fromTag(const TagLib::String& s)
{
    return QString::fromStdWString(s.toWString());
}

TagLib::FileName toFileName(const QString& path)
{
    // TagLib acepta wchar_t* en Windows, lo que preserva rutas Unicode.
    static thread_local std::wstring buffer;
    buffer = QDir::toNativeSeparators(path).toStdWString();
    return TagLib::FileName(buffer.c_str());
}

QString firstValue(const TagLib::PropertyMap& props, const char* key)
{
    const TagLib::String k(key);
    if (!props.contains(k))
        return QString();
    const TagLib::StringList values = props[k];
    if (values.isEmpty())
        return QString();
    return fromTag(values.front());
}

// "8/18" -> 8 ; "2011-05-01" -> 2011
int leadingNumber(const QString& raw)
{
    static const QRegularExpression re(QStringLiteral("(\\d+)"));
    const auto m = re.match(raw);
    return m.hasMatch() ? m.captured(1).toInt() : 0;
}

void setOrRemove(TagLib::PropertyMap& props, const char* key, const QString& value)
{
    const TagLib::String k(key);
    props.erase(k);
    if (!value.trimmed().isEmpty())
        props.insert(k, TagLib::StringList(toTag(value)));
}

void setOrRemoveNumber(TagLib::PropertyMap& props, const char* key, int value)
{
    setOrRemove(props, key, value > 0 ? QString::number(value) : QString());
}

QImage decodePicture(const TagLib::VariantMap& raw)
{
    TagLib::VariantMap picture = raw;  // copia: Map::operator[] no es const
    if (!picture.contains("data"))
        return QImage();

    const TagLib::ByteVector data = picture["data"].toByteVector();
    if (data.isEmpty())
        return QImage();

    QImage image;
    image.loadFromData(reinterpret_cast<const uchar*>(data.data()),
                       static_cast<int>(data.size()));
    return image;
}

QImage pickFrontCover(const TagLib::List<TagLib::VariantMap>& pictures)
{
    if (pictures.isEmpty())
        return QImage();

    // Se prefiere explicitamente la portada frontal si el archivo trae varias.
    for (auto it = pictures.begin(); it != pictures.end(); ++it) {
        TagLib::VariantMap picture = *it;
        if (picture.contains("pictureType")) {
            const QString type = fromTag(picture["pictureType"].toString());
            if (type.compare(QStringLiteral("Front Cover"), Qt::CaseInsensitive) == 0) {
                const QImage image = decodePicture(*it);
                if (!image.isNull())
                    return image;
            }
        }
    }

    for (auto it = pictures.begin(); it != pictures.end(); ++it) {
        const QImage image = decodePicture(*it);
        if (!image.isNull())
            return image;
    }
    return QImage();
}

} // namespace

namespace MetadataService {

TrackInfo read(const QString& path, bool withCover)
{
    TrackInfo info;
    const QFileInfo fi(path);
    if (!fi.exists() || !fi.isFile())
        return info;

    info.path     = fi.absoluteFilePath();
    info.fileName = fi.fileName();
    info.fileSize = fi.size();
    info.format   = fi.suffix().toUpper();

    TagLib::FileRef file(toFileName(info.path), true, TagLib::AudioProperties::Average);
    if (file.isNull())
        return info;

    if (const TagLib::AudioProperties* props = file.audioProperties()) {
        info.durationMs  = props->lengthInMilliseconds();
        info.bitrateKbps = props->bitrate();
        info.sampleRate  = props->sampleRate();
        info.channels    = props->channels();
    }

    const TagLib::PropertyMap tags = file.properties();
    info.title       = firstValue(tags, "TITLE");
    info.artist      = firstValue(tags, "ARTIST");
    info.album       = firstValue(tags, "ALBUM");
    info.albumArtist = firstValue(tags, "ALBUMARTIST");
    info.genre       = firstValue(tags, "GENRE");
    info.comment     = firstValue(tags, "COMMENT");
    info.composer    = firstValue(tags, "COMPOSER");
    info.year        = leadingNumber(firstValue(tags, "DATE"));
    info.trackNumber = leadingNumber(firstValue(tags, "TRACKNUMBER"));
    info.discNumber  = leadingNumber(firstValue(tags, "DISCNUMBER"));

    const int rating = leadingNumber(firstValue(tags, "RATING"));
    info.rating = qBound(0, rating, 5);

    if (withCover)
        info.cover = pickFrontCover(file.complexProperties("PICTURE"));

    info.tagsLoaded = true;
    return info;
}

QImage readCover(const QString& path)
{
    TagLib::FileRef file(toFileName(path), false);
    if (file.isNull())
        return QImage();
    return pickFrontCover(file.complexProperties("PICTURE"));
}

bool write(const TrackInfo& info, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    if (info.path.isEmpty())
        return fail(Lang::tr("No hay archivo seleccionado."));

    const QFileInfo fi(info.path);
    if (!fi.exists())
        return fail(Lang::tr("El archivo ya no existe."));
    if (!fi.isWritable())
        return fail(Lang::tr("El archivo es de solo lectura."));

    TagLib::FileRef file(toFileName(info.path), false);
    if (file.isNull())
        return fail(Lang::tr("Formato no reconocido por TagLib."));

    // Se parte de las etiquetas existentes para no perder campos que la
    // interfaz no expone (BPM, ISRC, letras, etc.).
    TagLib::PropertyMap props = file.properties();

    setOrRemove(props, "TITLE",       info.title);
    setOrRemove(props, "ARTIST",      info.artist);
    setOrRemove(props, "ALBUM",       info.album);
    setOrRemove(props, "ALBUMARTIST", info.albumArtist);
    setOrRemove(props, "GENRE",       info.genre);
    setOrRemove(props, "COMMENT",     info.comment);
    setOrRemove(props, "COMPOSER",    info.composer);
    setOrRemoveNumber(props, "DATE",        info.year);
    setOrRemoveNumber(props, "TRACKNUMBER", info.trackNumber);
    setOrRemoveNumber(props, "DISCNUMBER",  info.discNumber);
    setOrRemoveNumber(props, "RATING",      info.rating);

    file.setProperties(props);

    if (!file.save())
        return fail(Lang::tr("TagLib no pudo escribir en el archivo."));

    if (error)
        error->clear();
    return true;
}

bool writeCover(const QString& path, const QImage& cover, QString* error)
{
    const auto fail = [error](const QString& message) {
        if (error)
            *error = message;
        return false;
    };

    TagLib::FileRef file(toFileName(path), false);
    if (file.isNull())
        return fail(Lang::tr("Formato no reconocido por TagLib."));

    TagLib::List<TagLib::VariantMap> pictures;

    if (!cover.isNull()) {
        // Se normaliza a JPEG 600x600: es lo que aceptan todos los formatos
        // de etiqueta y mantiene el archivo en un tamano razonable.
        QImage scaled = cover;
        if (scaled.width() > 600 || scaled.height() > 600)
            scaled = scaled.scaled(600, 600, Qt::KeepAspectRatio, Qt::SmoothTransformation);

        QByteArray bytes;
        QBuffer buffer(&bytes);
        buffer.open(QIODevice::WriteOnly);
        if (!scaled.save(&buffer, "JPEG", 92))
            return fail(Lang::tr("No se pudo codificar la imagen."));
        buffer.close();

        TagLib::VariantMap picture;
        picture.insert("data", TagLib::ByteVector(bytes.constData(),
                                                  static_cast<unsigned int>(bytes.size())));
        picture.insert("mimeType",    TagLib::String("image/jpeg"));
        picture.insert("pictureType", TagLib::String("Front Cover"));
        picture.insert("description", TagLib::String(""));
        pictures.append(picture);
    }

    // Una lista vacia elimina todas las imagenes incrustadas.
    file.setComplexProperties("PICTURE", pictures);

    if (!file.save())
        return fail(Lang::tr("TagLib no pudo escribir la caratula."));

    if (error)
        error->clear();
    return true;
}

bool isWritable(const QString& path)
{
    const QFileInfo fi(path);
    return fi.exists() && fi.isFile() && fi.isWritable();
}

} // namespace MetadataService
