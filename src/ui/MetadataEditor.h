#pragma once

#include "core/TrackInfo.h"

#include <QWidget>

class CoverArtView;
class FlatButton;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class RatingBar;

// Editor completo de etiquetas. Ocupa su propia pestana del panel central,
// asi que hay sitio para todos los campos a la vez en lugar del desplegable
// apretado que habia antes en la columna izquierda.
class MetadataEditor : public QWidget {
    Q_OBJECT

public:
    explicit MetadataEditor(QWidget* parent = nullptr);

    void setTrack(const TrackInfo& info);
    const TrackInfo& track() const { return m_original; }

    // Copia de `m_original` con lo que hay ahora en los campos.
    TrackInfo editedTrack() const;

    bool isDirty() const { return m_dirty; }

signals:
    void saveRequested(const TrackInfo& info);
    void ratingChanged(int rating);
    void dirtyChanged(bool dirty);
    void coverChangeRequested();
    void coverRemoveRequested();
    void coverExportRequested();
    void revealRequested(const QString& path);
    void closeRequested();

private slots:
    void refreshDirtyState();
    void revert();
    void save();

private:
    void buildUi();

    TrackInfo m_original;
    bool m_dirty   = false;
    bool m_loading = false;

    CoverArtView* m_cover = nullptr;
    RatingBar*    m_rating = nullptr;

    QLineEdit* m_title       = nullptr;
    QLineEdit* m_artist      = nullptr;
    QLineEdit* m_albumArtist = nullptr;
    QLineEdit* m_album       = nullptr;
    QLineEdit* m_genre       = nullptr;
    QLineEdit* m_composer    = nullptr;
    QSpinBox*  m_year        = nullptr;
    QSpinBox*  m_trackNumber = nullptr;
    QSpinBox*  m_discNumber  = nullptr;
    QPlainTextEdit* m_comment = nullptr;

    QLabel* m_pathLabel = nullptr;
    QLabel* m_techLabel = nullptr;
    QLabel* m_warning   = nullptr;
    QLabel* m_emptyHint = nullptr;
    QWidget* m_formArea = nullptr;

    FlatButton* m_saveButton   = nullptr;
    FlatButton* m_revertButton = nullptr;
    FlatButton* m_revealButton = nullptr;
};
