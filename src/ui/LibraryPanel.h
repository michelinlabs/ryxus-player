#pragma once

#include <QStringList>
#include <QWidget>

class FlatButton;
class QLineEdit;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;

// Columna de carpetas, con el modelo de trabajo de VS Code: no se navega todo
// el disco, se "abren" carpetas concretas y cada una queda como raiz del
// arbol. Mientras no haya ninguna, el panel invita a abrir una.
class LibraryPanel : public QWidget {
    Q_OBJECT

public:
    explicit LibraryPanel(QWidget* parent = nullptr);

    QString currentFolder() const;
    void    selectFolder(const QString& path);

    QStringList openedFolders() const { return m_folders; }
    void        setOpenedFolders(const QStringList& folders);

public slots:
    void openFolderDialog();

signals:
    void folderSelected(const QString& path);
    void folderActivated(const QString& path);      // doble clic
    void folderPlayRequested(const QString& path);  // "Reproducir esta carpeta"
    void openedFoldersChanged(const QStringList& folders);

private slots:
    void onSelectionChanged();
    void onItemExpanded(QTreeWidgetItem* item);
    void onItemDoubleClicked(QTreeWidgetItem* item, int column);
    void onQuickSearch(const QString& text);
    void showTreeContextMenu(const QPoint& pos);

private:
    void buildUi();
    void rebuildRoots();
    void refreshEmptyState();
    void refreshDriveFilters();
    void applyDriveFilter(const QString& drive);

    QTreeWidgetItem* addRoot(const QString& path);
    void populate(QTreeWidgetItem* item);
    void expandBranch(QTreeWidgetItem* item, int& budget);
    static QString pathOf(QTreeWidgetItem* item);
    void filterItem(QTreeWidgetItem* item, const QString& needle);

    QStackedWidget* m_stack   = nullptr;
    QWidget*        m_empty   = nullptr;
    QTreeWidget*    m_tree    = nullptr;
    QLineEdit*      m_search  = nullptr;
    QWidget*        m_driveRow = nullptr;
    FlatButton*     m_openButton = nullptr;

    QList<FlatButton*> m_driveButtons;
    QStringList m_folders;      // carpetas abiertas, en orden
    QString     m_activeDrive;
};
