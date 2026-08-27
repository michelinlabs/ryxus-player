#include "ui/MainWindow.h"

#include "core/LibraryScanner.h"
#include "core/MetadataService.h"
#include "core/Lang.h"
#include "core/Settings.h"
#include "core/WaveformWorker.h"
#include "ui/BackgroundHost.h"
#include "ui/EqCurveEditor.h"
#include "ui/EqualizerPanel.h"
#include "ui/FileListPanel.h"
#include "ui/Icons.h"
#include "ui/LibraryPanel.h"
#include "ui/MetadataEditor.h"
#include "ui/TrackDetails.h"
#include "ui/NowPlayingPanel.h"
#include "ui/PlayerBar.h"
#include "ui/PlaylistPanel.h"
#include "ui/RibbonTabBar.h"
#include "ui/SettingsDialog.h"
#include "ui/SeekBar.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMouseEvent>
#include <QProcess>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWindow>

namespace {
constexpr int kResizeBorder = 6;   // franja sensible al redimensionado
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Roxas Player"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMinimumSize(1100, 640);

    m_engine = new AudioEngine(this);

    buildUi();
    wireSignals();
    installShortcuts();
    restoreSession();

    if (!m_engine->isInitialized()) {
        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                             Lang::tr("%1\n\nLa interfaz funciona, pero no habra sonido.")
                                 .arg(m_engine->lastError()));
    }

    qApp->installEventFilter(this);
}

MainWindow::~MainWindow()
{
    if (m_scanner)
        m_scanner->requestStop();

    for (QThread* thread : {m_waveformThread, m_scannerThread}) {
        if (!thread)
            continue;
        thread->quit();
        thread->wait(2000);
    }
}

// ---------------------------------------------------------------- construccion

void MainWindow::buildUi()
{
    // El widget central pinta el color base y la imagen de fondo; todo lo
    // demas se dibuja encima con alfa.
    m_background = new BackgroundHost(this);
    QWidget* central = m_background;
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new TitleBar(central);
    root->addWidget(m_titleBar);

    // --- cuerpo ------------------------------------------------------------
    //
    // La columna izquierda llega hasta la barra inferior: asi la ficha de
    // detalles dispone de todo el alto hasta los botones de transporte. El
    // ecualizador se abre solo bajo las otras dos columnas, empezando justo a
    // la derecha del panel izquierdo, en vez de cruzar la ventana entera.
    auto* body = new QWidget(central);
    auto* bodyLayout = new QHBoxLayout(body);
    bodyLayout->setContentsMargins(0, 0, 0, 0);
    bodyLayout->setSpacing(0);

    m_nowPlaying = new NowPlayingPanel(m_engine, body);
    bodyLayout->addWidget(m_nowPlaying);

    auto* rightSide = new QWidget(body);
    auto* rightLayout = new QVBoxLayout(rightSide);
    rightLayout->setContentsMargins(0, 0, 0, 0);
    rightLayout->setSpacing(0);

    auto* panels = new QWidget(rightSide);
    auto* panelsLayout = new QHBoxLayout(panels);
    panelsLayout->setContentsMargins(0, 0, 0, 0);
    panelsLayout->setSpacing(0);

    auto* centerColumn = new QWidget(panels);
    auto* centerLayout = new QVBoxLayout(centerColumn);
    centerLayout->setContentsMargins(0, 0, 0, 0);
    centerLayout->setSpacing(0);

    m_centerTabs = new RibbonTabBar(centerColumn);
    m_centerTabs->setTabs({Lang::tr("Archivos locales"),
                           Lang::tr("Etiquetas"),
                           Lang::tr("Marcadores"),
                           Lang::tr("Mis Nubes"),
                           Lang::tr("Podcasts")});
    centerLayout->addWidget(m_centerTabs);

    m_centerStack = new QStackedWidget(centerColumn);
    centerLayout->addWidget(m_centerStack, 1);
    buildCenterColumn();

    panelsLayout->addWidget(centerColumn, 1);

    m_playlists = new PlaylistPanel(panels);
    m_playlists->setFixedWidth(Theme::Metrics::RightPanelWidth);
    panelsLayout->addWidget(m_playlists);

    rightLayout->addWidget(panels, 1);

    // --- ecualizador (oculto por defecto) ---------------------------------
    m_equalizer = new EqualizerPanel(&m_engine->equalizer(), m_engine, rightSide);
    m_equalizer->hide();
    rightLayout->addWidget(m_equalizer);

    bodyLayout->addWidget(rightSide, 1);
    root->addWidget(body, 1);

    // --- barra inferior ----------------------------------------------------
    m_playerBar = new PlayerBar(central);
    root->addWidget(m_playerBar);

    m_tick = new QTimer(this);
    m_tick->setInterval(40);
    connect(m_tick, &QTimer::timeout, this, &MainWindow::onTick);
    m_tick->start();
}

void MainWindow::buildCenterColumn()
{
    // Pestana 0: arbol de carpetas + tabla de archivos.
    m_centerSplitter = new QSplitter(Qt::Horizontal, m_centerStack);
    m_centerSplitter->setObjectName(QStringLiteral("centerSplitter"));
    m_centerSplitter->setHandleWidth(1);
    m_centerSplitter->setChildrenCollapsible(false);

    m_library = new LibraryPanel(m_centerSplitter);
    m_fileList = new FileListPanel(m_centerSplitter);

    m_centerSplitter->addWidget(m_library);
    m_centerSplitter->addWidget(m_fileList);
    m_centerSplitter->setStretchFactor(0, 0);
    m_centerSplitter->setStretchFactor(1, 1);
    m_centerSplitter->setSizes({Theme::Metrics::TreePanelWidth, 700});

    m_centerStack->addWidget(m_centerSplitter);

    // Pestana 1: editor de etiquetas a pagina completa.
    m_tagEditor = new MetadataEditor(m_centerStack);
    m_centerStack->addWidget(m_tagEditor);

    // Pestana 2: marcadores (carpetas anadidas a la biblioteca).
    auto* bookmarks = new QWidget(m_centerStack);
    auto* bookmarksLayout = new QVBoxLayout(bookmarks);
    bookmarksLayout->setContentsMargins(18, 18, 18, 18);
    auto* bookmarksLabel = new QLabel(bookmarks);
    bookmarksLabel->setObjectName(QStringLiteral("placeholderText"));
    bookmarksLabel->setWordWrap(true);
    bookmarksLabel->setAlignment(Qt::AlignCenter);
    bookmarksLabel->setText(Lang::tr(
        "Carpetas marcadas\n\nUsa el boton + del arbol de carpetas para anadir\n"
        "las rutas que quieras tener siempre a mano."));
    bookmarksLayout->addWidget(bookmarksLabel);
    m_centerStack->addWidget(bookmarks);

    // Pestanas 3 y 4: presentes en el diseno, sin servicio detras.
    for (const QString& note : {Lang::tr("Mis Nubes"), Lang::tr("Podcasts")}) {
        auto* page = new QWidget(m_centerStack);
        auto* layout = new QVBoxLayout(page);
        auto* label = new QLabel(page);
        label->setObjectName(QStringLiteral("placeholderText"));
        label->setAlignment(Qt::AlignCenter);
        label->setText(Lang::tr("%1\n\nSeccion reservada: requiere conectar un servicio externo.")
                           .arg(note));
        layout->addWidget(label);
        m_centerStack->addWidget(page);
    }
}

void MainWindow::wireSignals()
{
    // --- barra de titulo ---------------------------------------------------
    connect(m_titleBar, &TitleBar::minimizeRequested, this, &MainWindow::showMinimized);
    connect(m_titleBar, &TitleBar::maximizeRequested, this, &MainWindow::toggleMaximized);
    connect(m_titleBar, &TitleBar::closeRequested,    this, &MainWindow::close);
    connect(m_titleBar, &TitleBar::menuRequested,     this, &MainWindow::showAppMenu);
    connect(m_titleBar, &TitleBar::settingsRequested, this, &MainWindow::openSettings);

    connect(m_centerTabs, &RibbonTabBar::currentChanged,
            this, [this](int index) {
                m_centerStack->setCurrentIndex(index);

                // Entrar a "Etiquetas" por la pestana debe cargar sola la
                // pista en curso; obligar a pasar por el boton seria un paso
                // de mas que no aporta nada.
                if (index != 1 || m_tagEditor->track().isValid())
                    return;
                if (m_detailsTrack.isValid())
                    openTagEditor(m_detailsTrack);
                else if (m_currentTrack.isValid())
                    openTagEditor(m_currentTrack);
            });

    // --- biblioteca --------------------------------------------------------
    connect(m_library, &LibraryPanel::folderSelected,  this, &MainWindow::onFolderSelected);
    connect(m_library, &LibraryPanel::folderActivated, this, &MainWindow::onFolderActivated);

    m_scannerThread = new QThread(this);
    m_scanner = new LibraryScanner;
    m_scanner->moveToThread(m_scannerThread);
    connect(m_scannerThread, &QThread::finished, m_scanner, &QObject::deleteLater);
    connect(m_scanner, &LibraryScanner::batchReady,   this, &MainWindow::onScanBatch);
    connect(m_scanner, &LibraryScanner::scanFinished, this, &MainWindow::onScanFinished);
    m_scannerThread->start();

    // --- analisis de forma de onda ----------------------------------------
    m_waveformThread = new QThread(this);
    m_waveformWorker = new WaveformWorker;
    m_waveformWorker->moveToThread(m_waveformThread);
    connect(m_waveformThread, &QThread::finished, m_waveformWorker, &QObject::deleteLater);
    connect(m_waveformWorker, &WaveformWorker::ready, this,
            [this](const QString& path, const QVector<float>& peaks) {
                if (path == m_currentTrack.path)
                    m_playerBar->seekBar()->setPeaks(peaks);
            });
    m_waveformThread->start();

    // --- lista central -----------------------------------------------------
    connect(m_fileList, &FileListPanel::trackActivated, this, [this](const TrackInfo& info) {
        auto* playlist = m_playlists->currentPlaylist();
        if (!playlist)
            return;
        int row = playlist->indexOfPath(info.path);
        if (row < 0) {
            playlist->appendTracks({info});
            row = playlist->rowCount() - 1;
        }
        playPlaylistRow(row);
    });

    connect(m_fileList, &FileListPanel::playRequested, this,
            [this](const QVector<TrackInfo>& tracks) {
                auto* playlist = m_playlists->currentPlaylist();
                if (!playlist || tracks.isEmpty())
                    return;
                const int firstRow = playlist->rowCount();
                playlist->appendTracks(tracks);
                playPlaylistRow(firstRow);
            });

    connect(m_fileList, &FileListPanel::enqueueRequested,
            m_playlists, &PlaylistPanel::enqueue);
    connect(m_fileList, &FileListPanel::propertiesRequested,
            this, &MainWindow::showProperties);
    connect(m_fileList, &FileListPanel::selectionChanged,
            this, &MainWindow::setDetailsTrack);
    connect(m_fileList, &FileListPanel::editTagsRequested,
            this, &MainWindow::openTagEditor);
    connect(m_fileList, &FileListPanel::revealRequested,
            this, &MainWindow::revealInExplorer);
    connect(m_fileList, &FileListPanel::rereadTagsRequested,
            this, &MainWindow::onRereadTags);
    connect(m_fileList, &FileListPanel::ratingRequested,
            this, &MainWindow::onBulkRating);
    connect(m_fileList, &FileListPanel::deleteFilesRequested,
            this, &MainWindow::deleteFilesPermanently);

    // --- listas de reproduccion -------------------------------------------
    connect(m_playlists, &PlaylistPanel::trackActivated, this, &MainWindow::playPlaylistRow);
    connect(m_playlists, &PlaylistPanel::propertiesRequested, this, &MainWindow::showProperties);
    connect(m_playlists, &PlaylistPanel::revealRequested, this, &MainWindow::revealInExplorer);

    // --- panel de reproduccion --------------------------------------------
    connect(m_nowPlaying, &NowPlayingPanel::ratingChanged, this, &MainWindow::onRatingChanged);
    connect(m_nowPlaying, &NowPlayingPanel::editTagsRequested,
            this, &MainWindow::openTagEditorForCurrent);

    connect(m_tagEditor, &MetadataEditor::saveRequested, this, &MainWindow::onSaveMetadata);
    connect(m_tagEditor, &MetadataEditor::revealRequested, this, &MainWindow::revealInExplorer);
    connect(m_tagEditor, &MetadataEditor::ratingChanged, this, [this](int rating) {
        TrackInfo edited = m_tagEditor->track();
        edited.rating = rating;
        if (MetadataService::write(edited))
            refreshTrackEverywhere(edited);
    });
    connect(m_tagEditor, &MetadataEditor::coverChangeRequested,
            this, &MainWindow::onCoverChangeRequested);
    connect(m_tagEditor, &MetadataEditor::coverRemoveRequested,
            this, &MainWindow::onCoverRemoveRequested);
    connect(m_tagEditor, &MetadataEditor::coverExportRequested,
            this, &MainWindow::onCoverExportRequested);
    connect(m_nowPlaying, &NowPlayingPanel::coverChangeRequested,
            this, &MainWindow::onCoverChangeRequested);
    connect(m_nowPlaying, &NowPlayingPanel::coverRemoveRequested,
            this, &MainWindow::onCoverRemoveRequested);
    connect(m_nowPlaying, &NowPlayingPanel::coverExportRequested,
            this, &MainWindow::onCoverExportRequested);

    // --- barra inferior ----------------------------------------------------
    connect(m_playerBar, &PlayerBar::playClicked,  this, &MainWindow::togglePlayPause);
    connect(m_playerBar, &PlayerBar::pauseClicked, this, [this]() { m_engine->pause(); });
    connect(m_playerBar, &PlayerBar::stopClicked,      this, &MainWindow::stopPlayback);
    connect(m_playerBar, &PlayerBar::nextClicked,      this, &MainWindow::playNext);
    connect(m_playerBar, &PlayerBar::previousClicked,  this, &MainWindow::playPrevious);
    connect(m_playerBar, &PlayerBar::repeatCycled,     this, &MainWindow::cycleRepeatMode);

    connect(m_playerBar, &PlayerBar::shuffleToggled, this, [this](bool enabled) {
        m_shuffle = enabled;
        Settings::setShuffle(enabled);
        if (enabled && m_playlists->currentPlaylist())
            m_playlists->currentPlaylist()->reshuffle();
    });

    connect(m_playerBar, &PlayerBar::volumeChanged, this, [this](float value) {
        m_engine->setVolume(value);
        m_engine->setMuted(false);
        m_playerBar->setMuted(false);
        Settings::setVolume(value);
    });

    connect(m_playerBar, &PlayerBar::muteToggled, this, [this]() {
        const bool muted = !m_engine->isMuted();
        m_engine->setMuted(muted);
        m_playerBar->setMuted(muted);
        Settings::setMuted(muted);
    });

    connect(m_playerBar, &PlayerBar::equalizerPanelToggled,
            this, &MainWindow::toggleEqualizerPanel);

    connect(m_playerBar, &PlayerBar::timeDisplayToggled, this, [this]() {
        SeekBar* seek = m_playerBar->seekBar();
        seek->setShowRemaining(!seek->showsRemaining());
    });

    connect(m_playerBar, &PlayerBar::abLoopClicked, this, [this]() {
        // Primer clic marca A, el segundo marca B, el tercero limpia.
        if (m_loopStartMs < 0) {
            m_loopStartMs = m_engine->positionMs();
            m_playerBar->setAbLoopActive(true);
        } else if (m_loopEndMs < 0) {
            m_loopEndMs = m_engine->positionMs();
            if (m_loopEndMs <= m_loopStartMs)
                std::swap(m_loopStartMs, m_loopEndMs);
        } else {
            m_loopStartMs = m_loopEndMs = -1;
            m_playerBar->setAbLoopActive(false);
        }
    });

    connect(m_playerBar->seekBar(), &SeekBar::seekRequested, this, [this](qint64 ms) {
        m_engine->seekMs(ms);
    });

    // --- ecualizador -------------------------------------------------------
    connect(m_equalizer, &EqualizerPanel::closeRequested, this, [this]() {
        m_playerBar->setEqualizerPanelOpen(false);
        toggleEqualizerPanel(false);
    });
    connect(m_equalizer, &EqualizerPanel::enabledChanged, this, [this](bool enabled) {
        m_playerBar->setEqualizerActive(enabled);
    });

    // --- motor -------------------------------------------------------------
    connect(m_engine, &AudioEngine::stateChanged, this, &MainWindow::onEngineStateChanged);
}

void MainWindow::installShortcuts()
{
    const auto bind = [this](const QKeySequence& keys, auto slot) {
        auto* shortcut = new QShortcut(keys, this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, slot);
    };

    bind(QKeySequence(Qt::Key_Space),                  &MainWindow::togglePlayPause);
    bind(QKeySequence(Qt::CTRL | Qt::Key_Right),       &MainWindow::playNext);
    bind(QKeySequence(Qt::CTRL | Qt::Key_Left),        &MainWindow::playPrevious);
    bind(QKeySequence(Qt::CTRL | Qt::Key_O),           &MainWindow::openFilesDialog);
    bind(QKeySequence(Qt::CTRL | Qt::Key_K), [this]() { m_library->openFolderDialog(); });
    bind(QKeySequence(Qt::Key_F4), &MainWindow::openTagEditorForCurrent);
    bind(QKeySequence(Qt::CTRL | Qt::Key_E), [this]() {
        const bool open = !m_equalizer->isVisible();
        m_playerBar->setEqualizerPanelOpen(open);
        toggleEqualizerPanel(open);
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_Up), [this]() {
        const float value = qMin(1.0f, m_engine->volume() + 0.05f);
        m_engine->setVolume(value);
        m_playerBar->setVolume(value);
        Settings::setVolume(value);
    });
    bind(QKeySequence(Qt::CTRL | Qt::Key_Down), [this]() {
        const float value = qMax(0.0f, m_engine->volume() - 0.05f);
        m_engine->setVolume(value);
        m_playerBar->setVolume(value);
        Settings::setVolume(value);
    });
}

// ------------------------------------------------------------------- sesion

void MainWindow::restoreSession()
{
    const QByteArray geometry = Settings::windowGeometry();
    if (!geometry.isEmpty())
        restoreGeometry(geometry);
    else
        resize(1600, 860);

    const QByteArray splitter = Settings::splitterState();
    if (!splitter.isEmpty())
        m_centerSplitter->restoreState(splitter);

    const float volume = Settings::volume();
    m_engine->setVolume(volume);
    m_playerBar->setVolume(volume);

    const bool muted = Settings::muted();
    m_engine->setMuted(muted);
    m_playerBar->setMuted(muted);

    m_shuffle = Settings::shuffle();
    m_playerBar->setShuffle(m_shuffle);

    m_repeat = static_cast<PlaylistModel::RepeatMode>(qBound(0, Settings::repeatMode(), 2));
    m_playerBar->setRepeatMode(int(m_repeat));

    m_playerBar->setEqualizerActive(Settings::eqEnabled());

    const bool eqVisible = Settings::eqPanelVisible();
    m_playerBar->setEqualizerPanelOpen(eqVisible);
    toggleEqualizerPanel(eqVisible);

    m_library->setOpenedFolders(Settings::libraryFolders());

    applyBackgroundSettings();

    const auto saved = Settings::playlists();
    if (!saved.isEmpty())
        m_playlists->restore(saved, Settings::activePlaylist());

    // Al seleccionar en el arbol se dispara folderSelected, que ya lanza el
    // escaneo: no hay que llamarlo a mano.
    const QString lastFolder = Settings::lastBrowsedFolder();
    if (!lastFolder.isEmpty() && QDir(lastFolder).exists())
        m_library->selectFolder(lastFolder);

    m_nowPlaying->clearTrack();

    // El foco inicial va a la lista de archivos, no a un campo de edicion.
    m_fileList->setFocus(Qt::OtherFocusReason);
}

void MainWindow::saveSession()
{
    Settings::setWindowGeometry(saveGeometry());
    Settings::setSplitterState(m_centerSplitter->saveState());
    Settings::setEqPanelVisible(m_equalizer->isVisible());
    Settings::setRepeatMode(int(m_repeat));
    Settings::setShuffle(m_shuffle);
    Settings::setPlaylists(m_playlists->serialize());
    Settings::setActivePlaylist(m_playlists->currentPlaylistIndex());
    m_equalizer->saveToSettings();
    Settings::store().sync();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    saveSession();
    m_tick->stop();
    m_engine->stop();
    QMainWindow::closeEvent(event);
}

// -------------------------------------------------------------- reproduccion

void MainWindow::playPlaylistRow(int row)
{
    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist || row < 0 || row >= playlist->rowCount())
        return;

    const TrackInfo& stored = playlist->trackAt(row);

    if (!m_engine->open(stored.path)) {
        QMessageBox::warning(this, QStringLiteral("Roxas Player"), m_engine->lastError());
        return;
    }

    playlist->setCurrentIndex(row);
    m_playlists->selectRow(row);

    // Se relee con caratula: en la lista se guardan sin ella por rendimiento.
    TrackInfo full = MetadataService::read(stored.path, true);
    if (!full.isValid())
        full = stored;
    full.rating  = stored.rating;
    full.checked = stored.checked;

    loadTrackIntoUi(full);
    m_engine->play();
}

void MainWindow::loadTrackIntoUi(const TrackInfo& info)
{
    m_currentTrack = info;
    m_loopStartMs = m_loopEndMs = -1;
    m_playerBar->setAbLoopActive(false);

    m_nowPlaying->setPlayingTrack(info);
    setDetailsTrack(info);

    SeekBar* seek = m_playerBar->seekBar();
    seek->clearPeaks();
    seek->setDurationMs(m_engine->durationMs());
    seek->setPositionMs(0);

    setWindowTitle(QStringLiteral("%1 - Roxas Player").arg(info.displayName()));

    // El analisis de la onda ocurre en su propio hilo.
    QMetaObject::invokeMethod(m_waveformWorker, "analyze", Qt::QueuedConnection,
                              Q_ARG(QString, info.path),
                              Q_ARG(int, WaveformWorker::kDefaultBuckets));
}

void MainWindow::playNext()
{
    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist)
        return;
    const int next = playlist->nextIndex(m_shuffle, m_repeat);
    if (next >= 0)
        playPlaylistRow(next);
    else
        stopPlayback();
}

void MainWindow::playPrevious()
{
    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist)
        return;

    // Como en cualquier reproductor: si ya avanzo, "anterior" reinicia.
    if (m_engine->positionMs() > 3000) {
        m_engine->seekMs(0);
        return;
    }

    const int previous = playlist->previousIndex(m_shuffle);
    if (previous >= 0)
        playPlaylistRow(previous);
}

void MainWindow::togglePlayPause()
{
    if (m_engine->currentPath().isEmpty()) {
        auto* playlist = m_playlists->currentPlaylist();
        if (playlist && playlist->rowCount() > 0)
            playPlaylistRow(qMax(0, playlist->currentIndex()));
        return;
    }
    m_engine->togglePlayPause();
}

void MainWindow::stopPlayback()
{
    m_engine->stop();
    m_playerBar->seekBar()->setPositionMs(0);
}

void MainWindow::onEngineStateChanged(AudioEngine::State state)
{
    const bool playing = state == AudioEngine::State::Playing;
    m_playerBar->setPlaying(playing);
    m_nowPlaying->setPlaying(playing);
}

void MainWindow::onTick()
{
    const qint64 position = m_engine->positionMs();
    m_playerBar->seekBar()->setPositionMs(position);

    // Bucle A-B.
    if (m_loopStartMs >= 0 && m_loopEndMs > m_loopStartMs && position >= m_loopEndMs)
        m_engine->seekMs(m_loopStartMs);

    if (m_engine->takeFinishedFlag()) {
        if (m_repeat == PlaylistModel::RepeatMode::One) {
            m_engine->seekMs(0);
            m_engine->play();
        } else {
            playNext();
        }
    }

    const bool playing = m_engine->isPlaying();
    m_playerBar->setPlaying(playing);
    m_nowPlaying->setPlaying(playing);
    m_equalizer->setAnalyzerActive(m_equalizer->isVisible() && playing);
}

void MainWindow::cycleRepeatMode()
{
    m_repeat = static_cast<PlaylistModel::RepeatMode>((int(m_repeat) + 1) % 3);
    m_playerBar->setRepeatMode(int(m_repeat));
    Settings::setRepeatMode(int(m_repeat));
}

// ----------------------------------------------------------------- biblioteca

void MainWindow::onFolderSelected(const QString& folder)
{
    if (folder.isEmpty())
        return;

    m_scanner->requestStop();
    m_fileList->beginScan(folder);

    ++m_scanRequestId;
    QMetaObject::invokeMethod(m_scanner, "scanFolder", Qt::QueuedConnection,
                              Q_ARG(QString, folder),
                              Q_ARG(bool, false),
                              Q_ARG(quint64, m_scanRequestId));
}

void MainWindow::onFolderActivated(const QString& folder)
{
    onFolderSelected(folder);
    // Las pistas llegan por lotes; se encolan segun van apareciendo.
}

void MainWindow::onScanBatch(const QVector<TrackInfo>& tracks, quint64 requestId)
{
    if (requestId != m_scanRequestId)
        return;
    m_fileList->appendTracks(tracks);
}

void MainWindow::onScanFinished(const QString&, int total, quint64 requestId)
{
    if (requestId != m_scanRequestId)
        return;
    m_fileList->endScan(total);
}

// ------------------------------------------------------------------ metadatos

void MainWindow::onSaveMetadata(const TrackInfo& info)
{
    QString error;
    if (!MetadataService::write(info, &error)) {
        QMessageBox::warning(this, Lang::tr("No se pudo guardar"),
                             QStringLiteral("%1\n\n%2").arg(info.fileName, error));
        return;
    }

    TrackInfo reloaded = MetadataService::read(info.path, true);
    if (!reloaded.isValid())
        reloaded = info;

    refreshTrackEverywhere(reloaded);
    m_tagEditor->setTrack(reloaded);
    setDetailsTrack(reloaded);

    // Si lo editado es justo lo que suena, se actualiza tambien la cabecera.
    if (reloaded.path.compare(m_currentTrack.path, Qt::CaseInsensitive) == 0) {
        m_currentTrack = reloaded;
        m_nowPlaying->setPlayingTrack(reloaded);
        setWindowTitle(QStringLiteral("%1 - Roxas Player").arg(reloaded.displayName()));
    }
}

void MainWindow::onRatingChanged(int rating)
{
    if (!m_currentTrack.isValid())
        return;

    m_currentTrack.rating = rating;
    MetadataService::write(m_currentTrack);
    refreshTrackEverywhere(m_currentTrack);
}

void MainWindow::refreshTrackEverywhere(const TrackInfo& info)
{
    m_fileList->refreshTrack(info);

    for (int i = 0; i < m_playlists->playlistCount(); ++i) {
        PlaylistModel* playlist = m_playlists->playlistAt(i);
        const int row = playlist->indexOfPath(info.path);
        if (row >= 0)
            playlist->updateTrack(row, info);
    }
}

void MainWindow::onCoverChangeRequested()
{
    TrackInfo target = m_tagEditor->track();
    if (!target.isValid())
        target = m_currentTrack;
    if (!target.isValid())
        return;

    const QString file = QFileDialog::getOpenFileName(
        this, Lang::tr("Elegir caratula"),
        QFileInfo(target.path).absolutePath(),
        Lang::tr("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp)"));

    if (file.isEmpty())
        return;

    QImage image(file);
    if (image.isNull()) {
        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                             Lang::tr("No se pudo leer la imagen."));
        return;
    }

    QString error;
    if (!MetadataService::writeCover(target.path, image, &error)) {
        QMessageBox::warning(this, Lang::tr("No se pudo guardar la caratula"), error);
        return;
    }

    target.cover = image;
    m_tagEditor->setTrack(target);
    if (target.path.compare(m_currentTrack.path, Qt::CaseInsensitive) == 0) {
        m_currentTrack.cover = image;
        m_nowPlaying->setPlayingTrack(m_currentTrack);
    }
}

void MainWindow::onCoverRemoveRequested()
{
    TrackInfo target = m_tagEditor->track();
    if (!target.isValid())
        target = m_currentTrack;
    if (!target.isValid())
        return;

    QString error;
    if (!MetadataService::writeCover(target.path, QImage(), &error)) {
        QMessageBox::warning(this, Lang::tr("No se pudo quitar la caratula"), error);
        return;
    }

    target.cover = QImage();
    m_tagEditor->setTrack(target);
    if (target.path.compare(m_currentTrack.path, Qt::CaseInsensitive) == 0) {
        m_currentTrack.cover = QImage();
        m_nowPlaying->setPlayingTrack(m_currentTrack);
    }
}

void MainWindow::onCoverExportRequested()
{
    TrackInfo target = m_tagEditor->track();
    if (!target.isValid() || target.cover.isNull())
        target = m_currentTrack;
    if (target.cover.isNull())
        return;

    const QString suggested = QDir(QFileInfo(target.path).absolutePath())
                                  .filePath(QStringLiteral("cover.jpg"));
    const QString file = QFileDialog::getSaveFileName(
        this, Lang::tr("Exportar caratula"), suggested,
        QStringLiteral("JPEG (*.jpg);;PNG (*.png)"));

    if (file.isEmpty())
        return;
    if (!target.cover.save(file))
        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                             Lang::tr("No se pudo escribir el archivo."));
}

void MainWindow::onRereadTags(const QVector<TrackInfo>& tracks)
{
    for (const TrackInfo& track : tracks) {
        const TrackInfo reloaded = MetadataService::read(track.path, false);
        if (reloaded.isValid())
            refreshTrackEverywhere(reloaded);
    }
}

void MainWindow::onBulkRating(const QVector<TrackInfo>& tracks, int rating)
{
    for (TrackInfo track : tracks) {
        track.rating = qBound(0, rating, 5);
        if (MetadataService::write(track))
            refreshTrackEverywhere(track);
    }

    if (m_currentTrack.isValid()) {
        for (const TrackInfo& track : tracks) {
            if (track.path.compare(m_currentTrack.path, Qt::CaseInsensitive) != 0)
                continue;
            m_currentTrack.rating = qBound(0, rating, 5);
            m_nowPlaying->setPlayingTrack(m_currentTrack);
            break;
        }
    }
}

void MainWindow::showProperties(const TrackInfo& info)
{
    openTagEditor(info);
}

void MainWindow::setDetailsTrack(const TrackInfo& info)
{
    m_detailsTrack = info;
    m_nowPlaying->setDetailsTrack(info);
}

void MainWindow::openTagEditor(const TrackInfo& info)
{
    if (!info.isValid())
        return;

    // Se relee con caratula: los listados guardan las pistas sin ella.
    const TrackInfo full = MetadataService::read(info.path, true);
    const TrackInfo target = full.isValid() ? full : info;

    setDetailsTrack(target);
    m_tagEditor->setTrack(target);

    // Pestana 1 = "Etiquetas".
    m_centerTabs->setCurrentIndex(1);
    m_centerStack->setCurrentIndex(1);
}

void MainWindow::openTagEditorForCurrent()
{
    // Prioriza lo que el usuario tiene seleccionado; si no hay nada, la pista
    // que esta sonando.
    if (m_detailsTrack.isValid())
        openTagEditor(m_detailsTrack);
    else if (m_currentTrack.isValid())
        openTagEditor(m_currentTrack);
}

void MainWindow::revealInExplorer(const QString& path)
{
    const QFileInfo fi(path);
    if (!fi.exists())
        return;

    // /select deja el archivo resaltado dentro de su carpeta.
    QProcess::startDetached(QStringLiteral("explorer.exe"),
                            {QStringLiteral("/select,") + QDir::toNativeSeparators(path)});
}

void MainWindow::deleteFilesPermanently(const QVector<TrackInfo>& tracks)
{
    if (tracks.isEmpty())
        return;

    const auto answer = QMessageBox::warning(
        this, Lang::tr("Eliminar archivos"),
        Lang::tr("Se eliminaran %1 archivo(s) del disco de forma permanente.\n"
                 "Esta accion no se puede deshacer.")
            .arg(tracks.size()),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if (answer != QMessageBox::Yes)
        return;

    QStringList failures;
    for (const TrackInfo& track : tracks) {
        if (track.path == m_currentTrack.path)
            m_engine->close();
        if (!QFile::remove(track.path))
            failures << track.fileName;
    }

    if (!failures.isEmpty()) {
        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                             Lang::tr("No se pudieron eliminar:\n%1")
                                 .arg(failures.join(QLatin1Char('\n'))));
    }

    onFolderSelected(m_library->currentFolder());
}

void MainWindow::openPaths(const QStringList& paths)
{
    QStringList files;
    for (const QString& path : paths) {
        const QFileInfo fi(path);
        if (fi.isDir()) {
            // Una carpeta pasada por linea de comandos se abre como raiz.
            QStringList opened = m_library->openedFolders();
            const QString clean = QDir::cleanPath(fi.absoluteFilePath());
            if (!opened.contains(clean, Qt::CaseInsensitive)) {
                opened << clean;
                Settings::setLibraryFolders(opened);
                m_library->setOpenedFolders(opened);
            }
            m_library->selectFolder(clean);
            QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::Readable);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (TrackInfo::isSupported(candidate))
                    files << candidate;
            }
        } else if (fi.isFile() && TrackInfo::isSupported(path)) {
            files << fi.absoluteFilePath();
        }
    }

    if (files.isEmpty())
        return;

    files.sort(Qt::CaseInsensitive);

    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist)
        return;

    const int firstRow = playlist->rowCount();
    playlist->appendFiles(files);
    playPlaylistRow(firstRow);
}

void MainWindow::openFilesDialog()
{
    const QStringList files = QFileDialog::getOpenFileNames(
        this, QStringLiteral("Abrir archivos"),
        Settings::lastBrowsedFolder(), TrackInfo::supportedFilter());

    if (files.isEmpty())
        return;

    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist)
        return;

    const int firstRow = playlist->rowCount();
    playlist->appendFiles(files);
    playPlaylistRow(firstRow);
}

// ---------------------------------------------------------------------- fondo

void MainWindow::applyBackgroundSettings()
{
    const QString path = Settings::backgroundImage();
    if (!path.isEmpty() && !m_background->setImagePath(path)) {
        // La imagen guardada ya no existe o no se puede leer: se olvida.
        Settings::setBackgroundImage(QString());
    }

    m_background->setMode(static_cast<BackgroundHost::Mode>(
        qBound(0, Settings::backgroundMode(), 3)));
    m_background->setDarkening(Settings::backgroundDarkening());

    // Sin imagen no tiene sentido volver translucidos los paneles: solo
    // dejarian ver el color base y la interfaz perderia contraste.
    const float transparency = m_background->hasImage()
        ? Settings::backgroundTransparency() / 100.0f
        : 0.0f;

    Theme::setPanelTransparency(transparency);
    Theme::refreshStyleSheet(qApp);
    repaintForTransparency();
}

void MainWindow::repaintForTransparency()
{
    // Los widgets que se pintan a mano consultan Theme::*Bg() en cada
    // repintado, asi que basta con invalidarlos todos.
    m_background->update();
    const QList<QWidget*> children = findChildren<QWidget*>();
    for (QWidget* child : children)
        child->update();
}

void MainWindow::chooseBackgroundImage()
{
    const QString start = Settings::backgroundImage().isEmpty()
        ? QStandardPaths::writableLocation(QStandardPaths::PicturesLocation)
        : QFileInfo(Settings::backgroundImage()).absolutePath();

    const QString file = QFileDialog::getOpenFileName(
        this, Lang::tr("Elegir imagen de fondo"), start,
        Lang::tr("Imagenes (*.jpg *.jpeg *.png *.bmp *.webp *.gif);;Todos los archivos (*)"));

    if (file.isEmpty())
        return;

    if (!m_background->setImagePath(file)) {
        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                             Lang::tr("No se pudo leer la imagen."));
        return;
    }

    Settings::setBackgroundImage(file);
    applyBackgroundSettings();
}

void MainWindow::clearBackgroundImage()
{
    m_background->setImagePath(QString());
    Settings::setBackgroundImage(QString());
    applyBackgroundSettings();
}

void MainWindow::setBackgroundTransparency(int percent)
{
    Settings::setBackgroundTransparency(percent);
    if (!m_background->hasImage())
        return;

    Theme::setPanelTransparency(percent / 100.0f);
    Theme::refreshStyleSheet(qApp);
    repaintForTransparency();
}

void MainWindow::setBackgroundDarkening(int percent)
{
    Settings::setBackgroundDarkening(percent);
    m_background->setDarkening(percent);
}

void MainWindow::setBackgroundMode(int mode)
{
    Settings::setBackgroundMode(mode);
    m_background->setMode(static_cast<BackgroundHost::Mode>(qBound(0, mode, 3)));
}

void MainWindow::openSettings()
{
    if (!m_settings) {
        m_settings = new SettingsDialog(this);

        connect(m_settings, &SettingsDialog::themeChanged, this, &MainWindow::applyTheme);
        connect(m_settings, &SettingsDialog::restartRequested,
                this, &MainWindow::restartForLanguage);

        connect(m_settings, &SettingsDialog::backgroundImageChanged,
                this, [this](const QString& path) {
                    if (path.isEmpty()) {
                        clearBackgroundImage();
                        return;
                    }
                    if (!m_background->setImagePath(path)) {
                        QMessageBox::warning(this, QStringLiteral("Roxas Player"),
                                             Lang::tr("No se pudo leer la imagen."));
                        return;
                    }
                    Settings::setBackgroundImage(path);
                    applyBackgroundSettings();
                });

        connect(m_settings, &SettingsDialog::backgroundModeChanged,
                this, &MainWindow::setBackgroundMode);
        connect(m_settings, &SettingsDialog::transparencyChanged,
                this, &MainWindow::setBackgroundTransparency);
        connect(m_settings, &SettingsDialog::darkeningChanged,
                this, &MainWindow::setBackgroundDarkening);
    }

    m_settings->show();
    m_settings->raise();
    m_settings->activateWindow();
}

void MainWindow::applyTheme(const QString& paletteId)
{
    Theme::setPalette(paletteId);
    Settings::setThemeId(paletteId);

    // La hoja de estilos se regenera con los colores nuevos; los widgets que
    // se pintan a mano consultan Theme::* en cada repintado, asi que basta
    // con invalidarlos.
    Theme::refreshStyleSheet(qApp);
    repaintForTransparency();

    if (m_settings)
        m_settings->update();
}

void MainWindow::restartForLanguage()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Roxas Player"),
        Lang::tr("Se reiniciara Roxas Player para aplicar el idioma.\n"
                 "Se perdera la reproduccion en curso."),
        QMessageBox::Ok | QMessageBox::Cancel, QMessageBox::Ok);

    if (answer != QMessageBox::Ok)
        return;

    saveSession();
    Settings::store().sync();

    QProcess::startDetached(qApp->applicationFilePath(), QStringList());
    qApp->quit();
}

void MainWindow::showAppMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));
    const QColor iconColor = Theme::TextDim;

    menu.addAction(Icons::icon(Icons::Plus, iconColor),
                   Lang::tr("Abrir archivos..."),
                   QKeySequence(Qt::CTRL | Qt::Key_O),
                   this, &MainWindow::openFilesDialog);

    menu.addAction(Icons::icon(Icons::FolderOpen, iconColor),
                   Lang::tr("Abrir carpeta..."),
                   QKeySequence(Qt::CTRL | Qt::Key_K),
                   m_library, &LibraryPanel::openFolderDialog);

    menu.addSeparator();

    QAction* eqAction = menu.addAction(Icons::icon(Icons::Equalizer, iconColor),
                                       Lang::tr("Ecualizador de 8 bandas"));
    eqAction->setCheckable(true);
    eqAction->setChecked(m_equalizer->isVisible());
    eqAction->setShortcut(QKeySequence(Qt::CTRL | Qt::Key_E));
    connect(eqAction, &QAction::toggled, this, [this](bool open) {
        m_playerBar->setEqualizerPanelOpen(open);
        toggleEqualizerPanel(open);
    });

    menu.addSeparator();

    menu.addAction(Icons::icon(Icons::Settings, iconColor),
                   Lang::tr("Configuracion..."),
                   this, &MainWindow::openSettings);

    // --- submenu de fondo --------------------------------------------------
    QMenu* background = menu.addMenu(Icons::icon(Icons::Image, iconColor),
                                     QStringLiteral("Fondo"));
    background->setFont(Theme::uiFont(9));

    background->addAction(Lang::tr("Elegir imagen..."),
                          this, &MainWindow::chooseBackgroundImage);

    QAction* clearAction = background->addAction(Lang::tr("Quitar imagen"),
                                                 this, &MainWindow::clearBackgroundImage);
    clearAction->setEnabled(m_background->hasImage());

    background->addSeparator();

    QMenu* fitMenu = background->addMenu(Lang::tr("Ajuste"));
    fitMenu->setFont(Theme::uiFont(9));
    auto* fitGroup = new QActionGroup(fitMenu);
    fitGroup->setExclusive(true);

    const BackgroundHost::Mode modes[] = {
        BackgroundHost::Mode::Cover, BackgroundHost::Mode::Fit,
        BackgroundHost::Mode::Stretch, BackgroundHost::Mode::Tile
    };
    for (int i = 0; i < 4; ++i) {
        QAction* action = fitMenu->addAction(BackgroundHost::modeName(modes[i]));
        action->setCheckable(true);
        action->setChecked(m_background->mode() == modes[i]);
        action->setEnabled(m_background->hasImage());
        fitGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, i]() { setBackgroundMode(i); });
    }

    background->addSeparator();

    // Deslizadores incrustados en el propio menu: se ajustan viendo el
    // resultado en vivo, sin abrir un dialogo aparte.
    const auto addSlider = [this, background](const QString& title, int value,
                                              int minimum, int maximum,
                                              void (MainWindow::*apply)(int)) {
        auto* panel = new QWidget(background);
        panel->setObjectName(QStringLiteral("bgMenuPanel"));
        auto* layout = new QVBoxLayout(panel);
        layout->setContentsMargins(14, 5, 14, 7);
        layout->setSpacing(3);

        auto* header = new QHBoxLayout;
        auto* label = new QLabel(title, panel);
        label->setObjectName(QStringLiteral("bgMenuLabel"));
        label->setFont(Theme::uiFont(8));
        auto* readout = new QLabel(QStringLiteral("%1 %").arg(value), panel);
        readout->setObjectName(QStringLiteral("bgMenuValue"));
        readout->setFont(Theme::uiFont(8));
        readout->setAlignment(Qt::AlignRight);
        header->addWidget(label);
        header->addStretch(1);
        header->addWidget(readout);
        layout->addLayout(header);

        auto* slider = new QSlider(Qt::Horizontal, panel);
        slider->setObjectName(QStringLiteral("bgSlider"));
        slider->setRange(minimum, maximum);
        slider->setValue(value);
        slider->setFixedWidth(190);
        slider->setEnabled(m_background->hasImage());
        layout->addWidget(slider);

        connect(slider, &QSlider::valueChanged, this, [this, readout, apply](int v) {
            readout->setText(QStringLiteral("%1 %").arg(v));
            (this->*apply)(v);
        });

        auto* action = new QWidgetAction(background);
        action->setDefaultWidget(panel);
        background->addAction(action);
    };

    addSlider(Lang::tr("Transparencia de los paneles"),
              Settings::backgroundTransparency(), 0, 85,
              &MainWindow::setBackgroundTransparency);

    addSlider(Lang::tr("Oscurecer la imagen"),
              Settings::backgroundDarkening(), 0, 90,
              &MainWindow::setBackgroundDarkening);

    menu.addSeparator();
    menu.addAction(Icons::icon(Icons::Close, iconColor),
                   Lang::tr("Salir"), this, &MainWindow::close);

    menu.exec(globalPos);
}

// -------------------------------------------------------------------- ventana

void MainWindow::toggleMaximized()
{
    if (isMaximized())
        showNormal();
    else
        showMaximized();
    m_titleBar->setMaximized(isMaximized());
}

void MainWindow::toggleEqualizerPanel(bool visible)
{
    m_equalizer->setVisible(visible);
    Settings::setEqPanelVisible(visible);
    // El plot solo consume FFT mientras se ve y hay audio.
    m_equalizer->setAnalyzerActive(visible && m_engine->isPlaying());
}

// --- redimensionado de la ventana sin marco --------------------------------

Qt::Edges MainWindow::edgesAt(const QPoint& pos) const
{
    Qt::Edges edges;
    if (pos.x() <= kResizeBorder)                 edges |= Qt::LeftEdge;
    if (pos.x() >= width() - kResizeBorder)       edges |= Qt::RightEdge;
    if (pos.y() <= kResizeBorder)                 edges |= Qt::TopEdge;
    if (pos.y() >= height() - kResizeBorder)      edges |= Qt::BottomEdge;
    return edges;
}

bool MainWindow::eventFilter(QObject* watched, QEvent* event)
{
    // La ventana no tiene marco, asi que el borde sensible al redimensionado
    // se detecta a mano. El filtro es global porque los paneles hijos se
    // comen los eventos de raton antes de que lleguen a la ventana.
    if (isMaximized() || isFullScreen())
        return QMainWindow::eventFilter(watched, event);

    const auto restoreCursor = [this]() {
        if (m_cursorOverridden) {
            QApplication::restoreOverrideCursor();
            m_cursorOverridden = false;
        }
        m_hoverEdges = Qt::Edges();
    };

    switch (event->type()) {
    case QEvent::MouseMove: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        const QPoint pos = mapFromGlobal(mouse->globalPosition().toPoint());
        if (!rect().contains(pos)) {
            restoreCursor();
            break;
        }

        const Qt::Edges edges = edgesAt(pos);
        if (edges == m_hoverEdges)
            break;

        restoreCursor();
        m_hoverEdges = edges;

        if (edges == (Qt::LeftEdge | Qt::TopEdge) || edges == (Qt::RightEdge | Qt::BottomEdge))
            QApplication::setOverrideCursor(Qt::SizeFDiagCursor);
        else if (edges == (Qt::RightEdge | Qt::TopEdge) || edges == (Qt::LeftEdge | Qt::BottomEdge))
            QApplication::setOverrideCursor(Qt::SizeBDiagCursor);
        else if (edges & (Qt::LeftEdge | Qt::RightEdge))
            QApplication::setOverrideCursor(Qt::SizeHorCursor);
        else if (edges & (Qt::TopEdge | Qt::BottomEdge))
            QApplication::setOverrideCursor(Qt::SizeVerCursor);
        else
            break;

        m_cursorOverridden = true;
        break;
    }

    case QEvent::MouseButtonPress: {
        auto* mouse = static_cast<QMouseEvent*>(event);
        if (mouse->button() != Qt::LeftButton || m_hoverEdges == Qt::Edges())
            break;
        if (QWindow* handle = windowHandle()) {
            handle->startSystemResize(m_hoverEdges);
            return true;
        }
        break;
    }

    case QEvent::Leave:
        if (watched == this)
            restoreCursor();
        break;

    default:
        break;
    }

    return QMainWindow::eventFilter(watched, event);
}
