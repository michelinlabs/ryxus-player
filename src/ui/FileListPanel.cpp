#include "ui/FileListPanel.h"
#include "core/Lang.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QSortFilterProxyModel>
#include <QTreeView>
#include <QVBoxLayout>

// -------------------------------------------------------- FileTableModel

FileTableModel::FileTableModel(QObject* parent)
    : QAbstractTableModel(parent)
{
}

int FileTableModel::rowCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : int(m_tracks.size());
}

int FileTableModel::columnCount(const QModelIndex& parent) const
{
    return parent.isValid() ? 0 : ColumnCount;
}

QVariant FileTableModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid() || index.row() >= m_tracks.size())
        return QVariant();

    const TrackInfo& track = m_tracks.at(index.row());

    switch (role) {
    case TrackRole: return QVariant::fromValue(track);
    case PathRole:  return track.path;

    case Qt::ToolTipRole:
        return track.path;

    case Qt::TextAlignmentRole:
        switch (index.column()) {
        case TrackNumber:
        case Duration:
        case Size:
            return int(Qt::AlignRight | Qt::AlignVCenter);
        default:
            return int(Qt::AlignLeft | Qt::AlignVCenter);
        }

    case Qt::ForegroundRole:
        // Las columnas derivadas de etiquetas se atenuan si faltan.
        if (index.column() == Title && track.title.isEmpty())
            return QColor(Theme::TextFaint);
        if (index.column() == TrackNumber)
            return QColor(Theme::TextDim);
        return QVariant();

    case SortRole:
        switch (index.column()) {
        case TrackNumber: return track.trackNumber;
        case Duration:    return track.durationMs;
        case Size:        return track.fileSize;
        default:          return data(index, Qt::DisplayRole);
        }

    case Qt::DisplayRole:
        switch (index.column()) {
        case TrackNumber: return track.trackNumber > 0 ? QString::number(track.trackNumber)
                                                       : QString::number(index.row() + 1);
        case FileName:    return track.fileName;
        case Title:       return track.displayTitle();
        case Artist:      return track.artist;
        case Album:       return track.album;
        case Duration:    return TrackInfo::formatDuration(track.durationMs);
        case Size:        return TrackInfo::formatSize(track.fileSize);
        default:          return QVariant();
        }

    default:
        return QVariant();
    }
}

QVariant FileTableModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation != Qt::Horizontal)
        return QVariant();

    if (role == Qt::TextAlignmentRole) {
        if (section == TrackNumber || section == Duration || section == Size)
            return int(Qt::AlignRight | Qt::AlignVCenter);
        return int(Qt::AlignLeft | Qt::AlignVCenter);
    }

    if (role != Qt::DisplayRole)
        return QVariant();

    switch (section) {
    case TrackNumber: return Lang::tr("Pista N.o");
    case FileName:    return Lang::tr("Nombre de archivo");
    case Title:       return Lang::tr("Titulo");
    case Artist:      return Lang::tr("Artista");
    case Album:       return Lang::tr("Album");
    case Duration:    return Lang::tr("Duracion");
    case Size:        return Lang::tr("Tamano");
    default:          return QVariant();
    }
}

void FileTableModel::appendTracks(const QVector<TrackInfo>& tracks)
{
    if (tracks.isEmpty())
        return;
    beginInsertRows(QModelIndex(), int(m_tracks.size()),
                    int(m_tracks.size() + tracks.size()) - 1);
    m_tracks += tracks;
    endInsertRows();
}

void FileTableModel::clearTracks()
{
    beginResetModel();
    m_tracks.clear();
    endResetModel();
}

void FileTableModel::replaceTrack(const TrackInfo& info)
{
    for (int row = 0; row < m_tracks.size(); ++row) {
        if (m_tracks.at(row).path.compare(info.path, Qt::CaseInsensitive) != 0)
            continue;
        m_tracks[row] = info;
        emit dataChanged(index(row, 0), index(row, ColumnCount - 1));
        return;
    }
}

const TrackInfo& FileTableModel::trackAt(int row) const
{
    if (row < 0 || row >= m_tracks.size())
        return m_invalid;
    return m_tracks.at(row);
}

// --------------------------------------------------------- FileListPanel

FileListPanel::FileListPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("fileListPanel"));
    buildUi();
}

void FileListPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_model = new FileTableModel(this);

    m_proxy = new QSortFilterProxyModel(this);
    m_proxy->setSourceModel(m_model);
    m_proxy->setFilterCaseSensitivity(Qt::CaseInsensitive);
    m_proxy->setFilterKeyColumn(-1);            // busca en todas las columnas
    m_proxy->setSortRole(FileTableModel::SortRole);

    m_view = new QTreeView(this);
    m_view->setObjectName(QStringLiteral("fileList"));
    m_view->setModel(m_proxy);
    m_view->setRootIsDecorated(false);
    m_view->setUniformRowHeights(true);
    m_view->setAlternatingRowColors(true);
    m_view->setSelectionBehavior(QAbstractItemView::SelectRows);
    m_view->setSelectionMode(QAbstractItemView::ExtendedSelection);
    m_view->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_view->setSortingEnabled(true);
    m_view->setFrameShape(QFrame::NoFrame);
    m_view->setContextMenuPolicy(Qt::CustomContextMenu);
    m_view->setDragEnabled(true);
    m_view->setDragDropMode(QAbstractItemView::DragOnly);

    QHeaderView* header = m_view->header();
    header->setObjectName(QStringLiteral("fileListHeader"));
    header->setSectionResizeMode(QHeaderView::Interactive);
    header->setStretchLastSection(false);
    header->setHighlightSections(false);
    header->setSortIndicatorShown(true);

    m_view->setColumnWidth(FileTableModel::TrackNumber, 78);
    m_view->setColumnWidth(FileTableModel::FileName,   300);
    m_view->setColumnWidth(FileTableModel::Title,      220);
    m_view->setColumnWidth(FileTableModel::Artist,     170);
    m_view->setColumnWidth(FileTableModel::Album,      170);
    m_view->setColumnWidth(FileTableModel::Duration,    70);
    m_view->setColumnWidth(FileTableModel::Size,        80);

    // De salida solo se ven las tres columnas del skin de referencia. El
    // resto de los datos vive en la ficha del panel izquierdo, que es donde
    // hay hueco; se pueden reactivar desde el menu de la cabecera.
    for (int column : {FileTableModel::Artist, FileTableModel::Album,
                       FileTableModel::Duration, FileTableModel::Size})
        m_view->setColumnHidden(column, true);

    header->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(header, &QHeaderView::customContextMenuRequested,
            this, &FileListPanel::showColumnMenu);

    root->addWidget(m_view, 1);

    connect(m_view, &QTreeView::customContextMenuRequested,
            this, &FileListPanel::showContextMenu);
    connect(m_view, &QTreeView::doubleClicked, this, &FileListPanel::onDoubleClicked);
    connect(m_view->selectionModel(), &QItemSelectionModel::currentRowChanged,
            this, &FileListPanel::onCurrentRowChanged);

    // --- busqueda rapida + totales ----------------------------------------
    auto* bottom = new QWidget(this);
    bottom->setObjectName(QStringLiteral("searchRow"));
    bottom->setFixedHeight(Theme::Metrics::ToolRowHeight);
    auto* bottomLayout = new QHBoxLayout(bottom);
    bottomLayout->setContentsMargins(10, 0, 10, 0);
    bottomLayout->setSpacing(6);

    auto* searchIcon = new FlatButton(Icons::Search, bottom);
    searchIcon->setFixedDiameter(20);
    searchIcon->setGlyphSize(14);
    searchIcon->setStrokeWidth(1.3);
    searchIcon->setEnabled(false);
    bottomLayout->addWidget(searchIcon);

    m_search = new QLineEdit(bottom);
    m_search->setObjectName(QStringLiteral("quickSearch"));
    m_search->setPlaceholderText(Lang::tr("Busqueda rapida"));
    m_search->setFont(Theme::uiFont(9));
    m_search->setFrame(false);
    m_search->setClearButtonEnabled(true);
    bottomLayout->addWidget(m_search, 1);

    m_status = new QLabel(bottom);
    m_status->setObjectName(QStringLiteral("statusText"));
    m_status->setFont(Theme::uiFont(8));
    bottomLayout->addWidget(m_status);

    root->addWidget(bottom);

    connect(m_search, &QLineEdit::textChanged, this, [this](const QString& text) {
        m_proxy->setFilterFixedString(text);
        updateStatus();
    });
    connect(m_model, &QAbstractItemModel::rowsInserted, this, &FileListPanel::updateStatus);
    connect(m_model, &QAbstractItemModel::modelReset,   this, &FileListPanel::updateStatus);

    updateStatus();
}

void FileListPanel::beginScan(const QString& folder)
{
    m_folder = folder;
    m_model->clearTracks();
    m_status->setText(Lang::tr("Leyendo..."));
}

void FileListPanel::appendTracks(const QVector<TrackInfo>& tracks)
{
    m_model->appendTracks(tracks);
}

void FileListPanel::endScan(int)
{
    updateStatus();
}

void FileListPanel::refreshTrack(const TrackInfo& info)
{
    m_model->replaceTrack(info);
}

void FileListPanel::updateStatus()
{
    const int shown = m_proxy->rowCount();
    qint64 totalMs = 0;
    for (int row = 0; row < shown; ++row) {
        const QModelIndex source = m_proxy->mapToSource(m_proxy->index(row, 0));
        totalMs += m_model->trackAt(source.row()).durationMs;
    }

    m_status->setText(QStringLiteral("%1 / %2")
                          .arg(shown)
                          .arg(TrackInfo::formatDuration(int(totalMs), true)));
}

QVector<TrackInfo> FileListPanel::selectedTracks() const
{
    QVector<TrackInfo> tracks;
    const QModelIndexList rows = m_view->selectionModel()->selectedRows();
    tracks.reserve(rows.size());
    for (const QModelIndex& proxyIndex : rows) {
        const QModelIndex source = m_proxy->mapToSource(proxyIndex);
        tracks.append(m_model->trackAt(source.row()));
    }
    return tracks;
}

QVector<TrackInfo> FileListPanel::allTracks() const
{
    // Respeta el filtro y el orden que ve el usuario.
    QVector<TrackInfo> tracks;
    const int rows = m_proxy->rowCount();
    tracks.reserve(rows);
    for (int row = 0; row < rows; ++row) {
        const QModelIndex source = m_proxy->mapToSource(m_proxy->index(row, 0));
        tracks.append(m_model->trackAt(source.row()));
    }
    return tracks;
}

void FileListPanel::onCurrentRowChanged()
{
    const QModelIndex current = m_view->currentIndex();
    if (!current.isValid())
        return;
    const QModelIndex source = m_proxy->mapToSource(current);
    emit selectionChanged(m_model->trackAt(source.row()));
}

void FileListPanel::showColumnMenu(const QPoint& pos)
{
    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));
    menu.addAction(Lang::tr("Columnas visibles"))->setEnabled(false);
    menu.addSeparator();

    for (int column = 0; column < FileTableModel::ColumnCount; ++column) {
        const QString name =
            m_model->headerData(column, Qt::Horizontal, Qt::DisplayRole).toString();

        QAction* action = menu.addAction(name);
        action->setCheckable(true);
        action->setChecked(!m_view->isColumnHidden(column));
        // "Nombre de archivo" no se puede ocultar: sin ella la lista queda
        // sin nada con lo que identificar la pista.
        action->setEnabled(column != FileTableModel::FileName);

        connect(action, &QAction::toggled, this, [this, column](bool visible) {
            m_view->setColumnHidden(column, !visible);
        });
    }

    menu.exec(m_view->header()->mapToGlobal(pos));
}

void FileListPanel::onDoubleClicked(const QModelIndex& index)
{
    if (!index.isValid())
        return;
    const QModelIndex source = m_proxy->mapToSource(index);
    emit trackActivated(m_model->trackAt(source.row()));
}

void FileListPanel::keyPressEvent(QKeyEvent* event)
{
    const QVector<TrackInfo> selection = selectedTracks();

    switch (event->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        if (!selection.isEmpty())
            emit playRequested(selection);
        return;
    case Qt::Key_Insert:
        if (!selection.isEmpty())
            emit enqueueRequested(selection);
        return;
    case Qt::Key_F4:
        if (!selection.isEmpty())
            emit propertiesRequested(selection.first());
        return;
    default:
        break;
    }
    QWidget::keyPressEvent(event);
}

void FileListPanel::showContextMenu(const QPoint& pos)
{
    const QVector<TrackInfo> selection = selectedTracks();
    if (selection.isEmpty())
        return;

    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));

    const QColor iconColor = Theme::TextDim;

    QAction* play = menu.addAction(Icons::icon(Icons::Play, iconColor),
                                   Lang::tr("Reproducir"));
    play->setShortcut(QKeySequence(Qt::Key_Return));

    QAction* playSelected = menu.addAction(Lang::tr("Reproducir seleccionado"));

    menu.addSeparator();

    QAction* addFiles = menu.addAction(Icons::icon(Icons::Plus, iconColor),
                                       Lang::tr("Agregar archivos"));
    addFiles->setShortcut(QKeySequence(Qt::Key_Insert));

    QAction* addToPlaylist = menu.addAction(Lang::tr("Agregar a la lista de reproduccion"));

    menu.addSeparator();

    QAction* properties = menu.addAction(Icons::icon(Icons::Info, iconColor),
                                         Lang::tr("Propiedades"));
    properties->setShortcut(QKeySequence(Qt::Key_F4));

    QAction* reveal = menu.addAction(Icons::icon(Icons::FolderOpen, iconColor),
                                     Lang::tr("Ubicacion de archivo"));
    reveal->setShortcut(QKeySequence(QStringLiteral("Alt+O")));

    QAction* reread = menu.addAction(Icons::icon(Icons::Refresh, iconColor),
                                     Lang::tr("Releer etiquetas de archivos seleccionados"));

    menu.addSeparator();

    QMenu* rating = menu.addMenu(Icons::icon(Icons::Star, iconColor),
                                 Lang::tr("Calificacion"));
    rating->setFont(Theme::uiFont(9));
    QAction* ratingActions[6];
    ratingActions[0] = rating->addAction(Lang::tr("Sin calificar"));
    for (int stars = 1; stars <= 5; ++stars)
        ratingActions[stars] = rating->addAction(QString(stars, QChar(0x2605)));

    menu.addSeparator();

    QAction* editTags = menu.addAction(Icons::icon(Icons::Tag, iconColor),
                                       Lang::tr("Etiquetas..."));

    menu.addSeparator();

    QAction* deleteFiles = menu.addAction(Icons::icon(Icons::Trash, iconColor),
                                          Lang::tr("Eliminar permanentemente los archivos seleccionados"));
    deleteFiles->setShortcut(QKeySequence(QStringLiteral("Shift+Del")));

    QAction* chosen = menu.exec(m_view->viewport()->mapToGlobal(pos));
    if (!chosen)
        return;

    if (chosen == play || chosen == playSelected) {
        emit playRequested(selection);
    } else if (chosen == addFiles || chosen == addToPlaylist) {
        emit enqueueRequested(selection);
    } else if (chosen == properties || chosen == editTags) {
        emit editTagsRequested(selection.first());
    } else if (chosen == reveal) {
        emit revealRequested(selection.first().path);
    } else if (chosen == reread) {
        emit rereadTagsRequested(selection);
    } else if (chosen == deleteFiles) {
        emit deleteFilesRequested(selection);
    } else {
        for (int stars = 0; stars <= 5; ++stars) {
            if (chosen == ratingActions[stars]) {
                emit ratingRequested(selection, stars);
                break;
            }
        }
    }
}
