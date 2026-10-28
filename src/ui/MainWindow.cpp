#include "ui/MainWindow.h"

#include "core/LibraryScanner.h"
#include "core/MetadataService.h"
#include "core/Lang.h"
#include "core/Settings.h"
#include "core/ShellUtils.h"
#include "core/SystemAudioTap.h"
#include "core/WaveformWorker.h"
#include "ui/BackgroundHost.h"
#include "ui/CoverCropDialog.h"
#include "ui/EqCurveEditor.h"
#include "ui/EqualizerPanel.h"
#include "ui/FileListPanel.h"
#include "ui/Icons.h"
#include "ui/LibraryPanel.h"
#include "ui/MetadataEditor.h"
#include "ui/TrackDetails.h"
#include "ui/NowPlayingPanel.h"
#include "ui/PanelDock.h"
#include "ui/PlayerBar.h"
#include "ui/PlaylistPanel.h"
#include "ui/RibbonTabBar.h"
#include "ui/SettingsDialog.h"
#include "ui/SeekBar.h"
#include "ui/Theme.h"
#include "ui/TitleBar.h"
#include "ui/VideoMasks.h"
#include "ui/VideoPanel.h"
#include "ui/VideoFilterPanel.h"
#include "ui/SubtitleStyle.h"
#include "core/Subtitles.h"
#include "ui/Visualizations.h"

#include <QActionGroup>
#include <QApplication>
#include <QCloseEvent>
#include <QDesktopServices>
#include <QDir>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMimeData>
#include <QKeyEvent>
#include <QMoveEvent>
#include <QCryptographicHash>
#include <QDirIterator>
#include <QFileDialog>
#include <QFileInfo>
#include <QInputDialog>
#include <QGuiApplication>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMessageBox>
#include <QScreen>
#include <QMouseEvent>
#include <QProcess>
#include <QResizeEvent>
#include <QSettings>
#include <QShortcut>
#include <QSlider>
#include <QSplitter>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QThread>
#include <QTimer>
#include <QElapsedTimer>
#include <QTextStream>
#include <memory>
#include <QToolTip>
#include <QAbstractItemView>
#include <QAbstractSlider>
#include <QAbstractSpinBox>
#include <QColorDialog>
#include <QComboBox>
#include <QKeySequenceEdit>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <QUrl>
#include <QVBoxLayout>
#include <QWidgetAction>
#include <QWindow>

#ifdef Q_OS_WIN
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  include <windows.h>
#  include <dbt.h>
#  include <shellapi.h>
#endif

namespace {
constexpr int kResizeBorder = 6;   // franja sensible al redimensionado

// Ruta del desinstalador que Inno Setup deja junto al ejecutable
// (unins000.exe, unins001.exe... segun cuantas veces se haya reinstalado).
// Devuelve una cadena vacia si lo que se esta ejecutando es la copia portable,
// que ni se instala ni se desinstala.
QString uninstallerPath()
{
    const QDir dir(QCoreApplication::applicationDirPath());
    const QStringList found = dir.entryList({QStringLiteral("unins*.exe")},
                                            QDir::Files, QDir::Name);
    return found.isEmpty() ? QString() : dir.absoluteFilePath(found.last());
}

// Huella de la combinacion de monitores: cuales hay, a que resolucion real y
// donde esta cada uno. Se usa como clave para recordar la ventana en cada
// situacion (Settings::WindowPlacement).
QString currentScreenKey()
{
    QStringList parts;
    for (const QScreen* screen : QGuiApplication::screens()) {
        // En pixeles fisicos: la escala de Qt depende de la propia
        // resolucion y no debe cambiar la huella.
        const qreal dpr = screen->devicePixelRatio();
        const QRect g = screen->geometry();
        parts << QStringLiteral("%1|%2|%3|%4x%5@%6,%7")
                     .arg(screen->manufacturer(), screen->model(), screen->serialNumber())
                     .arg(qRound(g.width() * dpr)).arg(qRound(g.height() * dpr))
                     .arg(qRound(g.x() * dpr)).arg(qRound(g.y() * dpr));
    }
    parts.sort();   // el orden de la lista de Qt no significa nada
    if (const QScreen* primary = QGuiApplication::primaryScreen())
        parts << QStringLiteral("primary=") + primary->name();

    return QString::fromLatin1(
        QCryptographicHash::hash(parts.join(QLatin1Char(';')).toUtf8(),
                                 QCryptographicHash::Sha1).toHex().left(16));
}

QList<QByteArray> connectedOutputIds()
{
    QList<QByteArray> ids;
    for (const SystemAudioTap::Device& device : SystemAudioTap::outputDevices())
        ids << device.id;
    return ids;
}

// Interfaz a la que hay que pasar lo procesado segun lo que hay conectado
// ahora (`present`) y lo que habia antes (`before`). Vacio = quedarse donde
// se esta. Solo se eligen salidas ya usadas alguna vez (Settings::knownOutputs).
QByteArray chooseInterface(const QList<QByteArray>& present, const QList<QByteArray>& before)
{
    if (!Settings::autoSwitchOutputs())
        return {};

    const QByteArray current = Settings::systemAudioOutput();
    const QByteArray source  = Settings::systemAudioSource();
    const QList<QByteArray> known = Settings::knownOutputs();   // reciente primero

    const auto usable = [&](const QByteArray& id) {
        return present.contains(id) && id != source;
    };

    // La ultima conectada manda. Si llegan varias a la vez, la usada mas
    // recientemente.
    for (const QByteArray& id : known) {
        if (usable(id) && !before.contains(id))
            return id == current ? QByteArray() : id;
    }

    // Se fue la que se usaba: la conocida mas reciente que siga conectada.
    if (!current.isEmpty() && !present.contains(current)) {
        for (const QByteArray& id : known) {
            if (usable(id))
                return id;
        }
    }
    return {};
}
}

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QStringLiteral("Ryxus Player"));
    setWindowFlag(Qt::FramelessWindowHint, true);
    setAttribute(Qt::WA_TranslucentBackground, false);
    setMinimumSize(1100, 640);

    // Soltar pistas o carpetas del Explorador sobre la ventana.
    setAcceptDrops(true);

    m_engine = new AudioEngine(this);

    buildUi();
    wireSignals();
    installShortcuts();
    restoreSession();

    // Seguimiento de la ventana por combinacion de monitores: se apunta cada
    // cambio y, si se conecta, quita o cambia de resolucion un monitor, se
    // recupera lo que hubiera para la combinacion nueva.
    m_placementSave = new QTimer(this);
    m_placementSave->setSingleShot(true);
    m_placementSave->setInterval(1000);
    connect(m_placementSave, &QTimer::timeout, this, &MainWindow::saveWindowPlacement);

    // Windows tarda un momento en recolocar las ventanas: se espera a que
    // termine antes de poner la nuestra donde toca.
    m_screenSetup = new QTimer(this);
    m_screenSetup->setSingleShot(true);
    m_screenSetup->setInterval(800);
    connect(m_screenSetup, &QTimer::timeout, this, &MainWindow::onScreenSetupChanged);

    const auto setupChanging = [this]() {
        m_placementSave->stop();
        m_screenSetup->start();
    };
    connect(qApp, &QGuiApplication::screenAdded, this, [this, setupChanging](QScreen* added) {
        watchScreen(added);
        setupChanging();
    });
    connect(qApp, &QGuiApplication::screenRemoved, this, setupChanging);
    connect(qApp, &QGuiApplication::primaryScreenChanged, this, setupChanging);
    for (QScreen* screen : QGuiApplication::screens())
        watchScreen(screen);

    if (!m_engine->isInitialized()) {
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
                             Lang::tr("%1\n\nLa interfaz funciona, pero no habra sonido.")
                                 .arg(m_engine->lastError()));
    }

    qApp->installEventFilter(this);

    // RYXUS_PERF_LOG=<archivo>: cada 2 s se apunta cuanto se bloqueo el hilo
    // de la interfaz, los cortes de audio y los fotogramas de video. Es la
    // herramienta para comprobar que una optimizacion sirve de algo.
    if (const QString log = qEnvironmentVariable("RYXUS_PERF_LOG"); !log.isEmpty()) {
        auto* probe = new QTimer(this);
        probe->setInterval(10);
        auto clock = std::make_shared<QElapsedTimer>();
        auto window = std::make_shared<QElapsedTimer>();
        auto maxGap = std::make_shared<qint64>(0);
        auto stalls = std::make_shared<int>(0);
        clock->start();
        window->start();
        connect(probe, &QTimer::timeout, this, [=]() {
            const qint64 gap = clock->restart();
            *maxGap = std::max(*maxGap, gap);
            if (gap > 50)
                ++*stalls;
            if (window->elapsed() < 2000)
                return;
            int received = 0, painted = 0;
            m_video->takeFrameStats(&received, &painted);
            QFile file(log);
            if (file.open(QIODevice::Append | QIODevice::Text)) {
                QTextStream(&file) << "maxGapMs=" << *maxGap << " stalls50=" << *stalls
                                   << " audioUnderruns=" << m_engine->takeStreamUnderruns()
                                   << " framesIn=" << received << " framesPainted=" << painted
                                   << " pos=" << m_engine->positionMs() << "\n";
            }
            *maxGap = 0;
            *stalls = 0;
            window->restart();
        });
        probe->start();
    }

    // Siempre arranca maximizado (contra el area util: la barra de tareas,
    // este donde este, queda a la vista). Lo restaurado arriba es el tamano
    // al que se vuelve al desmaximizar. En diferido, con la ventana ya en su
    // monitor.
    QTimer::singleShot(0, this, [this]() {
        if (!isWindowMaximized())
            toggleMaximized();
        else
            fitMaximizedToWorkArea();
    });
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
    m_background->setEngine(m_engine);
    QWidget* central = m_background;
    setCentralWidget(central);

    auto* root = new QVBoxLayout(central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    m_titleBar = new TitleBar(central);
    root->addWidget(m_titleBar);

    // --- cuerpo: paneles acoplables ----------------------------------------
    //
    // Los paneles viven en un QMainWindow interno, sin marco ni fondo, entre
    // la barra de titulo y la de reproduccion. Asi se mueven, se agrupan en
    // pestanas, flotan y se estiran como en Photoshop, y el fondo de la
    // ventana se sigue viendo a traves de ellos. La disposicion de partida la
    // pone applyLayoutPreset() o la que se guardo (restoreSession).
    m_dockHost = new QMainWindow(central);
    // QMainWindow se crea siempre como ventana aparte, aunque tenga padre:
    // hay que convertirlo en un widget hijo para que vaya dentro del layout.
    m_dockHost->setWindowFlags(Qt::Widget);
    m_dockHost->setObjectName(QStringLiteral("dockHost"));
    m_dockHost->setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowNestedDocks
                               | QMainWindow::AllowTabbedDocks);
    m_dockHost->setTabPosition(Qt::AllDockWidgetAreas, QTabWidget::North);

    // Qt pide un widget central aunque sea de relleno. Oculto no ocupa sitio
    // y los paneles se reparten toda la ventana.
    auto* placeholder = new QWidget(m_dockHost);
    placeholder->hide();
    m_dockHost->setCentralWidget(placeholder);

    QWidget* panels = m_dockHost;

    m_nowPlaying = new NowPlayingPanel(m_engine, panels);

    m_centerColumn = new QWidget(panels);
    QWidget* centerColumn = m_centerColumn;
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

    m_playlists = new PlaylistPanel(panels);
    m_playlists->setMinimumWidth(240);

    m_equalizer = new EqualizerPanel(&m_engine->equalizer(), m_engine, panels);

    // Los identificadores son los que se guardan en las disposiciones: no
    // cambiarlos o las guardadas dejan de encontrar sus paneles.
    m_docks[DockNowPlaying] = new PanelDock(Lang::tr("Reproduciendo"),
        QStringLiteral("dockNowPlaying"), m_nowPlaying, m_dockHost);
    m_docks[DockFiles] = new PanelDock(Lang::tr("Archivos"),
        QStringLiteral("dockFiles"), centerColumn, m_dockHost);
    m_docks[DockPlaylist] = new PanelDock(Lang::tr("Lista de reproduccion"),
        QStringLiteral("dockPlaylist"), m_playlists, m_dockHost);
    m_docks[DockEqualizer] = new PanelDock(Lang::tr("Ecualizador"),
        QStringLiteral("dockEqualizer"), m_equalizer, m_dockHost);

    m_video = new VideoPanel(m_engine, panels);
    m_video->setMask(Settings::videoMask());
    m_docks[DockVideo] = new PanelDock(Lang::tr("Video"),
        QStringLiteral("dockVideo"), m_video, m_dockHost);

    m_videoFilters = new VideoFilterPanel(panels);
    m_videoFilters->setCurrentMask(Settings::videoMask());
    m_docks[DockVideoFilters] = new PanelDock(Lang::tr("Filtros de video"),
        QStringLiteral("dockVideoFilters"), m_videoFilters, m_dockHost);

    // Todos tienen que estar dentro antes de restaurar una disposicion: la
    // que se guardo solo recoloca paneles que ya estan en la ventana.
    for (PanelDock* dock : m_docks)
        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, dock);
    m_docks[DockVideo]->hide();   // solo cuando hay imagen
    m_docks[DockVideoFilters]->hide();

    root->addWidget(m_dockHost, 1);

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

                // Entrar a "Etiquetas" carga la pista que se este inspeccionando.
                // Antes esto se saltaba en cuanto el editor ya tenia algo
                // cargado, y el editor se quedaba clavado en la cancion
                // anterior por mucho que se cambiara de seleccion.
                if (index == 1)
                    syncTagEditor(m_detailsTrack.isValid() ? m_detailsTrack : m_currentTrack);
            });

    // --- biblioteca --------------------------------------------------------
    connect(m_library, &LibraryPanel::folderSelected,  this, &MainWindow::onFolderSelected);
    connect(m_library, &LibraryPanel::folderActivated, this, &MainWindow::onFolderActivated);
    connect(m_library, &LibraryPanel::folderPlayRequested,
            this, &MainWindow::onFolderPlayRequested);

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
    connect(m_playlists, &PlaylistPanel::selectionChanged, this, &MainWindow::setDetailsTrack);

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
    connect(m_playerBar, &PlayerBar::playClicked,  this, &MainWindow::onPlayClicked);
    connect(m_playerBar, &PlayerBar::pauseClicked, this, [this]() { m_engine->pause(); });
    connect(m_playerBar, &PlayerBar::stopClicked,      this, &MainWindow::stopPlayback);
    connect(m_playerBar, &PlayerBar::nextClicked,      this, &MainWindow::playNext);
    connect(m_playerBar, &PlayerBar::previousClicked,  this, &MainWindow::playPrevious);
    connect(m_playerBar, &PlayerBar::repeatCycled,     this, &MainWindow::cycleRepeatMode);

    connect(m_playerBar, &PlayerBar::shuffleToggled, this, &MainWindow::setShuffleEnabled);

    connect(m_playerBar, &PlayerBar::volumeChanged, this, [this](float value) {
        m_engine->setVolume(value);
        m_engine->setMuted(false);
        m_playerBar->setMuted(false);
        Settings::setVolume(value);
    });

    connect(m_playerBar, &PlayerBar::muteToggled, this, &MainWindow::toggleMute);

    connect(m_playerBar, &PlayerBar::equalizerPanelToggled,
            this, &MainWindow::toggleEqualizerPanel);

    connect(m_playerBar, &PlayerBar::timeDisplayToggled, this, [this]() {
        SeekBar* seek = m_playerBar->seekBar();
        seek->setShowRemaining(!seek->showsRemaining());
    });

    connect(m_playerBar, &PlayerBar::abLoopClicked, this, &MainWindow::stepAbLoop);

    connect(m_playerBar->seekBar(), &SeekBar::seekRequested, this, [this](qint64 ms) {
        m_engine->seekMs(ms);
    });

    // --- ecualizador -------------------------------------------------------
    connect(m_equalizer, &EqualizerPanel::enabledChanged, this, [this](bool enabled) {
        m_playerBar->setEqualizerActive(enabled);
    });

    // El panel del ecualizador ahora se abre y se cierra tambien desde su
    // barra de titulo, el menu o una disposicion: el boton de la barra
    // inferior sigue lo que haga el panel, venga de donde venga.
    PanelDock* eqDock = m_docks[DockEqualizer];
    connect(eqDock->toggleViewAction(), &QAction::toggled, this, [this](bool open) {
        m_playerBar->setEqualizerPanelOpen(open);
    });
    // El plot solo consume FFT mientras se ve: en pestanas puede estar
    // abierto pero tapado por otro panel.
    connect(eqDock, &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_equalizer->setAnalyzerActive(visible && audioIsFlowing());
    });

    // --- motor -------------------------------------------------------------
    connect(m_engine, &AudioEngine::stateChanged, this, &MainWindow::onEngineStateChanged);

    // Lo que abre FFmpeg dice su duracion un poco despues de abrirlo.
    connect(m_engine, &AudioEngine::durationChanged, this, [this](qint64 ms) {
        m_playerBar->seekBar()->setDurationMs(ms);
    });
    connect(m_engine, &AudioEngine::mediaError, this, [this](const QString& message) {
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"), message);
    });

    // Al pasar de un video a otro, FFmpeg dice "sin imagen" y enseguida "con
    // imagen". Se espera un momento antes de cambiar de disposicion, para no
    // saltar a la de audio y volver.
    m_videoModeTimer = new QTimer(this);
    m_videoModeTimer->setSingleShot(true);
    m_videoModeTimer->setInterval(400);
    connect(m_videoModeTimer, &QTimer::timeout, this, &MainWindow::onVideoActiveChanged);
    connect(m_engine, &AudioEngine::videoActiveChanged, m_videoModeTimer,
            qOverload<>(&QTimer::start));

    connect(m_video, &VideoPanel::menuRequested, this, &MainWindow::showVideoMenu);
    connect(m_video, &VideoPanel::filtersRequested, this, [this]() {
        setVideoFiltersVisible(m_docks[DockVideoFilters]->isHidden());
    });
    connect(m_video, &VideoPanel::nextMaskRequested, this, [this]() {
        runCommand(Commands::NextVideoMask);
    });
    applyVideoGestureSettings();
    connect(m_video, &VideoPanel::volumeRequested, this, &MainWindow::setVolumeTo);

    // Paleta de filtros: miniaturas del fotograma en curso mientras se ve, y
    // un clic en una aplica esa mascara.
    connect(m_video, &VideoPanel::thumbnailSourceChanged,
            m_videoFilters, &VideoFilterPanel::setSource);
    connect(m_docks[DockVideoFilters], &QDockWidget::visibilityChanged, this, [this](bool visible) {
        m_video->setThumbnailsWanted(visible);
    });
    connect(m_videoFilters, &VideoFilterPanel::maskSelected, this, &MainWindow::setVideoMask);
    // Cerrarla con su boton es decir "no me la vuelvas a sacar".
    connect(m_docks[DockVideoFilters], &PanelDock::userClosed, this, [this]() {
        Settings::setVideoFiltersVisible(false);
    });

    // Los subtitulos de archivo son de un video concreto: al abrir otro se
    // quitan, y si junto al nuevo hay unos con su mismo nombre, se cargan.
    connect(m_engine, &AudioEngine::trackOpened, this, [this](const QString& path, qint64) {
        m_video->clearSubtitles();
        m_videoFilters->resetSource();
        // Las previsualizaciones salen del mismo archivo.
        m_video->setPreviewSource(TrackInfo::isVideo(path) ? path : QString());
        if (TrackInfo::isVideo(path))
            loadSidecarSubtitles(path);
    });
    connect(m_video, &VideoPanel::fullscreenRequested, this, &MainWindow::toggleVideoFullscreen);
    connect(m_video, &VideoPanel::playPauseRequested, this, &MainWindow::togglePlayPause);
}

void MainWindow::installShortcuts()
{
    // Un atajo por orden, todos configurables desde Configuracion. Valen en
    // cualquier ventana del programa, paneles flotantes incluidos.
    for (int command = 0; command < Commands::Count; ++command) {
        auto* shortcut = new QShortcut(this);
        shortcut->setContext(Qt::ApplicationShortcut);
        connect(shortcut, &QShortcut::activated, this, [this, command]() { runCommand(command); });
        m_shortcuts[command] = shortcut;
    }
    applyShortcuts();
}

void MainWindow::applyShortcuts()
{
    for (int command = 0; command < Commands::Count; ++command)
        m_shortcuts[command]->setKey(Commands::keys(command));
}

void MainWindow::runCommand(int command)
{
    switch (command) {
    case Commands::PlayPause:      togglePlayPause(); break;
    case Commands::Stop:           stopPlayback(); break;
    case Commands::Next:           playNext(); break;
    case Commands::Previous:       playPrevious(); break;
    case Commands::SeekForward:    seekBy(Settings::seekStepMs()); break;
    case Commands::SeekBackward:   seekBy(-Settings::seekStepMs()); break;
    case Commands::VolumeUp:       stepVolume(Settings::volumeStepPercent() / 100.0f); break;
    case Commands::VolumeDown:     stepVolume(-Settings::volumeStepPercent() / 100.0f); break;
    case Commands::Mute:           toggleMute(); break;
    case Commands::Shuffle: {
        const bool on = !m_shuffle;
        m_playerBar->setShuffle(on);
        setShuffleEnabled(on);
        break;
    }
    case Commands::Repeat:         cycleRepeatMode(); break;
    case Commands::AbLoop:         stepAbLoop(); break;

    case Commands::OpenFiles:      openFilesDialog(); break;
    case Commands::OpenFolder:     m_library->openFolderDialog(); break;
    case Commands::EditTags:       openTagEditorForCurrent(); break;

    case Commands::EqualizerPanel: toggleEqualizerPanel(m_docks[DockEqualizer]->isHidden()); break;
    case Commands::EqualizerOnOff: m_equalizer->setEqualizerEnabled(!Settings::eqEnabled()); break;

    case Commands::PanelNowPlaying: togglePanel(DockNowPlaying); break;
    case Commands::PanelFiles:      togglePanel(DockFiles); break;
    case Commands::PanelPlaylist:   togglePanel(DockPlaylist); break;
    case Commands::ShowAllPanels:   setAllPanelsVisible(true); break;
    case Commands::HideAllPanels:   setAllPanelsVisible(false); break;

    case Commands::NextLayout:      cycleLayout(1); break;
    case Commands::PreviousLayout:  cycleLayout(-1); break;
    case Commands::LayoutClassic:   applyLayoutPreset(LayoutClassic); break;
    case Commands::LayoutLibrary:   applyLayoutPreset(LayoutLibrary); break;
    case Commands::LayoutListening: applyLayoutPreset(LayoutListening); break;
    case Commands::LayoutStudio:    applyLayoutPreset(LayoutStudio); break;
    case Commands::SaveLayout:      saveCurrentLayout(); break;

    case Commands::ChooseBackground:  chooseBackgroundImage(); break;
    case Commands::ClearBackground:   clearBackgroundImage(); break;
    case Commands::NextVisualization: cycleVisualization(); break;

    case Commands::VideoCinema:        applyVideoLayout(VideoCinema); break;
    case Commands::VideoWithPlaylist:  applyVideoLayout(VideoWithPlaylist); break;
    case Commands::VideoWithEqualizer: applyVideoLayout(VideoWithEqualizer); break;
    case Commands::VideoStudio:        applyVideoLayout(VideoStudio); break;
    case Commands::NextVideoMask:
        setVideoMask((m_video->mask() + 1) % VideoMasks::Count);
        showCommandToast(Lang::tr("Mascara: %1").arg(VideoMasks::name(m_video->mask())));
        break;
    case Commands::VideoFullscreen:    toggleVideoFullscreen(); break;
    case Commands::AddAudioTrack:      addAudioTrackDialog(); break;
    case Commands::AddSubtitles:       addSubtitlesDialog(); break;
    case Commands::VideoFilters:
        setVideoFiltersVisible(m_docks[DockVideoFilters]->isHidden());
        break;

    case Commands::OpenSettings:   openSettings(); break;
    case Commands::ToggleMaximize: toggleMaximized(); break;
    case Commands::MinimizeWindow: showMinimized(); break;
    case Commands::Quit:           close(); break;
    default: break;
    }
}

void MainWindow::setShuffleEnabled(bool enabled)
{
    m_shuffle = enabled;
    Settings::setShuffle(enabled);
    if (enabled && m_playlists->currentPlaylist())
        m_playlists->currentPlaylist()->reshuffle();
}

void MainWindow::toggleMute()
{
    const bool muted = !m_engine->isMuted();
    m_engine->setMuted(muted);
    m_playerBar->setMuted(muted);
    Settings::setMuted(muted);
}

void MainWindow::stepVolume(float delta)
{
    setVolumeTo(m_engine->volume() + delta);
    // Con video a la vista, la barra de volumen tambien en la imagen.
    if (m_engine->hasVideo())
        m_video->showVolumeOsd(m_engine->volume());
}

void MainWindow::setVolumeTo(float volume)
{
    const float value = qBound(0.0f, volume, 1.0f);
    m_engine->setVolume(value);
    m_playerBar->setVolume(value);
    Settings::setVolume(value);
}

void MainWindow::seekBy(qint64 deltaMs)
{
    const qint64 duration = m_engine->durationMs();
    if (duration <= 0)
        return;
    const qint64 target = std::clamp<qint64>(m_engine->positionMs() + deltaMs, 0, duration - 1);
    m_engine->seekMs(target);
    if (m_engine->hasVideo())
        m_video->showSeekOsd(deltaMs, target);
}

void MainWindow::applyVideoGestureSettings()
{
    m_video->setScrubFramesPer100(Settings::videoScrubFramesPer100());
    m_video->setSeekStepMs(Settings::seekStepMs());
    m_video->setGestures(Settings::videoGestureSeek(), Settings::videoGestureVolume(),
                         Settings::videoGestureWheel());
    m_video->setPreviewWidth(Settings::videoPreviewWidth());
    const QString color = Settings::volumeBarColor();
    m_video->setVolumeBarStyle(Settings::volumeBarShape(),
                               color.isEmpty() ? QColor() : QColor(color));
}

void MainWindow::fillStepMenus(QMenu* menu)
{
    // Cuanto mueve cada pulsacion de una flecha (y cada paso de la rueda
    // sobre el video). Mantener la flecha pulsada repite.
    QMenu* seek = menu->addMenu(Lang::tr("Paso al avanzar o retroceder"));
    seek->setFont(Theme::uiFont(9));
    auto* seekGroup = new QActionGroup(seek);
    const int seekStep = Settings::seekStepMs();
    for (int seconds : {1, 2, 5, 10, 15, 30, 60}) {
        QAction* action = seek->addAction(Lang::tr("%1 s").arg(seconds));
        action->setCheckable(true);
        action->setChecked(seconds * 1000 == seekStep);
        seekGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, seconds]() {
            Settings::setSeekStepMs(seconds * 1000);
            m_video->setSeekStepMs(seconds * 1000);
        });
    }

    QMenu* volume = menu->addMenu(Lang::tr("Paso de volumen"));
    volume->setFont(Theme::uiFont(9));
    auto* volumeGroup = new QActionGroup(volume);
    const int volumeStep = Settings::volumeStepPercent();
    for (int percent : {1, 2, 5, 10, 20}) {
        QAction* action = volume->addAction(Lang::tr("%1 %").arg(percent));
        action->setCheckable(true);
        action->setChecked(percent == volumeStep);
        volumeGroup->addAction(action);
        connect(action, &QAction::triggered, this, [percent]() {
            Settings::setVolumeStepPercent(percent);
        });
    }
}

void MainWindow::stepAbLoop()
{
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
}

void MainWindow::cycleVisualization()
{
    // Ninguna -> la primera -> ... -> la ultima -> ninguna.
    int next = m_background->visualization() + 1;
    if (next >= Visualizations::count())
        next = -1;
    setBackgroundVisualization(next);
    showCommandToast(next < 0 ? Lang::tr("Sin visualizacion")
                              : Lang::tr(Visualizations::info(next).name));
}

void MainWindow::showCommandToast(const QString& text)
{
    // Aviso breve en el centro de la ventana: al cambiar algo con el teclado
    // se ve que paso sin tener que abrir ningun menu.
    QToolTip::showText(mapToGlobal(rect().center()), text, this, QRect(), 1500);
}

// ------------------------------------------------------------------- sesion

void MainWindow::restoreSession()
{
    // Primero lo guardado para estos monitores. Si es la primera vez con
    // ellos, lo ultimo que se uso en cualquier otra combinacion, ajustado a
    // lo que cabe.
    m_screenKey = currentScreenKey();
    if (!restoreWindowPlacement()) {
        const QByteArray geometry = Settings::windowGeometry();
        if (!geometry.isEmpty())
            restoreGeometry(geometry);
        else
            resize(1600, 860);

        clampIntoWorkArea();
    }

    const QByteArray splitter = Settings::splitterState();
    if (!splitter.isEmpty())
        m_centerSplitter->restoreState(splitter);

    // Disposicion de los paneles tal como se dejo. La primera vez, o si la
    // guardada es de una version que ya no encaja, la clasica. En diferido:
    // los tamanos de una disposicion solo se pueden repartir con la ventana
    // ya a su tamano final.
    if (!m_dockHost->restoreState(Settings::dockState(), kDockStateVersion))
        QTimer::singleShot(0, this, [this]() { applyLayoutPreset(LayoutClassic); });
    // Una disposicion guardada antes de que hubiera video no lo menciona y lo
    // dejaria a la vista: al arrancar no hay ninguno.
    m_docks[DockVideo]->hide();
    m_docks[DockVideoFilters]->hide();

    const float volume = Settings::volume();
    m_engine->setVolume(volume);
    m_playerBar->setVolume(volume);

    const bool muted = Settings::muted();
    m_engine->setMuted(muted);
    m_playerBar->setMuted(muted);

    m_shuffle = Settings::shuffle();
    m_playerBar->setShuffle(m_shuffle);

    // Si la captura del sistema quedo encendida, se vuelve a levantar. Si la
    // interfaz de la ultima vez no esta pero otra conocida si, se arranca con
    // esa; si no hay ninguna, se espera a que se conecte.
    m_presentOutputs = connectedOutputIds();
    if (Settings::systemAudioEnabled()) {
        const QByteArray other = chooseInterface(m_presentOutputs, m_presentOutputs);
        applySystemAudio(true, Settings::systemAudioSource(),
                         other.isEmpty() ? Settings::systemAudioOutput() : other);
    }

    m_repeat = static_cast<PlaylistModel::RepeatMode>(qBound(0, Settings::repeatMode(), 2));
    m_playerBar->setRepeatMode(int(m_repeat));

    m_playerBar->setEqualizerActive(Settings::eqEnabled());

    m_playerBar->setEqualizerPanelOpen(!m_docks[DockEqualizer]->isHidden());

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
    saveWindowPlacement();
    Settings::setSplitterState(m_centerSplitter->saveState());
    // Con un video abierto se guarda la disposicion de audio de la que se
    // vino: al arrancar no habra video.
    Settings::setDockState(m_inVideoMode ? m_audioLayoutState
                                         : m_dockHost->saveState(kDockStateVersion));
    Settings::setRepeatMode(int(m_repeat));
    Settings::setShuffle(m_shuffle);
    Settings::setPlaylists(m_playlists->serialize());
    Settings::setActivePlaylist(m_playlists->currentPlaylistIndex());
    m_equalizer->saveToSettings();
    Settings::store().sync();
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (m_videoFullscreen)
        toggleVideoFullscreen();   // que se guarde la ventana normal
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
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"), m_engine->lastError());
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

    setWindowTitle(QStringLiteral("%1 - Ryxus Player").arg(info.displayName()));

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

void MainWindow::onPlayClicked()
{
    // El boton de play no hace de pausa: para eso esta el de al lado. Si ya
    // esta sonando, vuelve al principio de la pista; si esta en pausa, reanuda.
    if (m_engine->isPlaying()) {
        m_engine->seekMs(0);
        m_playerBar->seekBar()->setPositionMs(0);
        if (m_loopStartMs >= 0 || m_loopEndMs >= 0) {
            m_loopStartMs = m_loopEndMs = -1;
            m_playerBar->setAbLoopActive(false);
        }
        return;
    }

    togglePlayPause();
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

    // El espectro y la onda siguen al audio, venga de donde venga: si lo que
    // suena entra por el cable virtual, tambien tienen que moverse.
    const bool flowing = audioIsFlowing();
    m_nowPlaying->setPlaying(flowing);
    m_equalizer->setAnalyzerActive(m_equalizer->isVisible() && flowing);
}

bool MainWindow::audioIsFlowing() const
{
    return m_engine->isPlaying() || (m_systemAudio && m_systemAudio->isRunning());
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

void MainWindow::onFolderPlayRequested(const QString& folder)
{
    auto* playlist = m_playlists->currentPlaylist();
    if (folder.isEmpty() || !playlist)
        return;

    // En el mismo orden en que se ven en la lista central.
    const QFileInfoList entries = QDir(folder).entryInfoList(
        QDir::Files | QDir::Readable, QDir::Name | QDir::LocaleAware);

    QVector<TrackInfo> tracks;
    tracks.reserve(entries.size());

    QGuiApplication::setOverrideCursor(Qt::WaitCursor);
    for (const QFileInfo& entry : entries) {
        const QString path = entry.absoluteFilePath();
        if (!TrackInfo::isSupported(path))
            continue;
        // Sin caratula: la lista no la muestra y leerla multiplicaria el coste.
        const TrackInfo info = MetadataService::read(path, false);
        if (info.isValid())
            tracks << info;
    }
    QGuiApplication::restoreOverrideCursor();

    if (tracks.isEmpty()) {
        QMessageBox::information(this, QStringLiteral("Ryxus Player"),
                                 Lang::tr("Esta carpeta no tiene pistas que se puedan reproducir."));
        return;
    }

    // A la cabecera de la cola, no al final: lo que ya habia sigue detras.
    playlist->insertTracks(0, tracks);

    // La lista central acompana a lo que se acaba de poner a sonar.
    onFolderSelected(folder);
    playPlaylistRow(0);
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
        setWindowTitle(QStringLiteral("%1 - Ryxus Player").arg(reloaded.displayName()));
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
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
                             Lang::tr("No se pudo leer la imagen."));
        return;
    }

    // Las caratulas se muestran cuadradas: una foto apaisada metida tal cual
    // sale deformada. Antes de escribir nada se elige el encuadre y a que
    // resolucion se guarda.
    CoverCropDialog crop(image, this);
    if (crop.exec() != QDialog::Accepted)
        return;

    image = crop.result();
    if (image.isNull())
        return;

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
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
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
    syncTagEditor(info);
}

void MainWindow::syncTagEditor(const TrackInfo& info)
{
    // El editor de etiquetas acompana a la pista inspeccionada. La unica
    // excepcion son los cambios sin guardar: ahi no se le quita al usuario lo
    // que esta escribiendo.
    if (!m_tagEditor || m_tagEditor->isDirty())
        return;

    if (!info.isValid()) {
        m_tagEditor->setTrack(TrackInfo());
        return;
    }

    if (info.path.compare(m_tagEditor->track().path, Qt::CaseInsensitive) == 0)
        return;

    // Se relee con caratula: los listados guardan las pistas sin ella.
    const TrackInfo full = MetadataService::read(info.path, true);
    m_tagEditor->setTrack(full.isValid() ? full : info);
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

void MainWindow::runUninstaller()
{
    const QString uninstaller = uninstallerPath();
    if (uninstaller.isEmpty())
        return;

    const auto answer = QMessageBox::question(
        this, Lang::tr("Desinstalar Ryxus Player"),
        Lang::tr("Se cerrara el reproductor y se abrira el desinstalador de "
                 "Windows.\n\n"
                 "Tus ajustes, temas y listas no se borran: si vuelves a "
                 "instalarlo, siguen ahi."),
        QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);

    if (answer != QMessageBox::Yes)
        return;

    // El desinstalador se copia a si mismo al temporal y relanza, asi que no
    // le molesta que el ejecutable siga en la carpeta al arrancar; aun asi se
    // cierra el reproductor para que no bloquee sus propias DLL.
    if (QProcess::startDetached(uninstaller, {}, QFileInfo(uninstaller).absolutePath()))
        close();
    else
        ShellUtils::revealInFileManager(uninstaller);
}

void MainWindow::revealInExplorer(const QString& path)
{
    ShellUtils::revealInFileManager(path);
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
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
                             Lang::tr("No se pudieron eliminar:\n%1")
                                 .arg(failures.join(QLatin1Char('\n'))));
    }

    // Si lo borrado era lo que habia en el editor de etiquetas, se vacia: si
    // no, seguiria ofreciendo guardar sobre un archivo que ya no existe.
    if (!m_tagEditor->track().path.isEmpty()
        && !QFile::exists(m_tagEditor->track().path)) {
        m_tagEditor->setTrack(TrackInfo());
    }
    if (m_detailsTrack.isValid() && !QFile::exists(m_detailsTrack.path))
        setDetailsTrack(TrackInfo());

    onFolderSelected(m_library->currentFolder());
}

void MainWindow::dragEnterEvent(QDragEnterEvent* event)
{
    // Solo se aceptan rutas locales; lo demas se deja pasar a quien toque.
    if (!event->mimeData()->hasUrls())
        return;

    for (const QUrl& url : event->mimeData()->urls()) {
        if (!url.isLocalFile())
            continue;
        const QString path = url.toLocalFile();
        if (QFileInfo(path).isDir() || TrackInfo::isSupported(path)) {
            event->acceptProposedAction();
            return;
        }
    }
}

void MainWindow::dropEvent(QDropEvent* event)
{
    QStringList paths;
    for (const QUrl& url : event->mimeData()->urls()) {
        if (url.isLocalFile())
            paths << url.toLocalFile();
    }

    if (paths.isEmpty())
        return;

    event->acceptProposedAction();
    openPaths(paths);
}

void MainWindow::openPaths(const QStringList& paths)
{
    QStringList files;
    QString     folderToOpen;   // la que se abre en el arbol y la lista central
    QString     trackToPlay;    // la pista concreta que arranca

    for (const QString& path : paths) {
        const QFileInfo fi(path);

        if (fi.isDir()) {
            const QString clean = QDir::cleanPath(fi.absoluteFilePath());
            if (folderToOpen.isEmpty())
                folderToOpen = clean;

            QStringList inFolder;
            QDirIterator it(fi.absoluteFilePath(), QDir::Files | QDir::Readable);
            while (it.hasNext()) {
                const QString candidate = it.next();
                if (TrackInfo::isSupported(candidate))
                    inFolder << candidate;
            }
            inFolder.sort(Qt::CaseInsensitive);

            if (trackToPlay.isEmpty() && !inFolder.isEmpty())
                trackToPlay = inFolder.first();
            files += inFolder;

        } else if (fi.isFile() && TrackInfo::isSupported(path)) {
            // Soltar un tema abre ademas la carpeta donde vive: asi queda a
            // mano el resto del disco, que es lo que se suele querer luego.
            if (folderToOpen.isEmpty())
                folderToOpen = QDir::cleanPath(fi.absolutePath());
            if (trackToPlay.isEmpty())
                trackToPlay = fi.absoluteFilePath();
            files << fi.absoluteFilePath();
        }
    }

    if (files.isEmpty())
        return;

    // --- la carpeta pasa a estar abierta en la biblioteca ------------------
    if (!folderToOpen.isEmpty()) {
        QStringList opened = m_library->openedFolders();
        if (!opened.contains(folderToOpen, Qt::CaseInsensitive)) {
            opened << folderToOpen;
            Settings::setLibraryFolders(opened);
            m_library->setOpenedFolders(opened);
        }
        m_library->selectFolder(folderToOpen);
        onFolderSelected(folderToOpen);
    }

    auto* playlist = m_playlists->currentPlaylist();
    if (!playlist)
        return;

    const int firstRow = playlist->rowCount();
    playlist->appendFiles(files);

    // Suena la pista que se solto, no la primera de la carpeta.
    const int target = trackToPlay.isEmpty() ? firstRow
                                             : playlist->indexOfPath(trackToPlay);
    playPlaylistRow(target >= 0 ? target : firstRow);
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
    m_background->setImageOpacity(Settings::backgroundImageOpacity());
    m_background->setVisualOpacity(Settings::backgroundVisualOpacity());
    m_background->setVisualization(Settings::backgroundVisualization());

    // Sin nada detras no tiene sentido volver translucidos los paneles: solo
    // dejarian ver el color base y la interfaz perderia contraste. El vumetro
    // cuenta como fondo, que para eso ocupa la ventana entera.
    const float transparency = m_background->hasBackdrop()
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
        BackgroundHost::imageFilter() + Lang::tr(";;Todos los archivos (*)"));

    if (file.isEmpty())
        return;

    if (!m_background->setImagePath(file)) {
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
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

void MainWindow::setBackgroundVisualization(int index)
{
    Settings::setBackgroundVisualization(index);
    m_background->setVisualization(index);

    // La capa viva cuenta como fondo: al encenderla o apagarla hay que rehacer
    // la transparencia de los paneles.
    applyBackgroundSettings();
}

void MainWindow::setBackgroundImageOpacity(int percent)
{
    Settings::setBackgroundImageOpacity(percent);
    m_background->setImageOpacity(percent);
}

void MainWindow::setBackgroundVisualOpacity(int percent)
{
    Settings::setBackgroundVisualOpacity(percent);
    m_background->setVisualOpacity(percent);
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
        connect(m_settings, &SettingsDialog::systemAudioChanged,
                this, &MainWindow::applySystemAudio);
        connect(m_settings, &SettingsDialog::shortcutsChanged,
                this, &MainWindow::applyShortcuts);
        connect(m_settings, &SettingsDialog::subtitleStyleChanged, this, [this]() {
            m_video->setSubtitleStyle(SubtitleStyle::load());
        });
        connect(m_settings, &SettingsDialog::visualizationChanged,
                this, &MainWindow::setBackgroundVisualization);
        connect(m_settings, &SettingsDialog::imageOpacityChanged,
                this, &MainWindow::setBackgroundImageOpacity);
        connect(m_settings, &SettingsDialog::visualOpacityChanged,
                this, &MainWindow::setBackgroundVisualOpacity);
        connect(m_settings, &SettingsDialog::restartRequested,
                this, &MainWindow::restartForLanguage);

        connect(m_settings, &SettingsDialog::backgroundImageChanged,
                this, [this](const QString& path) {
                    if (path.isEmpty()) {
                        clearBackgroundImage();
                        return;
                    }
                    if (!m_background->setImagePath(path)) {
                        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
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

void MainWindow::applySystemAudio(bool enabled, const QByteArray& source,
                                  const QByteArray& output)
{
    // Cambiar de interfaz es cambiar tambien de ecualizacion: lo de la que se
    // deja se guarda en su perfil y se carga el de la que llega. Si la nueva
    // todavia no tiene, se queda con lo que hay y lo hara suyo al irse.
    const QByteArray previous = Settings::systemAudioOutput();
    if (!output.isEmpty() && output != previous) {
        Settings::saveOutputProfile(previous);
        if (Settings::loadOutputProfile(output))
            m_equalizer->loadFromSettings();
    }

    Settings::setSystemAudioSource(source);
    Settings::setSystemAudioOutput(output);
    if (enabled)
        Settings::touchKnownOutput(output);
    if (m_settings)
        m_settings->refreshInterfaces();

    if (!m_systemAudio) {
        m_systemAudio = new SystemAudioTap(this);
        // El espectro, la onda y el fondo tienen que moverse tambien con lo
        // que entra por el cable, no solo con lo que reproduce el programa.
        m_systemAudio->setVisualSink(m_engine);
    }

    // Una orden nueva manda sobre los reintentos que quedaran pendientes.
    if (m_systemAudioRetry)
        m_systemAudioRetry->stop();
    m_systemAudioTries = 0;

    if (!enabled) {
        m_systemAudio->stop();
        m_engine->setChainBypassed(false);
        Settings::setSystemAudioEnabled(false);
        if (m_settings)
            m_settings->setSystemAudioStatus(Lang::tr("Captura del sistema apagada."), true);
        return;
    }

    startSystemAudio();
}

void MainWindow::startSystemAudio()
{
    // Nada mas encender el PC, Windows a veces no deja abrir la captura aunque
    // el cable este bien; al rato si. Antes habia que reiniciar el programa o
    // tocar la salida de Windows para que arrancara. Ahora se reintenta solo
    // durante un minuto; despues se espera a que se conecte algun dispositivo.
    constexpr int kMaxTries   = 20;
    constexpr int kIntervalMs = 3000;

    QString error;
    const bool started = m_systemAudio->start(Settings::systemAudioSource(),
                                              Settings::systemAudioOutput(), &error);

    // La musica del propio reproductor sale por la salida que se captura: si
    // la captura funciona, la ecualiza ella y aqui no se toca.
    m_engine->setChainBypassed(started);

    if (!started) {
        if (m_systemAudio->lastErrorRetryable() && m_systemAudioTries >= kMaxTries) {
            // Ya no se insiste con el reloj: lo normal es que la interfaz
            // este desenchufada. Se queda encendida y se reintenta en cuanto
            // Windows avise de que se conecto algo (onAudioDevicesChanged).
            Settings::setSystemAudioEnabled(true);
            if (m_settings) {
                m_settings->setSystemAudioStatus(
                    Lang::tr("%1 Se volvera a intentar al conectar un dispositivo de audio.")
                        .arg(error), true);
            }
            return;
        }

        if (m_systemAudio->lastErrorRetryable()) {
            ++m_systemAudioTries;
            if (!m_systemAudioRetry) {
                m_systemAudioRetry = new QTimer(this);
                m_systemAudioRetry->setSingleShot(true);
                m_systemAudioRetry->setInterval(kIntervalMs);
                connect(m_systemAudioRetry, &QTimer::timeout,
                        this, &MainWindow::startSystemAudio);
            }
            m_systemAudioRetry->start();

            // Sigue encendida mientras tanto: si se cierra el programa a
            // medias, la proxima vez lo vuelve a intentar.
            Settings::setSystemAudioEnabled(true);
            if (m_settings) {
                m_settings->setSystemAudioStatus(
                    Lang::tr("%1 Reintentando en unos segundos...").arg(error), true);
            }
            return;
        }

        m_systemAudioTries = 0;
        Settings::setSystemAudioEnabled(false);
        if (m_settings)
            m_settings->setSystemAudioStatus(error, false);
        return;
    }

    m_systemAudioTries = 0;
    Settings::setSystemAudioEnabled(true);
    if (m_settings) {
        m_settings->setSystemAudioStatus(
            Lang::tr("Procesando el audio del sistema."), true);
    }
}

void MainWindow::onAudioDevicesChanged()
{
    const QList<QByteArray> before = m_presentOutputs;
    m_presentOutputs = connectedOutputIds();

    // Con la captura encendida se sigue a las interfaces: la que se acaba de
    // conectar se lleva el sonido y, si se va la que se usaba, pasa a otra.
    if (Settings::systemAudioEnabled()) {
        const QByteArray target = chooseInterface(m_presentOutputs, before);
        if (!target.isEmpty())
            applySystemAudio(true, Settings::systemAudioSource(), target);
    }

    if (m_settings)
        m_settings->refreshSystemDevices();

    // Captura encendida pero parada: o esperaba a una interfaz que quiza
    // acaba de llegar, o se le acaba de ir la suya. En los dos casos se
    // vuelve a intentar desde cero.
    if (m_systemAudio && Settings::systemAudioEnabled() && !m_systemAudio->isRunning()) {
        if (m_systemAudioRetry)
            m_systemAudioRetry->stop();
        m_systemAudioTries = 0;
        startSystemAudio();
    }
}

bool MainWindow::nativeEvent(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    const auto* msg = static_cast<const MSG*>(message);

    // Arrastre o cambio de tamano hecho por Windows (startSystemMove y
    // startSystemResize). Al soltar se comprueba que la barra de titulo no
    // haya quedado debajo de una barra de tareas puesta arriba.
    if (msg->message == WM_ENTERSIZEMOVE) {
        m_sizeMoveStart = frameGeometry().size();
    } else if (msg->message == WM_EXITSIZEMOVE) {
        const bool resized = frameGeometry().size() != m_sizeMoveStart;
        // Arrastrar una ventana maximizada a mano la saca del maximizado.
        if (m_manualMaximized && geometry() != workArea()) {
            m_manualMaximized = false;
            m_titleBar->setMaximized(isWindowMaximized());
        }
        keepTitleBarReachable(resized);
    }

    if (msg->message == WM_DEVICECHANGE && msg->wParam == DBT_DEVNODES_CHANGED) {
        // Llegan varios avisos seguidos por cada aparato, y Windows tarda un
        // poco mas en dar de alta sus salidas de audio: se espera a que se
        // calme antes de mirar.
        if (!m_deviceChange) {
            m_deviceChange = new QTimer(this);
            m_deviceChange->setSingleShot(true);
            m_deviceChange->setInterval(1500);
            connect(m_deviceChange, &QTimer::timeout,
                    this, &MainWindow::onAudioDevicesChanged);
        }
        m_deviceChange->start();
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::restartForLanguage()
{
    const auto answer = QMessageBox::question(
        this, QStringLiteral("Ryxus Player"),
        Lang::tr("Se reiniciara Ryxus Player para aplicar el idioma.\n"
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

    // Cada opcion muestra a la derecha su atajo vigente, el que se haya
    // elegido en Configuracion. Va como texto tras un tabulador y no como
    // atajo del QAction: si no, con el menu abierto la tecla se dispararia
    // dos veces (aqui y en el atajo global).
    const auto withKeys = [](const QString& text, int command) {
        const QString keys = Commands::keysText(command);
        return keys.isEmpty() ? text : text + QLatin1Char('	') + keys;
    };
    const auto addCommand = [this, &withKeys](QMenu* target, int command,
                                              const QIcon& icon = QIcon(),
                                              const QString& text = QString()) {
        const QString label = text.isEmpty() ? Lang::tr(Commands::info(command).label) : text;
        QAction* action = target->addAction(icon, withKeys(label, command));
        connect(action, &QAction::triggered, this, [this, command]() { runCommand(command); });
        return action;
    };
    const auto addSubmenu = [&](QMenu* parent, Icons::Icon icon, const QString& title) {
        QMenu* sub = icon == Icons::None ? parent->addMenu(title)
                                         : parent->addMenu(Icons::icon(icon, iconColor), title);
        sub->setFont(Theme::uiFont(9));
        return sub;
    };

    addCommand(&menu, Commands::OpenFiles, Icons::icon(Icons::Plus, iconColor));
    addCommand(&menu, Commands::OpenFolder, Icons::icon(Icons::FolderOpen, iconColor));

    menu.addSeparator();

    QAction* eqAction = addCommand(&menu, Commands::EqualizerPanel,
                                   Icons::icon(Icons::Equalizer, iconColor));
    eqAction->setCheckable(true);
    eqAction->setChecked(!m_docks[DockEqualizer]->isHidden());

    QAction* eqOnAction = addCommand(&menu, Commands::EqualizerOnOff);
    eqOnAction->setCheckable(true);
    eqOnAction->setChecked(Settings::eqEnabled());

    // --- submenu de reproduccion -------------------------------------------
    QMenu* playback = addSubmenu(&menu, Icons::Play, Lang::tr("Reproduccion"));
    addCommand(playback, Commands::PlayPause);
    addCommand(playback, Commands::Stop);
    addCommand(playback, Commands::Next);
    addCommand(playback, Commands::Previous);
    playback->addSeparator();
    addCommand(playback, Commands::VolumeUp);
    addCommand(playback, Commands::VolumeDown);
    QAction* muteAction = addCommand(playback, Commands::Mute);
    muteAction->setCheckable(true);
    muteAction->setChecked(m_engine->isMuted());
    playback->addSeparator();
    QAction* shuffleAction = addCommand(playback, Commands::Shuffle);
    shuffleAction->setCheckable(true);
    shuffleAction->setChecked(m_shuffle);
    addCommand(playback, Commands::Repeat);
    addCommand(playback, Commands::AbLoop);
    playback->addSeparator();
    addCommand(playback, Commands::SeekForward);
    addCommand(playback, Commands::SeekBackward);
    fillStepMenus(playback);

    menu.addSeparator();

    // --- submenu de paneles ------------------------------------------------
    QMenu* panels = addSubmenu(&menu, Icons::Grip, Lang::tr("Paneles"));

    const struct { int dock; int command; } kPanelCommands[] = {
        {DockNowPlaying, Commands::PanelNowPlaying},
        {DockFiles,      Commands::PanelFiles},
        {DockPlaylist,   Commands::PanelPlaylist},
        {DockEqualizer,  Commands::EqualizerPanel},
    };
    for (const auto& entry : kPanelCommands) {
        PanelDock* dock = m_docks[entry.dock];
        QAction* action = addCommand(panels, entry.command, QIcon(), dock->windowTitle());
        action->setCheckable(true);
        action->setChecked(!dock->isHidden());
    }

    QAction* filtersAction = addCommand(panels, Commands::VideoFilters, QIcon(),
                                        m_docks[DockVideoFilters]->windowTitle());
    filtersAction->setCheckable(true);
    filtersAction->setChecked(!m_docks[DockVideoFilters]->isHidden());

    // El de video no tiene atajo propio: lo trae solo abrir un video.
    PanelDock* videoDock = m_docks[DockVideo];
    QAction* videoAction = panels->addAction(videoDock->windowTitle());
    videoAction->setCheckable(true);
    videoAction->setChecked(!videoDock->isHidden());
    connect(videoAction, &QAction::toggled, this, [this](bool on) {
        if (m_docks[DockVideo]->isHidden() == on)
            togglePanel(DockVideo);
    });

    panels->addSeparator();
    addCommand(panels, Commands::ShowAllPanels);
    addCommand(panels, Commands::HideAllPanels);

    // --- submenu de disposiciones ------------------------------------------
    QMenu* layouts = addSubmenu(&menu, Icons::Grip, Lang::tr("Disposicion de paneles"));
    addCommand(layouts, Commands::NextLayout);
    addCommand(layouts, Commands::PreviousLayout);
    layouts->addSeparator();

    // Puntos de partida: cada usuario elige el que mas se le parece y lo
    // ajusta a su gusto.
    const int kPresetCommands[LayoutPresetCount] = {
        Commands::LayoutClassic, Commands::LayoutLibrary,
        Commands::LayoutListening, Commands::LayoutStudio,
    };
    for (int preset = 0; preset < LayoutPresetCount; ++preset)
        addCommand(layouts, kPresetCommands[preset], QIcon(), layoutPresetName(preset));

    const QList<Settings::SavedLayout> saved = Settings::savedLayouts();
    if (!saved.isEmpty()) {
        layouts->addSeparator();
        for (const Settings::SavedLayout& layout : saved) {
            const QString name = layout.name;
            layouts->addAction(name, this, [this, name]() { applySavedLayout(name); });
        }
    }

    layouts->addSeparator();
    addCommand(layouts, Commands::SaveLayout, Icons::icon(Icons::Save, iconColor));

    QMenu* removeMenu = addSubmenu(layouts, Icons::Trash,
                                   Lang::tr("Eliminar una disposicion guardada"));
    removeMenu->setEnabled(!saved.isEmpty());
    for (const Settings::SavedLayout& layout : saved) {
        const QString name = layout.name;
        removeMenu->addAction(name, this, [name]() { Settings::removeLayout(name); });
    }

    menu.addSeparator();

    // --- video: menu propio, aparte de las opciones de audio --------------
    QMenu* videoMenu = addSubmenu(&menu, Icons::Image, Lang::tr("Video"));
    fillVideoMenu(videoMenu);

    menu.addSeparator();

    addCommand(&menu, Commands::OpenSettings, Icons::icon(Icons::Settings, iconColor));
    menu.addAction(Lang::tr("Atajos de teclado..."), this, [this]() {
        openSettings();
        m_settings->showShortcuts();
    });

    QMenu* background = menu.addMenu(Icons::icon(Icons::Image, iconColor),
                                     QStringLiteral("Fondo"));
    background->setFont(Theme::uiFont(9));

    addCommand(background, Commands::ChooseBackground, QIcon(), Lang::tr("Elegir imagen..."));
    QAction* clearAction = addCommand(background, Commands::ClearBackground, QIcon(),
                                      Lang::tr("Quitar imagen"));
    clearAction->setEnabled(m_background->hasImage());

    background->addSeparator();

    // Submenu de la capa viva: las mismas visualizaciones que en Configuracion,
    // a mano desde el menu principal.
    QMenu* visualMenu = background->addMenu(Lang::tr("Visualizacion"));
    visualMenu->setFont(Theme::uiFont(9));
    auto* visualGroup = new QActionGroup(visualMenu);
    visualGroup->setExclusive(true);

    const auto addVisual = [&](int index, const QString& label) {
        QAction* action = visualMenu->addAction(label);
        action->setCheckable(true);
        action->setChecked(m_background->visualization() == index);
        visualGroup->addAction(action);
        connect(action, &QAction::triggered, this, [this, index]() {
            setBackgroundVisualization(index);
        });
    };

    addCommand(visualMenu, Commands::NextVisualization);
    visualMenu->addSeparator();
    addVisual(-1, Lang::tr("Ninguna"));
    visualMenu->addSeparator();
    for (int i = 0; i < Visualizations::count(); ++i)
        addVisual(i, Lang::tr(Visualizations::info(i).name));

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

    // Atajo al desinstalador: aparece solo si el programa se instalo de
    // verdad. Es la via corta cuando alguien no da con la entrada de
    // "Aplicaciones instaladas" de Windows.
    if (!uninstallerPath().isEmpty()) {
        menu.addAction(Icons::icon(Icons::Trash, iconColor),
                       Lang::tr("Desinstalar Ryxus Player..."),
                       this, &MainWindow::runUninstaller);
        menu.addSeparator();
    }

    QMenu* windowMenu = addSubmenu(&menu, Icons::None, Lang::tr("Ventana"));
    addCommand(windowMenu, Commands::ToggleMaximize);
    addCommand(windowMenu, Commands::MinimizeWindow);

    menu.addSeparator();

    addCommand(&menu, Commands::Quit, Icons::icon(Icons::Close, iconColor));

    menu.exec(globalPos);
}

// -------------------------------------------------------------------- ventana

void MainWindow::toggleMaximized()
{
    // Una ventana sin marco maximizada con showMaximized() tapa la barra de
    // tareas: Windows solo le descuenta el area de trabajo a las ventanas que
    // llevan marco. Se maximiza a mano contra availableGeometry(), que ya
    // descuenta la barra este donde este -- arriba, abajo o a un lado -- y del
    // grosor que sea.
    if (isMaximized()) {
        showNormal();                 // veniamos de un maximizado del sistema
    } else if (m_manualMaximized) {
        setGeometry(m_restoreGeometry);
        m_manualMaximized = false;
    } else if (screen()) {
        m_restoreGeometry = geometry();
        setGeometry(workArea());
        m_manualMaximized = true;
    }

    m_titleBar->setMaximized(isWindowMaximized());
}

QRect MainWindow::workArea() const
{
    QScreen* target = screen();
    if (!target)
        return geometry();

    QRect work = target->availableGeometry();

#ifdef Q_OS_WIN
    // Barra de tareas que se oculta sola: no reserva sitio, asi que el area
    // util es el monitor entero. Pero una ventana que lo cubre entero cuenta
    // para Windows como pantalla completa, y entonces la barra ya no asoma al
    // llevar el raton al borde. Se deja libre una linea en ese borde.
    if (work == target->geometry()) {
        HMONITOR monitor = MonitorFromWindow(reinterpret_cast<HWND>(winId()),
                                             MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{};
        info.cbSize = sizeof(info);
        if (monitor && GetMonitorInfoW(monitor, &info)) {
            const auto autoHideOn = [&](UINT edge) {
                APPBARDATA bar{};
                bar.cbSize = sizeof(bar);
                bar.uEdge  = edge;
                bar.rc     = info.rcMonitor;
                return SHAppBarMessage(ABM_GETAUTOHIDEBAREX, &bar) != 0;
            };
            if (autoHideOn(ABE_TOP))    work.setTop(work.top() + 1);
            if (autoHideOn(ABE_BOTTOM)) work.setBottom(work.bottom() - 1);
            if (autoHideOn(ABE_LEFT))   work.setLeft(work.left() + 1);
            if (autoHideOn(ABE_RIGHT))  work.setRight(work.right() - 1);
        }
    }
#endif
    return work;
}

void MainWindow::keepTitleBarReachable(bool resized)
{
    QScreen* target = screen();
    if (!target)
        return;

    // Solo importa si arriba hay algo que tape: una barra de tareas (o
    // cualquier otra barra) en el borde superior de este monitor.
    const QRect work = workArea();
    if (work.top() <= target->geometry().top())
        return;

    QRect frame = frameGeometry();
    if (frame.top() >= work.top())
        return;

    // La barra de titulo propia es la unica forma de mover la ventana: si
    // queda debajo de la barra de tareas no hay como agarrarla. Al moverla se
    // baja entera; al estirarla por arriba se recorta ese borde, para no
    // empujar el de abajo fuera de la pantalla.
    if (resized)
        frame.setTop(work.top());
    else
        frame.moveTop(work.top());
    setGeometry(frame);
}

void MainWindow::changeEvent(QEvent* event)
{
    QMainWindow::changeEvent(event);

    if (event->type() != QEvent::WindowStateChange || !isMaximized() || isFullScreen())
        return;

    // Windows la maximizo por su cuenta: arrastrandola al borde de arriba
    // (Aero Snap) o con Win+Flecha arriba. Sin marco, ese maximizado ocupa
    // tambien el sitio de la barra de tareas, la de arriba incluida. Se
    // cambia por el propio, que respeta el area util. En diferido: no se
    // puede cambiar el estado de la ventana mientras se avisa de el.
    QTimer::singleShot(0, this, [this]() {
        if (!isMaximized())
            return;
        const QRect normal = normalGeometry();
        showNormal();
        m_restoreGeometry = normal.isValid() ? normal : geometry();
        setGeometry(workArea());
        m_manualMaximized = true;
        m_titleBar->setMaximized(true);
    });
}

void MainWindow::moveEvent(QMoveEvent* event)
{
    QMainWindow::moveEvent(event);
    // Mientras se decide la combinacion nueva no se apunta nada: es Windows
    // quien esta moviendo la ventana, no el usuario.
    if (m_placementSave && !m_screenSetup->isActive())
        m_placementSave->start();
}

void MainWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    if (m_placementSave && !m_screenSetup->isActive())
        m_placementSave->start();
}

void MainWindow::saveWindowPlacement()
{
    if (m_screenKey.isEmpty() || isMinimized() || isFullScreen())
        return;

    Settings::WindowPlacement placement;
    placement.geometry  = saveGeometry();
    placement.maximized = m_manualMaximized;
    placement.normal    = m_manualMaximized ? m_restoreGeometry : QRect();
    Settings::setWindowPlacement(m_screenKey, placement);
}

bool MainWindow::restoreWindowPlacement()
{
    const Settings::WindowPlacement placement = Settings::windowPlacement(m_screenKey);
    if (placement.geometry.isEmpty())
        return false;

    m_manualMaximized = false;
    restoreGeometry(placement.geometry);
    if (placement.maximized && placement.normal.isValid()) {
        m_restoreGeometry = placement.normal;
        m_manualMaximized = true;
    }
    m_titleBar->setMaximized(isWindowMaximized());

    // Por si acaso: con la misma huella el area util puede variar un poco
    // (la barra de tareas cambiada de sitio, por ejemplo).
    clampIntoWorkArea();
    return true;
}

void MainWindow::fitMaximizedToWorkArea()
{
    // La geometria guardada de un maximizado es la del area util de entonces:
    // si desde aquel dia la barra de tareas cambio de borde o de tamano, se
    // quedaria una franja vacia o la ventana debajo de la barra. Se recalcula
    // siempre contra la de ahora, este donde este la barra.
    if (m_manualMaximized && !m_videoFullscreen && geometry() != workArea())
        setGeometry(workArea());
}

void MainWindow::watchScreen(QScreen* screen)
{
    // Mover la barra de tareas, cambiarla de tamano u ocultarla cambia el
    // area util sin cambiar de monitor: el maximizado se reajusta al momento.
    connect(screen, &QScreen::availableGeometryChanged, this,
            [this]() { fitMaximizedToWorkArea(); });

    // Cambiar la resolucion o la disposicion de un monitor tambien es otra
    // combinacion.
    connect(screen, &QScreen::geometryChanged, this, [this]() {
        m_placementSave->stop();
        m_screenSetup->start();
    });
}

void MainWindow::onScreenSetupChanged()
{
    // Aunque la combinacion acabe siendo la misma (un monitor que se apaga y
    // se enciende), Windows puede haber movido la ventana entretanto: se
    // devuelve a lo guardado igualmente. Primera vez con estos monitores: se
    // queda donde la dejo Windows, pero entera dentro del area util, y desde
    // aqui se apunta para ellos.
    m_screenKey = currentScreenKey();
    if (!restoreWindowPlacement())
        clampIntoWorkArea();
    fitMaximizedToWorkArea();
}

void MainWindow::clampIntoWorkArea()
{
    QScreen* target = screen();
    if (!target)
        return;

    const QRect work = workArea();
    QRect frame = frameGeometry();

    // La barra de titulo propia es la unica forma de mover, minimizar o cerrar
    // la ventana. Si la sesion anterior la dejo fuera del area util -- porque
    // la barra de tareas esta arriba, o porque cambio la resolucion -- quedaria
    // debajo de la barra y sin manera de agarrarla.
    frame.setWidth(qMin(frame.width(), work.width()));
    frame.setHeight(qMin(frame.height(), work.height()));

    if (frame.left() < work.left())     frame.moveLeft(work.left());
    if (frame.top() < work.top())       frame.moveTop(work.top());
    if (frame.right() > work.right())   frame.moveRight(work.right());
    if (frame.bottom() > work.bottom()) frame.moveBottom(work.bottom());

    if (frame != frameGeometry())
        setGeometry(frame);
}

QString MainWindow::layoutPresetName(int preset)
{
    switch (preset) {
    case LayoutClassic:   return Lang::tr("Clasica");
    case LayoutLibrary:   return Lang::tr("Biblioteca");
    case LayoutListening: return Lang::tr("Escucha");
    case LayoutStudio:    return Lang::tr("Estudio");
    }
    return QString();
}

void MainWindow::applyLayoutPreset(int preset)
{
    m_layoutCycle = preset;   // "disposicion siguiente" sigue desde aqui

    // Se parte de cero: todos acoplados y fuera de la ventana, y se vuelven
    // a meter en el orden de la disposicion. Todo va en una sola zona,
    // anidando divisiones: es lo que permite cualquier reparto sin depender
    // de las esquinas de QMainWindow.
    for (PanelDock* dock : m_docks) {
        dock->setFloating(false);
        m_dockHost->removeDockWidget(dock);
    }

    PanelDock* now   = m_docks[DockNowPlaying];
    PanelDock* files = m_docks[DockFiles];
    PanelDock* list  = m_docks[DockPlaylist];
    PanelDock* eq    = m_docks[DockEqualizer];

    // El primero de cada disposicion entra en la zona; los demas se
    // reparten partiendo su sitio.
    const auto start = [this](PanelDock* first) {
        m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, first);
    };

    // Los paneles que una disposicion no muestra tambien se colocan: al
    // pedirlos luego desde el menu aparecen en un sitio con sentido.
    switch (preset) {
    case LayoutLibrary:
        // [ Archivos | Reproduciendo / Lista ]
        start(files);
        m_dockHost->splitDockWidget(files, now, Qt::Horizontal);
        m_dockHost->splitDockWidget(now, list, Qt::Vertical);
        m_dockHost->splitDockWidget(files, eq, Qt::Vertical);
        for (PanelDock* dock : {now, files, list})
            dock->show();
        eq->hide();
        m_dockHost->resizeDocks({files, now}, {1000, 380}, Qt::Horizontal);
        m_dockHost->resizeDocks({now, list}, {420, 400}, Qt::Vertical);
        break;

    case LayoutListening:
        // [ Reproduciendo | Lista ] / Ecualizador, con Archivos en una
        // pestana junto a la lista.
        start(now);
        m_dockHost->splitDockWidget(now, eq, Qt::Vertical);
        m_dockHost->splitDockWidget(now, list, Qt::Horizontal);
        m_dockHost->tabifyDockWidget(list, files);
        for (PanelDock* dock : m_docks)
            dock->show();
        list->raise();
        m_dockHost->resizeDocks({now, list}, {520, 900}, Qt::Horizontal);
        m_dockHost->resizeDocks({now, eq}, {560, 240}, Qt::Vertical);
        break;

    case LayoutStudio:
        // Ecualizador a lo ancho y alto, para trabajar el sonido; arriba
        // Reproduciendo y la lista, con Archivos en pestana.
        start(now);
        m_dockHost->splitDockWidget(now, eq, Qt::Vertical);
        m_dockHost->splitDockWidget(now, list, Qt::Horizontal);
        m_dockHost->tabifyDockWidget(list, files);
        for (PanelDock* dock : m_docks)
            dock->show();
        list->raise();
        m_dockHost->resizeDocks({now, list}, {Theme::Metrics::LeftPanelWidth, 1200},
                                Qt::Horizontal);
        m_dockHost->resizeDocks({now, eq}, {360, 440}, Qt::Vertical);
        break;

    case LayoutClassic:
    default:
        // [ Reproduciendo | Archivos | Lista ], con el ecualizador bajo
        // Archivos y Lista, cerrado hasta que se pida.
        start(now);
        m_dockHost->splitDockWidget(now, files, Qt::Horizontal);
        m_dockHost->splitDockWidget(files, eq, Qt::Vertical);
        m_dockHost->splitDockWidget(files, list, Qt::Horizontal);
        for (PanelDock* dock : {now, files, list})
            dock->show();
        eq->hide();
        m_dockHost->resizeDocks({now, files, list},
                                {Theme::Metrics::LeftPanelWidth, 940,
                                 Theme::Metrics::RightPanelWidth}, Qt::Horizontal);
        m_dockHost->resizeDocks({files, eq}, {560, 240}, Qt::Vertical);
        break;
    }

    // Los de video se colocan tambien, ocultos: estas son las de audio.
    m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, m_docks[DockVideo]);
    m_docks[DockVideo]->hide();
    m_dockHost->splitDockWidget(m_docks[DockVideo], m_docks[DockVideoFilters], Qt::Vertical);
    m_docks[DockVideoFilters]->hide();

    m_centerSplitter->setSizes({Theme::Metrics::TreePanelWidth, 700});
}

void MainWindow::applySavedLayout(const QString& name)
{
    const QList<Settings::SavedLayout> saved = Settings::savedLayouts();
    for (int i = 0; i < saved.size(); ++i) {
        if (saved.at(i).name == name) {
            m_dockHost->restoreState(saved.at(i).state, kDockStateVersion);
            m_layoutCycle = LayoutPresetCount + i;
            return;
        }
    }
}

void MainWindow::saveCurrentLayout()
{
    bool ok = false;
    const QString name = QInputDialog::getText(
        this, Lang::tr("Guardar disposicion"),
        Lang::tr("Nombre de la disposicion:"), QLineEdit::Normal, QString(), &ok).trimmed();
    if (!ok || name.isEmpty())
        return;

    // Un nombre repetido reemplaza a la anterior, como con los presets del
    // ecualizador; se avisa antes para no pisarla sin querer.
    for (const Settings::SavedLayout& layout : Settings::savedLayouts()) {
        if (layout.name != name)
            continue;
        const auto answer = QMessageBox::question(
            this, QStringLiteral("Ryxus Player"),
            Lang::tr("Ya hay una disposicion llamada \"%1\". Reemplazarla?").arg(name));
        if (answer != QMessageBox::Yes)
            return;
        break;
    }

    Settings::saveLayout(name, m_dockHost->saveState(kDockStateVersion));
}

void MainWindow::setAllPanelsVisible(bool visible)
{
    // Ocultarlos todos deja a la vista el fondo de la ventana entero. El de
    // video solo se muestra si hay un video abierto.
    for (PanelDock* dock : m_docks) {
        if (visible && !m_engine->hasVideo()
            && (dock == m_docks[DockVideo] || dock == m_docks[DockVideoFilters]))
            continue;
        dock->setVisible(visible);
    }
}

void MainWindow::togglePanel(int dock)
{
    PanelDock* panel = m_docks[dock];
    const bool show = panel->isHidden();
    panel->setVisible(show);
    // Si esta en pestanas con otro, mostrarlo es tambien traerlo delante.
    if (show)
        panel->raise();
}

void MainWindow::cycleLayout(int step)
{
    // Recorre en orden las de fabrica y despues las guardadas, y vuelve a
    // empezar. Se avisa del nombre: con el teclado no se ve que menu se usa.
    const QList<Settings::SavedLayout> saved = Settings::savedLayouts();
    const int total = LayoutPresetCount + int(saved.size());
    m_layoutCycle = ((m_layoutCycle + step) % total + total) % total;

    QString name;
    if (m_layoutCycle < LayoutPresetCount) {
        applyLayoutPreset(m_layoutCycle);
        name = layoutPresetName(m_layoutCycle);
    } else {
        name = saved.at(m_layoutCycle - LayoutPresetCount).name;
        applySavedLayout(name);
    }
    showCommandToast(Lang::tr("Disposicion: %1").arg(name));
}

// ------------------------------------------------------------------- video

QString MainWindow::videoLayoutName(int layout)
{
    switch (layout) {
    case VideoCinema:        return Lang::tr("Solo la imagen");
    case VideoWithPlaylist:  return Lang::tr("Imagen y lista");
    case VideoWithEqualizer: return Lang::tr("Imagen y banco de filtros");
    case VideoStudio:        return Lang::tr("Estudio");
    }
    return QString();
}

void MainWindow::applyVideoLayout(int layout)
{
    Settings::setVideoLayout(layout);

    for (PanelDock* dock : m_docks) {
        dock->setFloating(false);
        m_dockHost->removeDockWidget(dock);
    }

    PanelDock* video = m_docks[DockVideo];
    PanelDock* now   = m_docks[DockNowPlaying];
    PanelDock* files = m_docks[DockFiles];
    PanelDock* list  = m_docks[DockPlaylist];
    PanelDock* eq    = m_docks[DockEqualizer];
    PanelDock* filters = m_docks[DockVideoFilters];

    // La imagen manda: entra la primera y los demas se reparten su sitio.
    m_dockHost->addDockWidget(Qt::LeftDockWidgetArea, video);
    QList<PanelDock*> shown{video};

    // Qt no reparte tamanos entre paneles ocultos: se muestran antes de
    // llamar a resizeDocks, o los anchos y altos pedidos no se aplican.
    const auto showShown = [&shown]() {
        for (PanelDock* dock : std::as_const(shown))
            dock->show();
    };

    switch (layout) {
    case VideoWithPlaylist:
        // La paleta de filtros, bajo la lista.
        m_dockHost->splitDockWidget(video, list, Qt::Horizontal);
        m_dockHost->splitDockWidget(list, filters, Qt::Vertical);
        shown << list << filters;
        showShown();
        m_dockHost->resizeDocks({video, list}, {1200, Theme::Metrics::RightPanelWidth},
                                Qt::Horizontal);
        break;

    case VideoWithEqualizer:
        // El banco de filtros debajo, con la paleta de filtros de imagen a su
        // derecha: el sonido y el aspecto del video, uno al lado del otro.
        m_dockHost->splitDockWidget(video, eq, Qt::Vertical);
        m_dockHost->splitDockWidget(eq, filters, Qt::Horizontal);
        shown << eq << filters;
        showShown();
        m_dockHost->resizeDocks({video, eq}, {560, 240}, Qt::Vertical);
        // El ecualizador no pasa de ~900 px con el rack cerrado: el resto,
        // para que la paleta muestre mas miniaturas por fila.
        m_dockHost->resizeDocks({eq, filters}, {920, 1080}, Qt::Horizontal);
        break;

    case VideoStudio:
        // [ imagen / banco de filtros + paleta | reproduciendo / lista ]
        m_dockHost->splitDockWidget(video, now, Qt::Horizontal);
        m_dockHost->splitDockWidget(video, eq, Qt::Vertical);
        m_dockHost->splitDockWidget(eq, filters, Qt::Horizontal);
        m_dockHost->splitDockWidget(now, list, Qt::Vertical);
        shown << now << list << eq << filters;
        showShown();
        m_dockHost->resizeDocks({video, now}, {1150, 380}, Qt::Horizontal);
        m_dockHost->resizeDocks({video, eq}, {520, 240}, Qt::Vertical);
        m_dockHost->resizeDocks({eq, filters}, {700, 450}, Qt::Horizontal);
        break;

    case VideoCinema:
    default:
        // La imagen y, debajo, una tira con la paleta de filtros: lo minimo
        // para cambiar el aspecto sin ir a buscarlo.
        m_dockHost->splitDockWidget(video, filters, Qt::Vertical);
        shown << filters;
        showShown();
        m_dockHost->resizeDocks({video, filters}, {900, 150}, Qt::Vertical);
        break;
    }

    // Cerrada por el usuario: se coloca, pero no se muestra.
    if (!Settings::videoFiltersVisible()) {
        shown.removeAll(filters);
        filters->hide();
    }

    // Los que no salen se colocan igualmente, ocultos, en una pestana junto
    // a la imagen: pedirlos luego desde el menu los trae a un sitio sensato.
    for (PanelDock* dock : {now, files, list, eq}) {
        if (shown.contains(dock))
            continue;
        m_dockHost->tabifyDockWidget(video, dock);
        dock->hide();
    }
    for (PanelDock* dock : std::as_const(shown))
        dock->show();
    video->raise();
}

void MainWindow::onVideoActiveChanged()
{
    const bool active = m_engine->hasVideo();
    if (active) {
        // La disposicion de audio se guarda una sola vez, al entrar: entre
        // un video y el siguiente ya estamos en la de video.
        if (!m_inVideoMode) {
            m_audioLayoutState = m_dockHost->saveState(kDockStateVersion);
            m_inVideoMode = true;
        }
        applyVideoLayout(Settings::videoLayout());
        // La mascara se recuerda de un video a otro: que no pase desapercibida
        // (el modo cine tapa arriba y abajo y parece un video recortado).
        if (Settings::videoMask() != VideoMasks::None)
            m_video->showNotice(Lang::tr("Mascara: %1  -  Alt+M para cambiarla")
                                    .arg(VideoMasks::name(Settings::videoMask())), 4000);
        // El teclado al video: las flechas avanzan y cambian el volumen sin
        // que el foco se quede en una lista que ahora no se ve.
        m_video->setFocus(Qt::OtherFocusReason);
        return;
    }

    m_video->clear();
    if (!m_inVideoMode)
        return;
    m_inVideoMode = false;
    if (m_videoFullscreen)
        toggleVideoFullscreen();
    m_dockHost->restoreState(m_audioLayoutState, kDockStateVersion);
    m_docks[DockVideo]->hide();
    m_docks[DockVideoFilters]->hide();
}

void MainWindow::fillVideoMenu(QMenu* menu)
{
    const QColor iconColor = Theme::TextDim;
    const auto withKeys = [](const QString& text, int command) {
        const QString keys = Commands::keysText(command);
        return keys.isEmpty() ? text : text + QLatin1Char('\t') + keys;
    };
    const auto addCommand = [this, &withKeys](QMenu* target, int command,
                                              const QString& text) {
        QAction* action = target->addAction(withKeys(text, command));
        connect(action, &QAction::triggered, this, [this, command]() { runCommand(command); });
        return action;
    };

    const bool video = m_engine->hasVideo();

    // --- idioma de audio -----------------------------------------------------
    QMenu* audio = menu->addMenu(Icons::icon(Icons::Volume, iconColor),
                                 Lang::tr("Idioma de audio"));
    audio->setFont(Theme::uiFont(9));
    {
        auto* group = new QActionGroup(audio);
        const QStringList tracks = m_engine->audioTrackNames();
        const QString external = m_engine->externalAudio();
        const int active = external.isEmpty() ? m_engine->activeAudioTrack() : -1;
        for (int i = 0; i < tracks.size(); ++i) {
            QAction* action = audio->addAction(tracks.at(i));
            action->setCheckable(true);
            action->setChecked(i == active);
            group->addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() { m_engine->setAudioTrack(i); });
        }
        if (!external.isEmpty()) {
            QAction* action = audio->addAction(
                Lang::tr("Pista externa: %1").arg(QFileInfo(external).fileName()));
            action->setCheckable(true);
            action->setChecked(true);
            group->addAction(action);
        }
        if (tracks.isEmpty() && external.isEmpty())
            audio->addAction(Lang::tr("El video no trae pistas de audio"))->setEnabled(false);

        audio->addSeparator();
        addCommand(audio, Commands::AddAudioTrack, Lang::tr("Anadir pista de audio..."))
            ->setEnabled(video);
        if (!external.isEmpty()) {
            audio->addAction(Lang::tr("Quitar la pista externa"), this,
                             [this]() { m_engine->clearExternalAudio(); });
        }
    }

    // --- subtitulos ------------------------------------------------------------
    QMenu* subtitles = menu->addMenu(Icons::icon(Icons::Tag, iconColor),
                                     Lang::tr("Subtitulos"));
    subtitles->setFont(Theme::uiFont(9));
    {
        auto* group = new QActionGroup(subtitles);
        const QString file = m_video->subtitlesPath();
        const int active = file.isEmpty() ? m_engine->activeSubtitleTrack() : -2;

        QAction* off = subtitles->addAction(Lang::tr("Desactivados"));
        off->setCheckable(true);
        off->setChecked(active == -1);
        group->addAction(off);
        connect(off, &QAction::triggered, this, [this]() {
            m_video->clearSubtitles();
            m_engine->setSubtitleTrack(-1);
        });

        const QStringList tracks = m_engine->subtitleTrackNames();
        for (int i = 0; i < tracks.size(); ++i) {
            QAction* action = subtitles->addAction(tracks.at(i));
            action->setCheckable(true);
            action->setChecked(i == active);
            group->addAction(action);
            connect(action, &QAction::triggered, this, [this, i]() {
                m_video->clearSubtitles();
                m_engine->setSubtitleTrack(i);
            });
        }
        if (!file.isEmpty()) {
            QAction* action = subtitles->addAction(
                Lang::tr("Archivo: %1").arg(QFileInfo(file).fileName()));
            action->setCheckable(true);
            action->setChecked(true);
            group->addAction(action);
        }

        subtitles->addSeparator();
        addCommand(subtitles, Commands::AddSubtitles, Lang::tr("Anadir subtitulos..."))
            ->setEnabled(video);
        subtitles->addAction(Lang::tr("Estilo de subtitulos..."), this, [this]() {
            openSettings();
            m_settings->showSubtitleStyle();
        });
    }

    menu->addSeparator();

    // --- disposiciones de video ------------------------------------------
    QMenu* layouts = menu->addMenu(Icons::icon(Icons::Grip, iconColor),
                                   Lang::tr("Disposicion de video"));
    layouts->setFont(Theme::uiFont(9));
    const int kLayoutCommands[VideoLayoutCount] = {
        Commands::VideoCinema, Commands::VideoWithPlaylist,
        Commands::VideoWithEqualizer, Commands::VideoStudio,
    };
    const int current = Settings::videoLayout();
    for (int layout = 0; layout < VideoLayoutCount; ++layout) {
        QAction* action = addCommand(layouts, kLayoutCommands[layout], videoLayoutName(layout));
        action->setCheckable(true);
        action->setChecked(layout == current);
    }

    // --- banco de filtros sobre el audio del video -------------------------
    QAction* filters = addCommand(menu, Commands::EqualizerPanel,
                                  Lang::tr("Mostrar el banco de filtros (ecualizador y efectos)"));
    filters->setIcon(Icons::icon(Icons::Equalizer, iconColor));
    filters->setCheckable(true);
    filters->setChecked(!m_docks[DockEqualizer]->isHidden());

    QAction* palette = addCommand(menu, Commands::VideoFilters,
                                  Lang::tr("Mostrar la paleta de filtros de video"));
    palette->setCheckable(true);
    palette->setChecked(!m_docks[DockVideoFilters]->isHidden());

    menu->addSeparator();

    // --- mascaras de imagen -------------------------------------------------
    QMenu* masks = menu->addMenu(Icons::icon(Icons::Image, iconColor), Lang::tr("Mascara"));
    masks->setFont(Theme::uiFont(9));
    addCommand(masks, Commands::NextVideoMask, Lang::tr("Mascara siguiente"));
    masks->addSeparator();
    auto* group = new QActionGroup(masks);
    group->setExclusive(true);
    for (int mask = 0; mask < VideoMasks::Count; ++mask) {
        QAction* action = masks->addAction(VideoMasks::name(mask));
        action->setCheckable(true);
        action->setChecked(m_video->mask() == mask);
        group->addAction(action);
        connect(action, &QAction::triggered, this, [this, mask]() { setVideoMask(mask); });
    }

    // --- gestos en pantalla ------------------------------------------------
    QMenu* gestures = menu->addMenu(Icons::icon(Icons::ArrowRight, iconColor),
                                    Lang::tr("Gestos en pantalla"));
    gestures->setFont(Theme::uiFont(9));
    {
        const auto addToggle = [this, gestures](const QString& text, bool on,
                                                void (*save)(bool)) {
            QAction* action = gestures->addAction(text);
            action->setCheckable(true);
            action->setChecked(on);
            connect(action, &QAction::toggled, this, [this, save](bool value) {
                save(value);
                applyVideoGestureSettings();
            });
        };
        addToggle(Lang::tr("Arrastrar a los lados: avanzar y retroceder"),
                  Settings::videoGestureSeek(), &Settings::setVideoGestureSeek);
        addToggle(Lang::tr("Arrastrar arriba y abajo: volumen"),
                  Settings::videoGestureVolume(), &Settings::setVideoGestureVolume);
        addToggle(Lang::tr("Rueda del raton: avanzar y retroceder"),
                  Settings::videoGestureWheel(), &Settings::setVideoGestureWheel);
        gestures->addSeparator();
    }

    // --- sensibilidad del arrastre por fotogramas ----------------------------
    QMenu* scrub = gestures->addMenu(Icons::icon(Icons::ArrowRight, iconColor),
                                     Lang::tr("Sensibilidad de arrastre"));
    scrub->setFont(Theme::uiFont(9));
    QAction* hint = scrub->addAction(Lang::tr("Arrastra sobre la imagen para moverte por fotogramas"));
    hint->setEnabled(false);
    hint = scrub->addAction(Lang::tr("Cuanto mas largo el arrastre, mas rapido avanza"));
    hint->setEnabled(false);
    scrub->addSeparator();
    {
        const struct { int frames; const char* label; } kSteps[] = {
            {5,   "Muy fina: 5 fotogramas cada 100 px"},
            {15,  "Fina: 15 fotogramas cada 100 px"},
            {30,  "Media: 30 fotogramas cada 100 px"},
            {60,  "Normal: 60 fotogramas cada 100 px"},
            {150, "Rapida: 150 fotogramas cada 100 px"},
            {400, "Saltos largos: 400 fotogramas cada 100 px"},
        };
        auto* group = new QActionGroup(scrub);
        const int current = Settings::videoScrubFramesPer100();
        for (const auto& step : kSteps) {
            QAction* action = scrub->addAction(Lang::tr(step.label));
            action->setCheckable(true);
            action->setChecked(step.frames == current);
            group->addAction(action);
            const int frames = step.frames;
            connect(action, &QAction::triggered, this, [this, frames]() {
                Settings::setVideoScrubFramesPer100(frames);
                m_video->setScrubFramesPer100(frames);
            });
        }
    }

    // --- tamano de la previsualizacion ---------------------------------------
    QMenu* preview = gestures->addMenu(Lang::tr("Tamano de la previsualizacion"));
    preview->setFont(Theme::uiFont(9));
    {
        auto* group = new QActionGroup(preview);
        const int current = Settings::videoPreviewWidth();
        const struct { int width; const char* label; } kSizes[] = {
            {160, "Pequena"}, {260, "Mediana"}, {380, "Grande"}, {520, "Muy grande"},
        };
        for (const auto& size : kSizes) {
            QAction* action = preview->addAction(
                Lang::tr("%1 (%2 px)").arg(Lang::tr(size.label)).arg(size.width));
            action->setCheckable(true);
            action->setChecked(size.width == current);
            group->addAction(action);
            const int width = size.width;
            connect(action, &QAction::triggered, this, [this, width]() {
                Settings::setVideoPreviewWidth(width);
                applyVideoGestureSettings();
            });
        }
    }

    // --- barra de volumen: forma y color -------------------------------------
    QMenu* volumeBar = gestures->addMenu(Icons::icon(Icons::Volume, iconColor),
                                         Lang::tr("Barra de volumen"));
    volumeBar->setFont(Theme::uiFont(9));
    {
        auto* shapes = new QActionGroup(volumeBar);
        const int currentShape = Settings::volumeBarShape();
        for (int shape = 0; shape < VideoPanel::VolumeShapeCount; ++shape) {
            QAction* action = volumeBar->addAction(VideoPanel::volumeShapeName(shape));
            action->setCheckable(true);
            action->setChecked(shape == currentShape);
            shapes->addAction(action);
            connect(action, &QAction::triggered, this, [this, shape]() {
                Settings::setVolumeBarShape(shape);
                applyVideoGestureSettings();
                m_video->showVolumeOsd(m_engine->volume());   // se ve como queda
            });
        }
        volumeBar->addSeparator();

        auto* colors = new QActionGroup(volumeBar);
        const QString currentColor = Settings::volumeBarColor();
        const struct { const char* name; const char* label; } kColors[] = {
            {"", "Color del tema"}, {"#ffffff", "Blanco"}, {"#3ddc84", "Verde"},
            {"#4aa3ff", "Azul"}, {"#ff9f43", "Naranja"}, {"#ff5fa2", "Rosa"},
            {"#ff4d4d", "Rojo"},
        };
        bool matched = false;
        for (const auto& entry : kColors) {
            const QString name = QString::fromLatin1(entry.name);
            QAction* action = volumeBar->addAction(Lang::tr(entry.label));
            if (!name.isEmpty()) {
                QPixmap swatch(14, 14);
                swatch.fill(QColor(name));
                action->setIcon(QIcon(swatch));
            }
            action->setCheckable(true);
            const bool on = name.compare(currentColor, Qt::CaseInsensitive) == 0;
            matched = matched || on;
            action->setChecked(on);
            colors->addAction(action);
            connect(action, &QAction::triggered, this, [this, name]() {
                Settings::setVolumeBarColor(name);
                applyVideoGestureSettings();
                m_video->showVolumeOsd(m_engine->volume());
            });
        }
        QAction* custom = volumeBar->addAction(Lang::tr("Elegir otro color..."));
        custom->setCheckable(true);
        custom->setChecked(!matched);
        colors->addAction(custom);
        connect(custom, &QAction::triggered, this, [this]() {
            const QString current = Settings::volumeBarColor();
            const QColor chosen = QColorDialog::getColor(
                current.isEmpty() ? Theme::AccentBright : QColor(current), this,
                Lang::tr("Color de la barra de volumen"));
            if (!chosen.isValid())
                return;
            Settings::setVolumeBarColor(chosen.name());
            applyVideoGestureSettings();
            m_video->showVolumeOsd(m_engine->volume());
        });
    }

    gestures->addSeparator();
    fillStepMenus(gestures);

    menu->addSeparator();
    QAction* fullscreen = addCommand(menu, Commands::VideoFullscreen,
                                     Lang::tr("Pantalla completa"));
    fullscreen->setCheckable(true);
    fullscreen->setChecked(m_videoFullscreen);
}

void MainWindow::showVideoMenu(const QPoint& globalPos)
{
    QMenu menu(this);
    menu.setFont(Theme::uiFont(9));
    fillVideoMenu(&menu);
    menu.exec(globalPos);
}

void MainWindow::addAudioTrackDialog()
{
    if (!m_engine->hasVideo())
        return;
    const QString start = QFileInfo(m_engine->currentPath()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, Lang::tr("Anadir pista de audio"), start, TrackInfo::supportedFilter());
    if (path.isEmpty())
        return;
    if (m_engine->setExternalAudio(path))
        showCommandToast(Lang::tr("Pista de audio: %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::addSubtitlesDialog()
{
    if (!m_engine->hasVideo())
        return;
    const QString start = QFileInfo(m_engine->currentPath()).absolutePath();
    const QString path = QFileDialog::getOpenFileName(
        this, Lang::tr("Anadir subtitulos"), start, Subtitles::fileFilter());
    if (path.isEmpty())
        return;

    if (!m_video->loadSubtitles(path)) {
        QMessageBox::warning(this, QStringLiteral("Ryxus Player"),
                             Lang::tr("No se encontraron subtitulos en %1.")
                                 .arg(QFileInfo(path).fileName()));
        return;
    }
    // Los del propio video se apagan para no ver dos a la vez.
    m_engine->setSubtitleTrack(-1);
    showCommandToast(Lang::tr("Subtitulos: %1").arg(QFileInfo(path).fileName()));
}

void MainWindow::loadSidecarSubtitles(const QString& videoPath)
{
    // "pelicula.mkv" -> "pelicula.srt", "pelicula.es.srt", "pelicula.vtt"...
    const QFileInfo video(videoPath);
    const QDir dir = video.absoluteDir();
    const QStringList candidates = dir.entryList(
        {video.completeBaseName() + QStringLiteral("*.srt"),
         video.completeBaseName() + QStringLiteral("*.vtt"),
         video.completeBaseName() + QStringLiteral("*.ass"),
         video.completeBaseName() + QStringLiteral("*.ssa")},
        QDir::Files, QDir::Name);
    for (const QString& name : candidates) {
        if (m_video->loadSubtitles(dir.absoluteFilePath(name)))
            return;
    }
}

void MainWindow::setVideoMask(int mask)
{
    m_video->setMask(mask);
    m_videoFilters->setCurrentMask(mask);
    Settings::setVideoMask(mask);
    // En pantalla completa no hay paleta a la vista: se dice cual queda.
    m_video->showNotice(Lang::tr("Mascara: %1").arg(VideoMasks::name(mask)), 1800);
}

void MainWindow::setVideoFiltersVisible(bool visible)
{
    // Lo que se elige aqui se recuerda: las disposiciones de video la sacan o
    // no segun esto.
    Settings::setVideoFiltersVisible(visible);
    PanelDock* dock = m_docks[DockVideoFilters];
    dock->setVisible(visible);
    if (visible)
        dock->raise();
}

void MainWindow::toggleVideoFullscreen()
{
    if (!m_videoFullscreen) {
        // Solo la imagen, sin barras ni otros paneles, ocupando la pantalla.
        // Se apunta como estaba todo para dejarlo igual al salir (Esc).
        m_preFullscreenDocks = m_dockHost->saveState(kDockStateVersion);
        m_preFullscreenGeometry = geometry();
        m_videoFullscreen = true;

        for (PanelDock* dock : m_docks) {
            if (dock != m_docks[DockVideo])
                dock->hide();
        }
        m_docks[DockVideo]->setFloating(false);
        m_docks[DockVideo]->show();
        m_titleBar->hide();
        m_playerBar->hide();
        m_docks[DockVideo]->setTitleBarShown(false);
        m_video->setCursorAutoHide(true);
        showFullScreen();
        showCommandToast(Lang::tr("Esc para salir de la pantalla completa"));
        return;
    }

    m_videoFullscreen = false;
    showNormal();
    setGeometry(m_preFullscreenGeometry);
    m_titleBar->show();
    m_playerBar->show();
    m_docks[DockVideo]->setTitleBarShown(true);
    m_video->setCursorAutoHide(false);
    m_dockHost->restoreState(m_preFullscreenDocks, kDockStateVersion);
}

void MainWindow::toggleEqualizerPanel(bool visible)
{
    PanelDock* dock = m_docks[DockEqualizer];
    dock->setVisible(visible);
    // Si esta en pestanas con otro panel, abrirlo es tambien traerlo delante.
    if (visible)
        dock->raise();
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
    // Las flechas son ordenes (volumen, avanzar), pero en una lista o un campo
    // de texto con el foco siguen moviendose por ellos: aceptar el
    // ShortcutOverride le devuelve la tecla al control. En deslizadores y
    // desplegables mandan las ordenes: una flecha no debe cambiar sin querer
    // el preset del ecualizador. Un control oculto (la lista de archivos en
    // una disposicion de video) tampoco se queda las flechas.
    if (event->type() == QEvent::ShortcutOverride) {
        auto* key = static_cast<QKeyEvent*>(event);
        const int k = key->key();
        const bool arrow = (k == Qt::Key_Left || k == Qt::Key_Right
                            || k == Qt::Key_Up || k == Qt::Key_Down)
                           && (key->modifiers() & ~Qt::KeypadModifier) == Qt::NoModifier;
        auto* focused = qobject_cast<QWidget*>(watched);
        if (arrow && focused && focused->isVisible()
            && (qobject_cast<QAbstractItemView*>(watched)
                      || qobject_cast<QAbstractSpinBox*>(watched)
                      || qobject_cast<QLineEdit*>(watched)
                      || qobject_cast<QTextEdit*>(watched)
                      || qobject_cast<QPlainTextEdit*>(watched)
                      || qobject_cast<QKeySequenceEdit*>(watched))) {
            key->accept();
            return true;
        }
    }

    if (m_videoFullscreen && event->type() == QEvent::KeyPress
        && static_cast<QKeyEvent*>(event)->key() == Qt::Key_Escape) {
        toggleVideoFullscreen();
        return true;
    }

    if (isWindowMaximized() || isFullScreen())
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
