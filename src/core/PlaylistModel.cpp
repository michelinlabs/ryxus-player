#include "core/PlaylistModel.h"
#include "core/MetadataService.h"

#include <QCollator>
#include <QDir>
#include <QFileInfo>
#include <QMimeData>
#include <QRandomGenerator>
#include <QUrl>

#include <algorithm>
#include <functional>
#include <numeric>

PlaylistModel::PlaylistModel(const QString& name, QObject* parent)
    : QAbstractListModel(parent)
    , m_name(name)
{
}

int PlaylistModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_tracks.size());
}

QVariant PlaylistModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return QVariant();

    const TrackInfo& track = m_tracks.at(index.row());

    switch (role) {
    case Qt::DisplayRole:  return track.displayName();
    case Qt::ToolTipRole:  return track.path;
    case TrackRole:        return QVariant::fromValue(track);
    case RatingRole:       return track.rating;
    case CheckedRole:      return track.checked;
    case IsCurrentRole:    return index.row() == m_currentIndex;
    case PathRole:         return track.path;
    case Qt::CheckStateRole:
        return track.checked ? Qt::Checked : Qt::Unchecked;
    default:
        return QVariant();
    }
}

bool PlaylistModel::setData(const QModelIndex& index, const QVariant& value, int role)
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tracks.size())
        return false;

    TrackInfo& track = m_tracks[index.row()];

    switch (role) {
    case Qt::CheckStateRole:
        track.checked = value.toInt() == Qt::Checked;
        break;
    case CheckedRole:
        track.checked = value.toBool();
        break;
    case RatingRole: {
        const int rating = qBound(0, value.toInt(), 5);
        if (track.rating == rating)
            return false;
        track.rating = rating;
        // La calificacion se persiste en el archivo, como hace AIMP.
        MetadataService::write(track);
        break;
    }
    case TrackRole:
        track = value.value<TrackInfo>();
        break;
    default:
        return false;
    }

    emit dataChanged(index, index, {role, Qt::DisplayRole});
    emit contentsChanged();
    return true;
}

Qt::ItemFlags PlaylistModel::flags(const QModelIndex& index) const
{
    Qt::ItemFlags base = Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled;
    if (!index.isValid())
        return base | Qt::ItemIsDropEnabled;
    return base | Qt::ItemIsUserCheckable | Qt::ItemIsDropEnabled;
}

Qt::DropActions PlaylistModel::supportedDropActions() const
{
    return Qt::MoveAction | Qt::CopyAction;
}

QStringList PlaylistModel::mimeTypes() const
{
    // El primero es el que usa la propia lista para reordenarse; el segundo,
    // el que traen la lista de archivos y el Explorador.
    QStringList types = QAbstractListModel::mimeTypes();
    types << QStringLiteral("text/uri-list");
    return types;
}

QMimeData* PlaylistModel::mimeData(const QModelIndexList& indexes) const
{
    QMimeData* data = QAbstractListModel::mimeData(indexes);
    if (!data)
        return nullptr;

    // Ademas del formato interno se exportan las rutas, para poder arrastrar
    // fuera del programa.
    QList<QUrl> urls;
    for (const QModelIndex& index : indexes) {
        if (index.isValid() && index.row() < m_tracks.size())
            urls << QUrl::fromLocalFile(m_tracks.at(index.row()).path);
    }
    if (!urls.isEmpty())
        data->setUrls(urls);

    return data;
}

bool PlaylistModel::canDropMimeData(const QMimeData* data, Qt::DropAction action,
                                    int row, int column, const QModelIndex& parent) const
{
    if (data && data->hasUrls())
        return true;
    return QAbstractListModel::canDropMimeData(data, action, row, column, parent);
}

bool PlaylistModel::dropMimeData(const QMimeData* data, Qt::DropAction action,
                                 int row, int column, const QModelIndex& parent)
{
    if (action == Qt::IgnoreAction)
        return true;

    if (data && data->hasUrls()) {
        // Soltar sobre una fila inserta justo ahi; soltar en el hueco de abajo
        // llega con row == -1 y va al final.
        int target = row;
        if (target < 0)
            target = parent.isValid() ? parent.row() : int(m_tracks.size());

        QStringList paths;
        const QList<QUrl> urls = data->urls();
        paths.reserve(urls.size());
        for (const QUrl& url : urls) {
            if (url.isLocalFile())
                paths << url.toLocalFile();
        }

        return insertFiles(target, paths) > 0;
    }

    return QAbstractListModel::dropMimeData(data, action, row, column, parent);
}

int PlaylistModel::insertFiles(int row, const QStringList& paths)
{
    QVector<TrackInfo> tracks;
    tracks.reserve(paths.size());

    for (const QString& path : paths) {
        // Soltar una carpeta trae la carpeta entera, que es lo que espera
        // cualquiera que arrastre un album desde el Explorador.
        const QFileInfo fi(path);
        if (fi.isDir()) {
            const QFileInfoList entries = QDir(path).entryInfoList(
                QDir::Files | QDir::Readable, QDir::Name | QDir::LocaleAware);
            for (const QFileInfo& entry : entries) {
                if (!TrackInfo::isSupported(entry.absoluteFilePath()))
                    continue;
                const TrackInfo info = MetadataService::read(entry.absoluteFilePath(), false);
                if (info.isValid())
                    tracks << info;
            }
            continue;
        }

        if (!TrackInfo::isSupported(path))
            continue;
        const TrackInfo info = MetadataService::read(path, false);
        if (info.isValid())
            tracks << info;
    }

    if (tracks.isEmpty())
        return 0;

    insertTracks(row, tracks);
    return int(tracks.size());
}

bool PlaylistModel::removeRows(int row, int count, const QModelIndex& parent)
{
    if (parent.isValid() || row < 0 || count <= 0 || row + count > m_tracks.size())
        return false;

    beginRemoveRows(QModelIndex(), row, row + count - 1);
    m_tracks.remove(row, count);
    endRemoveRows();

    if (m_currentIndex >= row + count)
        m_currentIndex -= count;
    else if (m_currentIndex >= row)
        m_currentIndex = -1;

    rebuildShuffleOrder();
    emit contentsChanged();
    return true;
}

void PlaylistModel::setName(const QString& name)
{
    if (m_name == name)
        return;
    m_name = name;
    emit contentsChanged();
}

void PlaylistModel::appendTracks(const QVector<TrackInfo>& tracks)
{
    insertTracks(int(m_tracks.size()), tracks);
}

void PlaylistModel::insertTracks(int row, const QVector<TrackInfo>& tracks)
{
    if (tracks.isEmpty())
        return;
    row = qBound(0, row, int(m_tracks.size()));

    beginInsertRows(QModelIndex(), row, row + int(tracks.size()) - 1);
    for (int i = 0; i < tracks.size(); ++i)
        m_tracks.insert(row + i, tracks.at(i));
    endInsertRows();

    if (m_currentIndex >= row)
        m_currentIndex += int(tracks.size());

    rebuildShuffleOrder();
    emit contentsChanged();
}

void PlaylistModel::appendFiles(const QStringList& paths)
{
    QVector<TrackInfo> tracks;
    tracks.reserve(paths.size());
    for (const QString& path : paths) {
        if (!TrackInfo::isSupported(path))
            continue;
        TrackInfo info = MetadataService::read(path, false);
        if (info.isValid())
            tracks.append(info);
    }
    appendTracks(tracks);
}

void PlaylistModel::clearTracks()
{
    if (m_tracks.isEmpty())
        return;
    beginResetModel();
    m_tracks.clear();
    m_shuffleOrder.clear();
    m_currentIndex = -1;
    endResetModel();
    emit contentsChanged();
}

void PlaylistModel::removeIndexes(QList<int> rows)
{
    // De mayor a menor, para que los indices sigan siendo validos al borrar.
    std::sort(rows.begin(), rows.end(), std::greater<int>());
    rows.erase(std::unique(rows.begin(), rows.end()), rows.end());
    for (int row : rows)
        removeRows(row, 1);
}

void PlaylistModel::moveRow(int from, int to)
{
    if (from == to || from < 0 || from >= m_tracks.size())
        return;
    to = qBound(0, to, int(m_tracks.size()) - 1);

    beginResetModel();
    const TrackInfo moved = m_tracks.takeAt(from);
    m_tracks.insert(to, moved);
    if (m_currentIndex == from)
        m_currentIndex = to;
    endResetModel();

    rebuildShuffleOrder();
    emit contentsChanged();
}

const TrackInfo& PlaylistModel::trackAt(int row) const
{
    if (row < 0 || row >= m_tracks.size())
        return m_invalid;
    return m_tracks.at(row);
}

bool PlaylistModel::updateTrack(int row, const TrackInfo& info)
{
    if (row < 0 || row >= m_tracks.size())
        return false;
    m_tracks[row] = info;
    const QModelIndex idx = index(row);
    emit dataChanged(idx, idx);
    emit contentsChanged();
    return true;
}

int PlaylistModel::indexOfPath(const QString& path) const
{
    for (int i = 0; i < m_tracks.size(); ++i) {
        if (m_tracks.at(i).path.compare(path, Qt::CaseInsensitive) == 0)
            return i;
    }
    return -1;
}

void PlaylistModel::setCurrentIndex(int row)
{
    if (row == m_currentIndex)
        return;

    const int previous = m_currentIndex;
    m_currentIndex = (row >= 0 && row < m_tracks.size()) ? row : -1;

    if (previous >= 0 && previous < m_tracks.size()) {
        const QModelIndex idx = index(previous);
        emit dataChanged(idx, idx, {IsCurrentRole});
    }
    if (m_currentIndex >= 0) {
        const QModelIndex idx = index(m_currentIndex);
        emit dataChanged(idx, idx, {IsCurrentRole});
    }
    emit currentIndexChanged(m_currentIndex);
}

void PlaylistModel::rebuildShuffleOrder()
{
    m_shuffleOrder.resize(m_tracks.size());
    std::iota(m_shuffleOrder.begin(), m_shuffleOrder.end(), 0);
}

void PlaylistModel::reshuffle()
{
    rebuildShuffleOrder();
    if (m_shuffleOrder.size() < 2)
        return;

    auto* rng = QRandomGenerator::global();
    for (int i = int(m_shuffleOrder.size()) - 1; i > 0; --i) {
        const int j = int(rng->bounded(i + 1));
        std::swap(m_shuffleOrder[i], m_shuffleOrder[j]);
    }

    // La pista en curso encabeza el orden aleatorio, para que "siguiente"
    // avance de verdad en vez de repetirla.
    if (m_currentIndex >= 0) {
        const int pos = int(m_shuffleOrder.indexOf(m_currentIndex));
        if (pos > 0)
            std::swap(m_shuffleOrder[0], m_shuffleOrder[pos]);
    }
}

int PlaylistModel::nextIndex(bool shuffle, RepeatMode repeat) const
{
    if (m_tracks.isEmpty())
        return -1;
    if (repeat == RepeatMode::One && m_currentIndex >= 0)
        return m_currentIndex;

    if (shuffle) {
        if (m_shuffleOrder.size() != m_tracks.size())
            return int(QRandomGenerator::global()->bounded(m_tracks.size()));
        const int pos = int(m_shuffleOrder.indexOf(m_currentIndex));
        if (pos < 0)
            return m_shuffleOrder.first();
        if (pos + 1 < m_shuffleOrder.size())
            return m_shuffleOrder.at(pos + 1);
        return repeat == RepeatMode::All ? m_shuffleOrder.first() : -1;
    }

    if (m_currentIndex + 1 < m_tracks.size())
        return m_currentIndex + 1;
    return repeat == RepeatMode::All ? 0 : -1;
}

int PlaylistModel::previousIndex(bool shuffle) const
{
    if (m_tracks.isEmpty())
        return -1;

    if (shuffle && m_shuffleOrder.size() == m_tracks.size()) {
        const int pos = int(m_shuffleOrder.indexOf(m_currentIndex));
        if (pos > 0)
            return m_shuffleOrder.at(pos - 1);
        return m_shuffleOrder.last();
    }

    if (m_currentIndex > 0)
        return m_currentIndex - 1;
    return int(m_tracks.size()) - 1;
}

void PlaylistModel::sortBy(SortKey key, Qt::SortOrder order)
{
    if (m_tracks.size() < 2)
        return;

    const QString currentPath = (m_currentIndex >= 0) ? m_tracks.at(m_currentIndex).path
                                                      : QString();

    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);

    const auto less = [&](const TrackInfo& a, const TrackInfo& b) {
        switch (key) {
        case SortKey::Title:       return collator.compare(a.displayTitle(), b.displayTitle()) < 0;
        case SortKey::Artist:      return collator.compare(a.displayArtist(), b.displayArtist()) < 0;
        case SortKey::Album:       return collator.compare(a.album, b.album) < 0;
        case SortKey::Duration:    return a.durationMs < b.durationMs;
        case SortKey::FileName:    return collator.compare(a.fileName, b.fileName) < 0;
        case SortKey::TrackNumber: return a.trackNumber < b.trackNumber;
        case SortKey::Rating:      return a.rating < b.rating;
        }
        return false;
    };

    beginResetModel();
    std::stable_sort(m_tracks.begin(), m_tracks.end(),
                     [&](const TrackInfo& a, const TrackInfo& b) {
                         return order == Qt::AscendingOrder ? less(a, b) : less(b, a);
                     });
    endResetModel();

    if (!currentPath.isEmpty())
        m_currentIndex = indexOfPath(currentPath);

    rebuildShuffleOrder();
    emit contentsChanged();
}

qint64 PlaylistModel::totalDurationMs() const
{
    qint64 total = 0;
    for (const TrackInfo& track : m_tracks)
        total += track.durationMs;
    return total;
}

qint64 PlaylistModel::totalBytes() const
{
    qint64 total = 0;
    for (const TrackInfo& track : m_tracks)
        total += track.fileSize;
    return total;
}

int PlaylistModel::checkedCount() const
{
    int count = 0;
    for (const TrackInfo& track : m_tracks)
        if (track.checked)
            ++count;
    return count;
}

QStringList PlaylistModel::paths() const
{
    QStringList result;
    result.reserve(m_tracks.size());
    for (const TrackInfo& track : m_tracks)
        result << track.path;
    return result;
}
