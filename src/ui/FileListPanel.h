#pragma once

#include "core/TrackInfo.h"

#include <QAbstractTableModel>
#include <QVector>
#include <QWidget>

class QLabel;
class QLineEdit;
class QSortFilterProxyModel;
class QTreeView;

// Modelo tabular de la lista central. Las columnas replican las del skin de
// referencia y se amplian con artista, album, duracion y tamano.
class FileTableModel : public QAbstractTableModel {
    Q_OBJECT

public:
    enum Column {
        TrackNumber = 0,
        FileName,
        Title,
        Artist,
        Album,
        Duration,
        Size,
        ColumnCount
    };

    enum Roles { TrackRole = Qt::UserRole + 1, PathRole, SortRole };

    explicit FileTableModel(QObject* parent = nullptr);

    int      rowCount(const QModelIndex& parent = QModelIndex()) const override;
    int      columnCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

    void appendTracks(const QVector<TrackInfo>& tracks);
    void clearTracks();
    void replaceTrack(const TrackInfo& info);

    const TrackInfo& trackAt(int row) const;
    const QVector<TrackInfo>& tracks() const { return m_tracks; }

private:
    QVector<TrackInfo> m_tracks;
    TrackInfo m_invalid;
};

// Panel central: tabla de archivos de la carpeta seleccionada, busqueda
// rapida y barra de totales.
class FileListPanel : public QWidget {
    Q_OBJECT

public:
    explicit FileListPanel(QWidget* parent = nullptr);

    void beginScan(const QString& folder);
    void appendTracks(const QVector<TrackInfo>& tracks);
    void endScan(int totalFiles);

    QVector<TrackInfo> selectedTracks() const;
    QVector<TrackInfo> allTracks() const;
    void refreshTrack(const TrackInfo& info);

signals:
    void trackActivated(const TrackInfo& info);
    void selectionChanged(const TrackInfo& info);
    void editTagsRequested(const TrackInfo& info);
    void playRequested(const QVector<TrackInfo>& tracks);
    void enqueueRequested(const QVector<TrackInfo>& tracks);
    void propertiesRequested(const TrackInfo& info);
    void rereadTagsRequested(const QVector<TrackInfo>& tracks);
    void ratingRequested(const QVector<TrackInfo>& tracks, int rating);
    void revealRequested(const QString& path);
    void deleteFilesRequested(const QVector<TrackInfo>& tracks);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private slots:
    void showColumnMenu(const QPoint& pos);
    void onCurrentRowChanged();
    void showContextMenu(const QPoint& pos);
    void onDoubleClicked(const QModelIndex& index);
    void updateStatus();

private:
    void buildUi();

    FileTableModel*        m_model  = nullptr;
    QSortFilterProxyModel* m_proxy  = nullptr;
    QTreeView*             m_view   = nullptr;
    QLineEdit*             m_search = nullptr;
    QLabel*                m_status = nullptr;
    QString                m_folder;
};
