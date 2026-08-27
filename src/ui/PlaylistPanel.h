#pragma once

#include "core/PlaylistModel.h"

#include <QVector>
#include <QWidget>

class FlatButton;
class QLabel;
class QLineEdit;
class QListView;
class QSortFilterProxyModel;
class RibbonTabBar;

// Columna derecha: pestanas de listas, la lista en si (delegado de dos
// lineas), totales y la barra de herramientas inferior.
class PlaylistPanel : public QWidget {
    Q_OBJECT

public:
    explicit PlaylistPanel(QWidget* parent = nullptr);

    PlaylistModel* currentPlaylist() const;
    PlaylistModel* playlistAt(int index) const;
    int  playlistCount() const { return int(m_playlists.size()); }
    int  currentPlaylistIndex() const;

    PlaylistModel* addPlaylist(const QString& name);
    void removePlaylist(int index);
    void setCurrentPlaylistIndex(int index);

    void enqueue(const QVector<TrackInfo>& tracks);
    void selectRow(int row);

    QList<QPair<QString, QStringList>> serialize() const;
    void restore(const QList<QPair<QString, QStringList>>& lists, int active);

signals:
    void trackActivated(int row);
    void playlistChanged(PlaylistModel* playlist);
    void propertiesRequested(const TrackInfo& info);
    void revealRequested(const QString& path);

private slots:
    void onTabChanged(int index);
    void onAddPlaylist();
    void onClosePlaylist(int index);
    void onTabContextMenu(int index, const QPoint& globalPos);
    void onItemActivated(const QModelIndex& index);
    void showContextMenu(const QPoint& pos);
    void updateTotals();

private:
    void buildUi();
    void connectModel(PlaylistModel* model);
    QList<int> selectedSourceRows() const;

    RibbonTabBar*          m_tabs   = nullptr;
    QListView*             m_view   = nullptr;
    QSortFilterProxyModel* m_proxy  = nullptr;
    QLineEdit*             m_search = nullptr;
    QLabel*                m_totals = nullptr;

    FlatButton* m_addButton    = nullptr;
    FlatButton* m_removeButton = nullptr;
    FlatButton* m_moreButton   = nullptr;
    FlatButton* m_sortButton   = nullptr;
    FlatButton* m_searchButton = nullptr;
    FlatButton* m_menuButton   = nullptr;

    QVector<PlaylistModel*> m_playlists;
};
