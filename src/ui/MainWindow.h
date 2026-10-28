#pragma once

#include "core/AudioEngine.h"
#include "core/PlaylistModel.h"
#include "core/TrackInfo.h"
#include "ui/Commands.h"

#include <QMainWindow>
#include <QVector>

class BackgroundHost;
class EqualizerPanel;
class FileListPanel;
class LibraryPanel;
class LibraryScanner;
class MetadataEditor;
class NowPlayingPanel;
class PanelDock;
class PlayerBar;
class PlaylistPanel;
class RibbonTabBar;
class SettingsDialog;
class SystemAudioTap;
class TitleBar;
class VideoFilterPanel;
class VideoPanel;
class WaveformWorker;

class QMenu;
class QScreen;
class QShortcut;
class QSplitter;
class QStackedWidget;
class QThread;
class QTimer;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

    // Archivos recibidos por linea de comandos ("Abrir con..."): se anaden a
    // la lista activa y empieza a sonar el primero.
    void openPaths(const QStringList& paths);
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;
    bool eventFilter(QObject* watched, QEvent* event) override;

    // Windows avisa a las ventanas cuando se conecta o desconecta hardware:
    // asi se ven las interfaces USB enchufadas con el programa abierto.
    bool nativeEvent(const QByteArray& eventType, void* message, qintptr* result) override;

    // Cada movimiento o cambio de tamano se apunta para la combinacion de
    // monitores actual (con un respiro, para no escribir a cada pixel).
    void moveEvent(QMoveEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

    // Convierte el maximizado de Windows (Aero Snap, Win+Flecha arriba) en el
    // propio, que no tapa la barra de tareas.
    void changeEvent(QEvent* event) override;

    // Arrastrar pistas o carpetas desde el Explorador a la ventana.
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dropEvent(QDropEvent* event) override;

private slots:
    // --- reproduccion ------------------------------------------------------
    void playPlaylistRow(int row);
    void playNext();
    void playPrevious();
    void togglePlayPause();
    void onPlayClicked();
    void stopPlayback();
    void onEngineStateChanged(AudioEngine::State state);
    void onTick();

    // --- biblioteca --------------------------------------------------------
    void onFolderSelected(const QString& folder);
    void onFolderActivated(const QString& folder);
    void onFolderPlayRequested(const QString& folder);
    void onScanBatch(const QVector<TrackInfo>& tracks, quint64 requestId);
    void onScanFinished(const QString& folder, int total, quint64 requestId);

    // --- metadatos ---------------------------------------------------------
    void onSaveMetadata(const TrackInfo& info);
    void onRatingChanged(int rating);
    void onCoverChangeRequested();
    void onCoverRemoveRequested();
    void onCoverExportRequested();
    void onRereadTags(const QVector<TrackInfo>& tracks);
    void onBulkRating(const QVector<TrackInfo>& tracks, int rating);
    void showProperties(const TrackInfo& info);
    void openTagEditor(const TrackInfo& info);
    void openTagEditorForCurrent();
    void setDetailsTrack(const TrackInfo& info);
    void revealInExplorer(const QString& path);
    void deleteFilesPermanently(const QVector<TrackInfo>& tracks);

    // --- configuracion -----------------------------------------------------
    void openSettings();
    void applyTheme(const QString& paletteId);
    void restartForLanguage();
    void applySystemAudio(bool enabled, const QByteArray& source, const QByteArray& output);

    // --- fondo -------------------------------------------------------------
    void showAppMenu(const QPoint& globalPos);
    void chooseBackgroundImage();
    void clearBackgroundImage();
    void setBackgroundTransparency(int percent);
    void setBackgroundDarkening(int percent);
    void setBackgroundMode(int mode);
    void setBackgroundVisualization(int index);
    void setBackgroundImageOpacity(int percent);
    void setBackgroundVisualOpacity(int percent);

    // --- interfaz ----------------------------------------------------------
    void toggleMaximized();
    void runUninstaller();
    void toggleEqualizerPanel(bool visible);

    // --- ordenes con atajo (ver Commands) ----------------------------------
    void runCommand(int command);
    void applyShortcuts();   // tras cambiar atajos en Configuracion

    // --- paneles y disposiciones -------------------------------------------
    void setAllPanelsVisible(bool visible);
    void togglePanel(int dock);
    void cycleLayout(int step);   // +1 siguiente, -1 anterior
    void applyLayoutPreset(int preset);
    void applySavedLayout(const QString& name);
    void saveCurrentLayout();
    void cycleRepeatMode();
    void openFilesDialog();

private:
    void buildUi();
    void buildCenterColumn();
    void wireSignals();
    void installShortcuts();
    void restoreSession();
    void saveSession();

    void applyBackgroundSettings();

    // Gestos de la barra inferior que tambien tienen atajo.
    void setShuffleEnabled(bool enabled);
    void toggleMute();
    void stepVolume(float delta);
    void setVolumeTo(float volume);
    void seekBy(qint64 deltaMs);        // avanzar / retroceder, con aviso en el video
    void fillStepMenus(QMenu* menu);    // paso de las flechas (salto y volumen)
    void applyVideoGestureSettings();
    void stepAbLoop();
    void cycleVisualization();

    // Aviso breve con lo que acaba de hacer un atajo (que disposicion,
    // que visualizacion...).
    void showCommandToast(const QString& text);

    // Arranca la captura del sistema con el reparto guardado y, si Windows
    // todavia no la deja abrir, la vuelve a intentar sola al rato.
    void startSystemAudio();

    // Tamano y posicion de la ventana por combinacion de monitores. Ver
    // Settings::WindowPlacement.
    void saveWindowPlacement();
    bool restoreWindowPlacement();
    void watchScreen(QScreen* screen);
    void onScreenSetupChanged();

    // Se conecto o desconecto un dispositivo: refresca las listas de Ajustes
    // y, si la captura estaba esperando a su interfaz, la vuelve a intentar.
    void onAudioDevicesChanged();

    // Maximizado propio contra el area de trabajo, y rescate de la ventana
    // cuando la sesion anterior la dejo fuera de ella. Ver toggleMaximized().
    bool isWindowMaximized() const { return m_manualMaximized || isMaximized(); }

    // Hay audio circulando por la cadena: o suena el reproductor, o esta
    // entrando por la captura del sistema. El espectro del ecualizador y la
    // onda del panel izquierdo se mueven en los dos casos.
    bool audioIsFlowing() const;
    void clampIntoWorkArea();

    // Area util del monitor de la ventana: sin las barras de tareas, este
    // donde este cada una, y con una linea libre si alguna se oculta sola.
    QRect workArea() const;

    // Maximizado a mano: vuelve a ocupar exactamente el area util actual.
    void fitMaximizedToWorkArea();

    // Tras arrastrar o estirar: que la barra de titulo no quede debajo de una
    // barra de tareas puesta arriba.
    void keepTitleBarReachable(bool resized);
    void repaintForTransparency();
    void loadTrackIntoUi(const TrackInfo& info);
    void refreshTrackEverywhere(const TrackInfo& info);

    // Recarga el editor de etiquetas con `info`, salvo que haya cambios sin
    // guardar. Es lo que mantiene el editor pegado a la seleccion en vez de a
    // la ultima pista que se abrio en el.
    void syncTagEditor(const TrackInfo& info);
    Qt::Edges edgesAt(const QPoint& windowPos) const;

    // --- audio -------------------------------------------------------------
    AudioEngine* m_engine = nullptr;
    QTimer*      m_tick   = nullptr;

    QThread*        m_waveformThread = nullptr;
    WaveformWorker* m_waveformWorker = nullptr;
    QThread*        m_scannerThread  = nullptr;
    LibraryScanner* m_scanner        = nullptr;
    quint64         m_scanRequestId  = 0;

    // --- interfaz ----------------------------------------------------------
    BackgroundHost*  m_background = nullptr;
    TitleBar*        m_titleBar   = nullptr;
    NowPlayingPanel* m_nowPlaying = nullptr;
    RibbonTabBar*    m_centerTabs = nullptr;
    QStackedWidget*  m_centerStack = nullptr;
    LibraryPanel*    m_library    = nullptr;
    FileListPanel*   m_fileList   = nullptr;
    MetadataEditor*  m_tagEditor  = nullptr;
    PlaylistPanel*   m_playlists  = nullptr;
    EqualizerPanel*  m_equalizer  = nullptr;
    SettingsDialog*  m_settings   = nullptr;
    SystemAudioTap*  m_systemAudio = nullptr;
    QTimer*          m_systemAudioRetry = nullptr;
    int              m_systemAudioTries = 0;
    QTimer*          m_deviceChange = nullptr;   // agrupa la rafaga de avisos
    QList<QByteArray> m_presentOutputs;          // salidas en el ultimo vistazo
    PlayerBar*       m_playerBar  = nullptr;
    QSplitter*       m_centerSplitter = nullptr;
    QWidget*         m_centerColumn   = nullptr;

    // Paneles acoplables (ver PanelDock) y el QMainWindow interno que los
    // reparte. Ocultarlos deja ver el fondo de la ventana.
    enum Dock { DockNowPlaying = 0, DockFiles, DockPlaylist, DockEqualizer, DockVideo,
                DockVideoFilters, DockCount };
    QMainWindow* m_dockHost = nullptr;
    PanelDock*   m_docks[DockCount] = {};

    // Disposiciones de fabrica: puntos de partida para la de cada usuario.
    enum LayoutPreset { LayoutClassic = 0, LayoutLibrary, LayoutListening, LayoutStudio,
                        LayoutPresetCount };
    static QString layoutPresetName(int preset);

    // --- video ------------------------------------------------------------
    // Disposiciones para cuando hay imagen; van en su propio menu (Video),
    // aparte de las de audio. Al abrir un video se pasa sola a la ultima
    // usada, y al volver a la musica se recupera la disposicion de antes.
    enum VideoLayout { VideoCinema = 0, VideoWithPlaylist, VideoWithEqualizer, VideoStudio,
                       VideoLayoutCount };
    static QString videoLayoutName(int layout);
    void applyVideoLayout(int layout);
    void onVideoActiveChanged();
    void fillVideoMenu(QMenu* menu);
    void showVideoMenu(const QPoint& globalPos);
    void setVideoMask(int mask);
    void toggleVideoFullscreen();
    void addAudioTrackDialog();
    void addSubtitlesDialog();
    void loadSidecarSubtitles(const QString& videoPath);

    VideoPanel* m_video = nullptr;
    VideoFilterPanel* m_videoFilters = nullptr;
    void setVideoFiltersVisible(bool visible);
    QTimer*     m_videoModeTimer = nullptr;    // agrupa el baile al cambiar de video
    bool        m_inVideoMode = false;
    QByteArray  m_audioLayoutState;            // la de audio, para volver a ella
    bool        m_videoFullscreen = false;
    QByteArray  m_preFullscreenDocks;
    QRect       m_preFullscreenGeometry;

    // Se sube si cambian los paneles de forma que una disposicion guardada ya
    // no tenga sentido: restoreState la rechaza y se parte de la clasica.
    static constexpr int kDockStateVersion = 1;

    // Posicion en el recorrido de "disposicion siguiente": primero las de
    // fabrica y despues las guardadas.
    int m_layoutCycle = 0;

    QShortcut* m_shortcuts[Commands::Count] = {};

    // --- estado ------------------------------------------------------------
    TrackInfo m_currentTrack;   // la que suena
    TrackInfo m_detailsTrack;   // la que se inspecciona en la columna izquierda
    bool m_shuffle = false;
    PlaylistModel::RepeatMode m_repeat = PlaylistModel::RepeatMode::Off;

    // Bucle A-B: -1 cuando no hay punto marcado.
    qint64 m_loopStartMs = -1;
    qint64 m_loopEndMs   = -1;

    // Redimensionado de la ventana sin marco.
    Qt::Edges m_hoverEdges;
    bool      m_cursorOverridden = false;

    // Maximizado a mano: geometria a la que se vuelve al restaurar.
    QRect m_restoreGeometry;
    bool  m_manualMaximized = false;
    QSize m_sizeMoveStart;   // tamano al empezar un arrastre de Windows

    // Combinacion de monitores para la que se esta guardando la ventana. Al
    // cambiar, Windows mueve la ventana por su cuenta: mientras se decide la
    // nueva no se guarda nada, para no pisar lo de ninguna de las dos.
    QString m_screenKey;
    QTimer* m_placementSave = nullptr;
    QTimer* m_screenSetup   = nullptr;
};
