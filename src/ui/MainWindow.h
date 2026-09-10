#pragma once

#include "core/AudioEngine.h"
#include "core/PlaylistModel.h"
#include "core/TrackInfo.h"

#include <QMainWindow>
#include <QVector>

class BackgroundHost;
class EqualizerPanel;
class FileListPanel;
class LibraryPanel;
class LibraryScanner;
class MetadataEditor;
class NowPlayingPanel;
class PlayerBar;
class PlaylistPanel;
class RibbonTabBar;
class SettingsDialog;
class TitleBar;
class WaveformWorker;

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

private slots:
    // --- reproduccion ------------------------------------------------------
    void playPlaylistRow(int row);
    void playNext();
    void playPrevious();
    void togglePlayPause();
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

    // --- fondo -------------------------------------------------------------
    void showAppMenu(const QPoint& globalPos);
    void chooseBackgroundImage();
    void clearBackgroundImage();
    void setBackgroundTransparency(int percent);
    void setBackgroundDarkening(int percent);
    void setBackgroundMode(int mode);

    // --- interfaz ----------------------------------------------------------
    void toggleMaximized();
    void runUninstaller();
    void toggleEqualizerPanel(bool visible);
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

    // Maximizado propio contra el area de trabajo, y rescate de la ventana
    // cuando la sesion anterior la dejo fuera de ella. Ver toggleMaximized().
    bool isWindowMaximized() const { return m_manualMaximized || isMaximized(); }
    void clampIntoWorkArea();
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
    PlayerBar*       m_playerBar  = nullptr;
    QSplitter*       m_centerSplitter = nullptr;

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
};
