#pragma once

#include "core/TrackInfo.h"

#include <QAbstractListModel>

class QMimeData;
#include <QStringList>
#include <QVector>

// Una lista de reproduccion (una pestana del panel derecho).
class PlaylistModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TrackRole = Qt::UserRole + 1,
        RatingRole,
        CheckedRole,
        IsCurrentRole,
        PathRole
    };

    enum class RepeatMode { Off, All, One };

    explicit PlaylistModel(const QString& name = QStringLiteral("Default"),
                           QObject* parent = nullptr);

    // --- QAbstractListModel ------------------------------------------------
    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    bool     setData(const QModelIndex& index, const QVariant& value, int role) override;
    Qt::ItemFlags flags(const QModelIndex& index) const override;
    bool     removeRows(int row, int count, const QModelIndex& parent = QModelIndex()) override;
    Qt::DropActions supportedDropActions() const override;

    // Pistas soltadas desde la lista de archivos, desde el Explorador o desde
    // cualquier otro programa que exporte rutas.
    QStringList mimeTypes() const override;
    QMimeData*  mimeData(const QModelIndexList& indexes) const override;
    bool canDropMimeData(const QMimeData* data, Qt::DropAction action,
                         int row, int column, const QModelIndex& parent) const override;
    bool dropMimeData(const QMimeData* data, Qt::DropAction action,
                      int row, int column, const QModelIndex& parent) override;

    // Como appendFiles, pero colocando en una posicion concreta.
    int insertFiles(int row, const QStringList& paths);

    // --- contenido ---------------------------------------------------------
    QString name() const { return m_name; }
    void    setName(const QString& name);

    void appendTracks(const QVector<TrackInfo>& tracks);
    void appendFiles(const QStringList& paths);
    void insertTracks(int row, const QVector<TrackInfo>& tracks);
    void clearTracks();
    void removeIndexes(QList<int> rows);
    void moveRow(int from, int to);

    const TrackInfo& trackAt(int row) const;
    bool  updateTrack(int row, const TrackInfo& info);
    int   indexOfPath(const QString& path) const;
    const QVector<TrackInfo>& tracks() const { return m_tracks; }

    // --- reproduccion ------------------------------------------------------
    int  currentIndex() const { return m_currentIndex; }
    void setCurrentIndex(int row);

    int  nextIndex(bool shuffle, RepeatMode repeat) const;
    int  previousIndex(bool shuffle) const;
    void reshuffle();

    // --- orden -------------------------------------------------------------
    enum class SortKey { Title, Artist, Album, Duration, FileName, TrackNumber, Rating };
    void sortBy(SortKey key, Qt::SortOrder order = Qt::AscendingOrder);

    // --- totales para la barra de estado -----------------------------------
    qint64 totalDurationMs() const;
    qint64 totalBytes() const;
    int    checkedCount() const;

    // --- persistencia ------------------------------------------------------
    QStringList paths() const;

signals:
    void contentsChanged();
    void currentIndexChanged(int row);

private:
    void rebuildShuffleOrder();

    QString           m_name;
    QVector<TrackInfo> m_tracks;
    QVector<int>      m_shuffleOrder;
    int               m_currentIndex = -1;
    TrackInfo         m_invalid;
};
