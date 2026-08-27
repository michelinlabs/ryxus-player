#include "ui/LibraryPanel.h"
#include "core/Lang.h"

#include "core/Settings.h"
#include "core/TrackInfo.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QDir>
#include <QFileDialog>
#include <QFileIconProvider>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QStackedWidget>
#include <QTreeWidget>
#include <QVBoxLayout>

namespace {

constexpr int kPathRole    = Qt::UserRole + 1;
constexpr int kLoadedRole  = Qt::UserRole + 2;
constexpr int kIsRootRole  = Qt::UserRole + 3;

} // namespace

LibraryPanel::LibraryPanel(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("libraryPanel"));
    buildUi();
    refreshEmptyState();
}

void LibraryPanel::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    // --- barra superior: abrir carpeta -------------------------------------
    auto* groupRow = new QWidget(this);
    groupRow->setObjectName(QStringLiteral("groupingBar"));
    groupRow->setFixedHeight(Theme::Metrics::HeaderHeight);
    auto* groupLayout = new QHBoxLayout(groupRow);
    groupLayout->setContentsMargins(10, 0, 6, 0);
    groupLayout->setSpacing(8);

    auto* groupIcon = new FlatButton(Icons::Menu, groupRow);
    groupIcon->setFixedDiameter(20);
    groupIcon->setGlyphSize(14);
    groupIcon->setStrokeWidth(1.3);
    groupIcon->setEnabled(false);
    groupLayout->addWidget(groupIcon);

    auto* groupLabel = new QLabel(Lang::tr("Carpetas"), groupRow);
    groupLabel->setFont(Theme::uiFont(9));
    groupLayout->addWidget(groupLabel, 1);

    m_openButton = new FlatButton(Icons::FolderOpen, groupRow);
    m_openButton->setFixedDiameter(22);
    m_openButton->setGlyphSize(15);
    m_openButton->setStrokeWidth(1.4);
    m_openButton->setToolTip(Lang::tr("Abrir carpeta (Ctrl+K)"));
    groupLayout->addWidget(m_openButton);
    connect(m_openButton, &FlatButton::clicked, this, &LibraryPanel::openFolderDialog);

    root->addWidget(groupRow);

    // --- filtros por unidad -------------------------------------------------
    m_driveRow = new QWidget(this);
    m_driveRow->setObjectName(QStringLiteral("driveRow"));
    m_driveRow->setFixedHeight(Theme::Metrics::FilterRowHeight);
    auto* driveLayout = new QHBoxLayout(m_driveRow);
    driveLayout->setContentsMargins(10, 2, 8, 2);
    driveLayout->setSpacing(4);
    driveLayout->addStretch(1);
    root->addWidget(m_driveRow);

    // --- contenido: arbol o invitacion --------------------------------------
    m_stack = new QStackedWidget(this);

    // Pagina 0: invitacion, como el "Open Folder" de VS Code.
    m_empty = new QWidget(m_stack);
    auto* emptyLayout = new QVBoxLayout(m_empty);
    emptyLayout->setContentsMargins(18, 26, 18, 18);
    emptyLayout->setSpacing(10);
    emptyLayout->addStretch(1);

    auto* emptyText = new QLabel(m_empty);
    emptyText->setObjectName(QStringLiteral("placeholderText"));
    emptyText->setWordWrap(true);
    emptyText->setAlignment(Qt::AlignCenter);
    emptyText->setFont(Theme::uiFont(9));
    emptyText->setText(Lang::tr(
        "No hay ninguna carpeta abierta.\n\n"
        "Abre la carpeta donde tengas tu musica y quedara\n"
        "aqui como raiz para explorarla."));
    emptyLayout->addWidget(emptyText);

    auto* openBig = new FlatButton(Lang::tr("Abrir carpeta"), m_empty);
    openBig->setIconId(Icons::FolderOpen);
    openBig->setGlyphSize(16);
    openBig->setFixedHeight(30);
    openBig->setFilledWhenChecked(false);
    openBig->setColors(Theme::AccentBright, Theme::Text, Theme::Text);
    emptyLayout->addWidget(openBig, 0, Qt::AlignHCenter);
    connect(openBig, &FlatButton::clicked, this, &LibraryPanel::openFolderDialog);

    emptyLayout->addStretch(2);
    m_stack->addWidget(m_empty);

    // Pagina 1: el arbol.
    m_tree = new QTreeWidget(m_stack);
    m_tree->setObjectName(QStringLiteral("libraryTree"));
    m_tree->setHeaderHidden(true);
    m_tree->setColumnCount(1);
    m_tree->setSelectionMode(QAbstractItemView::SingleSelection);
    m_tree->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_tree->setAnimated(false);
    m_tree->setIndentation(14);
    m_tree->setUniformRowHeights(true);
    m_tree->setExpandsOnDoubleClick(false);
    m_tree->setFrameShape(QFrame::NoFrame);
    m_tree->setContextMenuPolicy(Qt::CustomContextMenu);
    m_stack->addWidget(m_tree);

    root->addWidget(m_stack, 1);

    connect(m_tree, &QTreeWidget::itemSelectionChanged, this, &LibraryPanel::onSelectionChanged);
    connect(m_tree, &QTreeWidget::itemExpanded,        this, &LibraryPanel::onItemExpanded);
    connect(m_tree, &QTreeWidget::itemDoubleClicked,   this, &LibraryPanel::onItemDoubleClicked);
    connect(m_tree, &QTreeWidget::customContextMenuRequested,
            this, &LibraryPanel::showTreeContextMenu);

    // --- busqueda rapida ---------------------------------------------------
    auto* searchRow = new QWidget(this);
    searchRow->setObjectName(QStringLiteral("searchRow"));
    searchRow->setFixedHeight(Theme::Metrics::ToolRowHeight);
    auto* searchLayout = new QHBoxLayout(searchRow);
    searchLayout->setContentsMargins(10, 0, 6, 0);
    searchLayout->setSpacing(6);

    auto* searchIcon = new FlatButton(Icons::Search, searchRow);
    searchIcon->setFixedDiameter(20);
    searchIcon->setGlyphSize(14);
    searchIcon->setStrokeWidth(1.3);
    searchIcon->setEnabled(false);
    searchLayout->addWidget(searchIcon);

    m_search = new QLineEdit(searchRow);
    m_search->setObjectName(QStringLiteral("quickSearch"));
    m_search->setPlaceholderText(Lang::tr("Busqueda rapida"));
    m_search->setFont(Theme::uiFont(9));
    m_search->setFrame(false);
    m_search->setClearButtonEnabled(true);
    searchLayout->addWidget(m_search, 1);

    auto* addButton = new FlatButton(Icons::Plus, searchRow);
    addButton->setFixedDiameter(22);
    addButton->setGlyphSize(15);
    addButton->setStrokeWidth(1.4);
    addButton->setToolTip(Lang::tr("Abrir otra carpeta"));
    searchLayout->addWidget(addButton);
    connect(addButton, &FlatButton::clicked, this, &LibraryPanel::openFolderDialog);

    root->addWidget(searchRow);

    connect(m_search, &QLineEdit::textChanged, this, &LibraryPanel::onQuickSearch);
}

// ------------------------------------------------------------- carpetas raiz

void LibraryPanel::openFolderDialog()
{
    const QString start = m_folders.isEmpty() ? Settings::lastBrowsedFolder()
                                              : m_folders.last();
    const QString folder = QFileDialog::getExistingDirectory(
        this, Lang::tr("Abrir carpeta de musica"), start);

    if (folder.isEmpty())
        return;

    const QString clean = QDir::cleanPath(folder);
    if (!m_folders.contains(clean, Qt::CaseInsensitive)) {
        m_folders << clean;
        Settings::setLibraryFolders(m_folders);
        emit openedFoldersChanged(m_folders);
        rebuildRoots();
    }

    selectFolder(clean);
}

void LibraryPanel::setOpenedFolders(const QStringList& folders)
{
    m_folders.clear();
    for (const QString& folder : folders) {
        const QString clean = QDir::cleanPath(folder);
        if (QDir(clean).exists() && !m_folders.contains(clean, Qt::CaseInsensitive))
            m_folders << clean;
    }
    rebuildRoots();
}

void LibraryPanel::rebuildRoots()
{
    m_tree->clear();
    for (const QString& folder : m_folders)
        addRoot(folder);

    refreshDriveFilters();
    refreshEmptyState();

    if (m_tree->topLevelItemCount() == 1)
        m_tree->topLevelItem(0)->setExpanded(true);
}

void LibraryPanel::refreshEmptyState()
{
    m_stack->setCurrentIndex(m_folders.isEmpty() ? 0 : 1);
    m_driveRow->setVisible(!m_folders.isEmpty());
}

QTreeWidgetItem* LibraryPanel::addRoot(const QString& path)
{
    const QDir dir(path);
    auto* item = new QTreeWidgetItem(m_tree);

    // La raiz muestra la ruta completa, como en la referencia
    // ("C:\\Users\\pikac\\Downloads"); los hijos, solo su nombre.
    item->setText(0, QDir::toNativeSeparators(path));
    item->setToolTip(0, QDir::toNativeSeparators(path));
    item->setData(0, kPathRole, path);
    item->setData(0, kLoadedRole, false);
    item->setData(0, kIsRootRole, true);
    item->setIcon(0, Icons::icon(Icons::FolderOpen, Theme::AccentBright, 16));

    if (dir.exists())
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
    return item;
}

QString LibraryPanel::pathOf(QTreeWidgetItem* item)
{
    return item ? item->data(0, kPathRole).toString() : QString();
}

void LibraryPanel::populate(QTreeWidgetItem* item)
{
    if (!item || item->data(0, kLoadedRole).toBool())
        return;
    item->setData(0, kLoadedRole, true);

    const QDir dir(pathOf(item));
    if (!dir.exists())
        return;

    // Carga perezosa: solo al desplegar. Asi abrir una carpeta enorme no
    // bloquea la interfaz recorriendo todo el arbol de golpe.
    const QFileInfoList entries = dir.entryInfoList(
        QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable, QDir::Name | QDir::LocaleAware);

    for (const QFileInfo& entry : entries) {
        auto* child = new QTreeWidgetItem(item);
        child->setText(0, entry.fileName());
        child->setToolTip(0, QDir::toNativeSeparators(entry.absoluteFilePath()));
        child->setData(0, kPathRole, entry.absoluteFilePath());
        child->setData(0, kLoadedRole, false);
        child->setData(0, kIsRootRole, false);
        child->setIcon(0, Icons::icon(Icons::Folder, Theme::TextDim, 16));

        // Solo se marca como desplegable si de verdad tiene subcarpetas.
        const QDir sub(entry.absoluteFilePath());
        if (!sub.entryInfoList(QDir::Dirs | QDir::NoDotAndDotDot | QDir::Readable).isEmpty())
            child->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        else
            child->setChildIndicatorPolicy(QTreeWidgetItem::DontShowIndicator);
    }
}

void LibraryPanel::onItemExpanded(QTreeWidgetItem* item)
{
    populate(item);
    if (!item->data(0, kIsRootRole).toBool())
        item->setIcon(0, Icons::icon(Icons::FolderOpen, Theme::TextDim, 16));
}

void LibraryPanel::onItemDoubleClicked(QTreeWidgetItem* item, int)
{
    const QString path = pathOf(item);
    if (!path.isEmpty())
        emit folderActivated(path);
}

void LibraryPanel::onSelectionChanged()
{
    const QString folder = currentFolder();
    if (folder.isEmpty())
        return;
    Settings::setLastBrowsedFolder(folder);
    emit folderSelected(folder);
}

QString LibraryPanel::currentFolder() const
{
    return pathOf(m_tree->currentItem());
}

void LibraryPanel::selectFolder(const QString& path)
{
    if (path.isEmpty())
        return;

    const QString target = QDir::cleanPath(path);

    // Busca la raiz que contiene la ruta y va desplegando hasta llegar.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        const QString rootPath = QDir::cleanPath(pathOf(item));

        if (target.compare(rootPath, Qt::CaseInsensitive) == 0) {
            m_tree->setCurrentItem(item);
            m_tree->scrollToItem(item);
            return;
        }
        if (!target.startsWith(rootPath + QLatin1Char('/'), Qt::CaseInsensitive))
            continue;

        const QStringList parts = target.mid(rootPath.size() + 1).split(QLatin1Char('/'),
                                                                       Qt::SkipEmptyParts);
        QTreeWidgetItem* current = item;
        for (const QString& part : parts) {
            populate(current);
            current->setExpanded(true);

            QTreeWidgetItem* next = nullptr;
            for (int c = 0; c < current->childCount(); ++c) {
                if (current->child(c)->text(0).compare(part, Qt::CaseInsensitive) == 0) {
                    next = current->child(c);
                    break;
                }
            }
            if (!next)
                break;
            current = next;
        }

        m_tree->setCurrentItem(current);
        m_tree->scrollToItem(current, QAbstractItemView::PositionAtCenter);
        return;
    }
}

// ---------------------------------------------------------------- filtros

void LibraryPanel::refreshDriveFilters()
{
    auto* layout = qobject_cast<QHBoxLayout*>(m_driveRow->layout());
    if (!layout)
        return;

    for (FlatButton* button : std::as_const(m_driveButtons)) {
        layout->removeWidget(button);
        button->deleteLater();
    }
    m_driveButtons.clear();

    const auto makeFilterButton = [this, layout](const QString& text, const QString& drive) {
        auto* button = new FlatButton(text, m_driveRow);
        button->setFixedSize(24, 22);
        button->setFont(Theme::uiFont(9, QFont::DemiBold));
        button->setCheckable(true);
        button->setFilledWhenChecked(true);
        button->setColors(Theme::TextDim, Theme::Text, Theme::Text);
        button->setToolTip(drive.isEmpty() ? Lang::tr("Todas las carpetas") : drive);
        layout->insertWidget(int(m_driveButtons.size()), button);
        m_driveButtons.append(button);
        connect(button, &FlatButton::clicked, this, [this, drive]() { applyDriveFilter(drive); });
        return button;
    };

    makeFilterButton(QStringLiteral("*"), QString())->setChecked(true);

    // Una letra por unidad presente entre las carpetas abiertas, igual que en
    // la referencia ("* C D Z").
    QStringList letters;
    for (const QString& folder : m_folders) {
        if (folder.size() >= 2 && folder.at(1) == QLatin1Char(':')) {
            const QString letter = folder.left(1).toUpper();
            if (!letters.contains(letter))
                letters << letter;
        }
    }
    letters.sort();
    for (const QString& letter : letters)
        makeFilterButton(letter, letter);
}

void LibraryPanel::applyDriveFilter(const QString& drive)
{
    m_activeDrive = drive;

    for (FlatButton* button : std::as_const(m_driveButtons)) {
        const bool matches = drive.isEmpty() ? button->text() == QStringLiteral("*")
                                             : button->text() == drive;
        QSignalBlocker blocker(button);
        button->setChecked(matches);
    }

    for (int i = 0; i < m_tree->topLevelItemCount(); ++i) {
        QTreeWidgetItem* item = m_tree->topLevelItem(i);
        const QString path = pathOf(item);
        const bool visible = drive.isEmpty()
            || (path.size() >= 1 && path.left(1).compare(drive, Qt::CaseInsensitive) == 0);
        item->setHidden(!visible);
    }
}

void LibraryPanel::filterItem(QTreeWidgetItem* item, const QString& needle)
{
    bool anyVisibleChild = false;
    for (int i = 0; i < item->childCount(); ++i) {
        filterItem(item->child(i), needle);
        if (!item->child(i)->isHidden())
            anyVisibleChild = true;
    }

    const bool matches = needle.isEmpty()
                      || item->text(0).contains(needle, Qt::CaseInsensitive);
    item->setHidden(!matches && !anyVisibleChild);

    if (!needle.isEmpty() && anyVisibleChild)
        item->setExpanded(true);
}

void LibraryPanel::onQuickSearch(const QString& text)
{
    // Solo filtra lo ya cargado: forzar la carga de todo el arbol para buscar
    // recorreria el disco entero en cada pulsacion.
    for (int i = 0; i < m_tree->topLevelItemCount(); ++i)
        filterItem(m_tree->topLevelItem(i), text.trimmed());

    if (text.trimmed().isEmpty())
        applyDriveFilter(m_activeDrive);
}

void LibraryPanel::showTreeContextMenu(const QPoint& pos)
{
    QTreeWidgetItem* item = m_tree->itemAt(pos);
    if (!item)
        return;

    const QString path = pathOf(item);
    const bool isRoot = item->data(0, kIsRootRole).toBool();

    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));

    QAction* play = menu.addAction(Icons::icon(Icons::Play, Theme::TextDim),
                                   Lang::tr("Reproducir esta carpeta"));
    menu.addSeparator();
    QAction* refresh = menu.addAction(Icons::icon(Icons::Refresh, Theme::TextDim),
                                      Lang::tr("Actualizar"));
    QAction* close = menu.addAction(Icons::icon(Icons::Close, Theme::TextDim),
                                    Lang::tr("Cerrar carpeta"));
    close->setEnabled(isRoot);

    QAction* chosen = menu.exec(m_tree->viewport()->mapToGlobal(pos));
    if (chosen == play) {
        emit folderActivated(path);
    } else if (chosen == refresh) {
        const bool wasExpanded = item->isExpanded();
        item->takeChildren();
        item->setData(0, kLoadedRole, false);
        item->setChildIndicatorPolicy(QTreeWidgetItem::ShowIndicator);
        if (wasExpanded) {
            populate(item);
            item->setExpanded(true);
        }
    } else if (chosen == close) {
        m_folders.removeAll(pathOf(item));
        Settings::setLibraryFolders(m_folders);
        emit openedFoldersChanged(m_folders);
        rebuildRoots();
    }
}
