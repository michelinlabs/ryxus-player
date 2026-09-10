#include "ui/PlaylistPanel.h"
#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/PlaylistDelegate.h"
#include "ui/RibbonTabBar.h"
#include "ui/Theme.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QItemSelectionModel>
#include <QLabel>
#include <QLineEdit>
#include <QListView>
#include <QMenu>
#include <QMessageBox>
#include <QSortFilterProxyModel>
#include <QVBoxLayout>

PlaylistPanel::PlaylistPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("playlistPanel"));
    buildUi();

    addPlaylist(QStringLiteral("Default"));
    setCurrentPlaylistIndex(0);
}

void PlaylistPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- pestanas ----------------------------------------------------------
    m_tabs = new RibbonTabBar(this);
    m_tabs->setShowAddButton(true);
    m_tabs->setClosableTabs(true);
    root->addWidget(m_tabs);

    connect(m_tabs, &RibbonTabBar::currentChanged, this, &PlaylistPanel::onTabChanged);
    connect(m_tabs, &RibbonTabBar::addRequested,   this, &PlaylistPanel::onAddPlaylist);
    connect(m_tabs, &RibbonTabBar::closeRequested, this, &PlaylistPanel::onClosePlaylist);
    connect(m_tabs, &RibbonTabBar::tabContextMenuRequested,
            this, &PlaylistPanel::onTabContextMenu);

    // --- lista -------------------------------------------------------------
    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterRole(Qt::DisplayRole);

    // --- fila de cabecera: vaciar la cola ----------------------------------
    auto* headerRow = new QWidget(this);
    headerRow->setObjectName(QStringLiteral("playlistHeaderRow"));
    headerRow->setFixedHeight(26);
    auto* headerLayout = new QHBoxLayout(headerRow);
    headerLayout->setContentsMargins(10, 0, 6, 0);
    headerLayout->setSpacing(6);
    headerLayout->addStretch(1);

    m_clearButton = new FlatButton(Lang::tr("Vaciar"), headerRow);
    m_clearButton->setIconId(Icons::Trash);
    m_clearButton->setGlyphSize(13);
    m_clearButton->setFixedHeight(20);
    m_clearButton->setToolTip(Lang::tr("Quitar todas las pistas de la lista"));
    headerLayout->addWidget(m_clearButton);
    root->addWidget(headerRow);

    connect(m_clearButton, &FlatButton::clicked, this, [this]() {
        auto* playlist = currentPlaylist();
        if (!playlist || playlist->rowCount() == 0)
            return;

        // Vaciar no se puede deshacer y el boton esta a un clic de distancia,
        // asi que se pregunta antes.
        const auto answer = QMessageBox::question(
            this, Lang::tr("Vaciar la lista"),
            Lang::tr("Se quitaran %1 pista(s) de la lista.\n"
                     "Los archivos no se tocan.").arg(playlist->rowCount()),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

        if (answer == QMessageBox::Yes)
            playlist->clearTracks();
    });

    m_view = new QListView(this);
    m_view->setObjectName(QStringLiteral("playlistView"));
    m_view->setModel(m_proxy);
    m_view->setItemDelegate(new PlaylistDelegate(this));
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setUniformItemSizes(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setMouseTracking(true);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    m_view->setDragDropMode(QAbstractItemView::InternalMove);
    m_view->setDefaultDropAction(Qt::MoveAction);
    root->addWidget(m_view, 1);

    connect(m_view, &QListView::doubleClicked, this, &PlaylistPanel::onItemActivated);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentChanged, this,
            [this](const QModelIndex& current, const QModelIndex&) {
                PlaylistModel* playlist = currentPlaylist();
                if (!playlist || !current.isValid())
                    return;
                const int row = m_proxy->mapToSource(current).row();
                if (row >= 0 && row < playlist->rowCount())
                    emit selectionChanged(playlist->trackAt(row));
            });
    connect(m_view, &QListView::customContextMenuRequested,
            this, &PlaylistPanel::showContextMenu);

    // --- totales -----------------------------------------------------------
    m_totals = new QLabel(this);
    m_totals->setObjectName(QStringLiteral("playlistTotals"));
    m_totals->setFont(Theme::uiFont(8));
    m_totals->setAlignment(Qt::AlignCenter);
    m_totals->setFixedHeight(20);
    root->addWidget(m_totals);

    // --- barra inferior ----------------------------------------------------
    auto* toolRow = new QWidget(this);
    toolRow->setObjectName(QStringLiteral("searchRow"));
    toolRow->setFixedHeight(Theme::Metrics::ToolRowHeight);
    auto* toolLayout = new QHBoxLayout(toolRow);
    toolLayout->setContentsMargins(10, 0, 6, 0);
    toolLayout->setSpacing(2);

    auto* searchIcon = new FlatButton(Icons::Search, toolRow);
    searchIcon->setFixedDiameter(20);
    searchIcon->setGlyphSize(14);
    searchIcon->setStrokeWidth(1.3);
    searchIcon->setEnabled(false);
    toolLayout->addWidget(searchIcon);

    m_search = new QLineEdit(toolRow);
    m_search->setObjectName(QStringLiteral("quickSearch"));
    m_search->setPlaceholderText(Lang::tr("Busqueda rapida"));
    m_search->setFont(Theme::uiFont(9));
    m_search->setFrame(false);
    m_search->setClearButtonEnabled(true);
    toolLayout->addWidget(m_search, 1);

    const auto makeTool = [toolRow](Icons::Icon icon, const QString& tip) {
        auto* button = new FlatButton(icon, toolRow);
        button->setFixedDiameter(24);
        button->setGlyphSize(16);
        button->setStrokeWidth(1.4);
        button->setToolTip(tip);
        return button;
    };

    m_addButton    = makeTool(Icons::Plus,       Lang::tr("Anadir archivos"));
    m_removeButton = makeTool(Icons::Minus,      Lang::tr("Quitar seleccionados"));
    m_moreButton   = makeTool(Icons::More,       Lang::tr("Mas acciones"));
    m_sortButton   = makeTool(Icons::SortUpDown, Lang::tr("Ordenar"));
    m_menuButton   = makeTool(Icons::Menu,       Lang::tr("Menu de la lista"));

    toolLayout->addWidget(m_addButton);
    toolLayout->addWidget(m_removeButton);
    toolLayout->addWidget(m_moreButton);
    toolLayout->addWidget(m_sortButton);
    toolLayout->addWidget(m_menuButton);

    root->addWidget(toolRow);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setFilterFixedString(text);
        updateTotals();
    });

    connect(m_addButton, &FlatButton::clicked, this, [this]() {
        const QStringList files = QFileDialog::getOpenFileNames(
            this, Lang::tr("Anadir archivos a la lista"),
            QString(), TrackInfo::supportedFilter());
        if (!files.isEmpty() && currentPlaylist())
            currentPlaylist()->appendFiles(files);
    });

    connect(m_removeButton, &FlatButton::clicked, this, [this]() {
        if (auto* playlist = currentPlaylist())
            playlist->removeIndexes(selectedSourceRows());
    });

    connect(m_moreButton, &FlatButton::clicked, this, [this]() {
        auto* playlist = currentPlaylist();
        if (!playlist)
            return;
        QMenu menu(this);
        menu.setFont(Theme::uiFont(9));
        menu.addAction(Lang::tr("Vaciar lista"), playlist, &PlaylistModel::clearTracks);
        menu.addAction(Lang::tr("Marcar todo"), this, [playlist]() {
            for (int row = 0; row < playlist->rowCount(); ++row)
                playlist->setData(playlist->index(row), true, PlaylistModel::CheckedRole);
        });
        menu.addAction(Lang::tr("Desmarcar todo"), this, [playlist]() {
            for (int row = 0; row < playlist->rowCount(); ++row)
                playlist->setData(playlist->index(row), false, PlaylistModel::CheckedRole);
        });
        menu.exec(m_moreButton->mapToGlobal(QPoint(0, m_moreButton->height())));
    });

    connect(m_sortButton, &FlatButton::clicked, this, [this]() {
        auto* playlist = currentPlaylist();
        if (!playlist)
            return;

        QMenu menu(this);
        menu.setFont(Theme::uiFont(9));
        const struct { const char* label; PlaylistModel::SortKey key; } entries[] = {
            {"Por titulo",         PlaylistModel::SortKey::Title},
            {"Por artista",        PlaylistModel::SortKey::Artist},
            {"Por album",          PlaylistModel::SortKey::Album},
            {"Por duracion",       PlaylistModel::SortKey::Duration},
            {"Por nombre de archivo", PlaylistModel::SortKey::FileName},
            {"Por N.o de pista",   PlaylistModel::SortKey::TrackNumber},
            {"Por calificacion",   PlaylistModel::SortKey::Rating},
        };
        for (const auto& entry : entries) {
            const PlaylistModel::SortKey key = entry.key;
            menu.addAction(QString::fromLatin1(entry.label), this, [playlist, key]() {
                playlist->sortBy(key);
            });
        }
        menu.addSeparator();
        menu.addAction(Lang::tr("Mezclar orden"), playlist, &PlaylistModel::reshuffle);
        menu.exec(m_sortButton->mapToGlobal(QPoint(0, m_sortButton->height())));
    });

    connect(m_menuButton, &FlatButton::clicked, this, [this]() {
        QMenu menu(this);
        menu.setFont(Theme::uiFont(9));
        menu.addAction(Lang::tr("Nueva lista"), this, &PlaylistPanel::onAddPlaylist);
        menu.addAction(Lang::tr("Renombrar lista actual"), this, [this]() {
            onTabContextMenu(currentPlaylistIndex(), QCursor::pos());
        });
        menu.exec(m_menuButton->mapToGlobal(QPoint(0, m_menuButton->height())));
    });
}

PlaylistModel* PlaylistPanel::addPlaylist(const QString& name)
{
    auto* playlist = new PlaylistModel(name, this);
    m_playlists.append(playlist);
    m_tabs->addTab(name);
    connectModel(playlist);
    return playlist;
}

void PlaylistPanel::connectModel(PlaylistModel* model)
{
    connect(model, &PlaylistModel::contentsChanged, this, &PlaylistPanel::updateTotals);
    connect(model, &QAbstractItemModel::rowsInserted, this, &PlaylistPanel::updateTotals);
    connect(model, &QAbstractItemModel::rowsRemoved,  this, &PlaylistPanel::updateTotals);
    connect(model, &QAbstractItemModel::modelReset,   this, &PlaylistPanel::updateTotals);
}

void PlaylistPanel::removePlaylist(int index)
{
    if (index < 0 || index >= m_playlists.size() || m_playlists.size() == 1)
        return;

    PlaylistModel* doomed = m_playlists.takeAt(index);
    m_tabs->removeTab(index);
    doomed->deleteLater();
}

int PlaylistPanel::currentPlaylistIndex() const
{
    return m_tabs->currentIndex();
}

PlaylistModel* PlaylistPanel::currentPlaylist() const
{
    return playlistAt(m_tabs->currentIndex());
}

PlaylistModel* PlaylistPanel::playlistAt(int index) const
{
    if (index < 0 || index >= m_playlists.size())
        return nullptr;
    return m_playlists.at(index);
}

void PlaylistPanel::setCurrentPlaylistIndex(int index)
{
    if (index < 0 || index >= m_playlists.size())
        return;
    m_tabs->setCurrentIndex(index);
    onTabChanged(index);
}

void PlaylistPanel::onTabChanged(int index)
{
    PlaylistModel* playlist = playlistAt(index);
    if (!playlist)
        return;

    m_proxy->setSourceModel(playlist);
    updateTotals();
    emit playlistChanged(playlist);
}

void PlaylistPanel::onAddPlaylist()
{
    bool accepted = false;
    const QString name = QInputDialog::getText(
        this, Lang::tr("Nueva lista de reproduccion"),
        Lang::tr("Nombre:"), QLineEdit::Normal,
        Lang::tr("Lista %1").arg(m_playlists.size() + 1), &accepted);

    if (!accepted || name.trimmed().isEmpty())
        return;

    addPlaylist(name.trimmed());
    setCurrentPlaylistIndex(int(m_playlists.size()) - 1);
}

void PlaylistPanel::onClosePlaylist(int index)
{
    removePlaylist(index);
}

void PlaylistPanel::onTabContextMenu(int index, const QPoint& globalPos)
{
    PlaylistModel* playlist = playlistAt(index);
    if (!playlist)
        return;

    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));

    QAction* rename = menu.addAction(Lang::tr("Renombrar..."));
    QAction* clear  = menu.addAction(Lang::tr("Vaciar"));
    menu.addSeparator();
    QAction* close  = menu.addAction(Lang::tr("Cerrar lista"));
    close->setEnabled(m_playlists.size() > 1);

    QAction* chosen = menu.exec(globalPos);
    if (chosen == rename) {
        bool accepted = false;
        const QString name = QInputDialog::getText(
            this, Lang::tr("Renombrar lista"), Lang::tr("Nombre:"),
            QLineEdit::Normal, playlist->name(), &accepted);
        if (accepted && !name.trimmed().isEmpty()) {
            playlist->setName(name.trimmed());
            m_tabs->renameTab(index, name.trimmed());
        }
    } else if (chosen == clear) {
        playlist->clearTracks();
    } else if (chosen == close) {
        removePlaylist(index);
    }
}

void PlaylistPanel::enqueue(const QVector<TrackInfo>& tracks)
{
    if (auto* playlist = currentPlaylist())
        playlist->appendTracks(tracks);
}

void PlaylistPanel::selectRow(int row)
{
    auto* playlist = currentPlaylist();
    if (!playlist || row < 0)
        return;

    const QModelIndex proxyIndex = m_proxy->mapFromSource(playlist->index(row));
    if (!proxyIndex.isValid())
        return;

    m_view->setCurrentIndex(proxyIndex);
    m_view->scrollTo(proxyIndex, QAbstractItemView::EnsureVisible);
}

QList<int> PlaylistPanel::selectedSourceRows() const
{
    QList<int> rows;
    for (const QModelIndex& proxyIndex : m_view->selectionModel()->selectedIndexes())
        rows << m_proxy->mapToSource(proxyIndex).row();
    return rows;
}

void PlaylistPanel::onItemActivated(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    emit trackActivated(m_proxy->mapToSource(index).row());
}

void PlaylistPanel::showContextMenu(const QPoint& pos)
{
    auto* playlist = currentPlaylist();
    const QModelIndex proxyIndex = m_view->indexAt(pos);
    if (!playlist || !proxyIndex.isValid())
        return;

    const int row = m_proxy->mapToSource(proxyIndex).row();
    const TrackInfo track = playlist->trackAt(row);

    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));

    QAction* play = menu.addAction(Icons::icon(Icons::Play, Theme::TextDim),
                                   Lang::tr("Reproducir"));
    menu.addSeparator();
    QAction* properties = menu.addAction(Icons::icon(Icons::Info, Theme::TextDim),
                                         Lang::tr("Propiedades"));
    QAction* reveal = menu.addAction(Icons::icon(Icons::FolderOpen, Theme::TextDim),
                                     Lang::tr("Ubicacion de archivo"));
    menu.addSeparator();

    QMenu* rating = menu.addMenu(Icons::icon(Icons::Star, Theme::TextDim),
                                 Lang::tr("Calificacion"));
    rating->setFont(Theme::uiFont(9));
    QAction* ratingActions[6];
    ratingActions[0] = rating->addAction(Lang::tr("Sin calificar"));
    for (int stars = 1; stars <= 5; ++stars)
        ratingActions[stars] = rating->addAction(QString(stars, QChar(0x2605)));

    menu.addSeparator();
    QAction* remove = menu.addAction(Icons::icon(Icons::Minus, Theme::TextDim),
                                     Lang::tr("Eliminar seleccionados de la lista"));
    remove->setShortcut(QKeySequence(Qt::Key_Delete));

    QAction* chosen = menu.exec(m_view->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == play) {
        emit trackActivated(row);
    } else if (chosen == properties) {
        emit propertiesRequested(track);
    } else if (chosen == reveal) {
        emit revealRequested(track.path);
    } else if (chosen == remove) {
        playlist->removeIndexes(selectedSourceRows());
    } else {
        for (int stars = 0; stars <= 5; ++stars) {
            if (chosen != ratingActions[stars])
                continue;
            for (int selected : selectedSourceRows())
                playlist->setData(playlist->index(selected), stars, PlaylistModel::RatingRole);
            break;
        }
    }
}

void PlaylistPanel::updateTotals()
{
    auto* playlist = currentPlaylist();
    if (!playlist) {
        m_totals->clear();
        return;
    }

    // Mismo formato que la referencia: "15 / 01:03:46 / 98,57 MB".
    m_totals->setText(QStringLiteral("%1 / %2 / %3")
                          .arg(playlist->rowCount())
                          .arg(TrackInfo::formatDuration(int(playlist->totalDurationMs()), true))
                          .arg(TrackInfo::formatSize(playlist->totalBytes())));
}

QList<QPair<QString, QStringList>> PlaylistPanel::serialize() const
{
    QList<QPair<QString, QStringList>> result;
    for (PlaylistModel* playlist : m_playlists)
        result.append({playlist->name(), playlist->paths()});
    return result;
}

void PlaylistPanel::restore(const QList<QPair<QString, QStringList>>& lists, int active)
{
    if (lists.isEmpty())
        return;

    // Se descartan las listas de arranque y se reconstruyen las guardadas.
    while (m_playlists.size() > 0) {
        PlaylistModel* doomed = m_playlists.takeLast();
        doomed->deleteLater();
    }
    m_tabs->setTabs(QStringList());

    for (const auto& entry : lists) {
        PlaylistModel* playlist = addPlaylist(entry.first);
        playlist->appendFiles(entry.second);
    }

    setCurrentPlaylistIndex(qBound(0, active, int(m_playlists.size()) - 1));
}
