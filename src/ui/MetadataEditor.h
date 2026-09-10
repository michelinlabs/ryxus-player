#pragma once

#include "core/TrackInfo.h"

#include <QVector>
#include <QWidget>

class CoverArtView;
class FlatButton;
class QCheckBox;
class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QSpinBox;
class RatingBar;

// Editor completo de etiquetas. Ocupa su propia pestana del panel central,
// asi que hay sitio para todos los campos a la vez en lugar del desplegable
// apretado que habia antes en la columna izquierda.
//
// Se divide en dos bloques: "Principal", con lo que se toca a diario, y
// "Extendida", plegado por defecto, con el resto de etiquetas que admiten los
// formatos (ISRC, editora, agrupacion, derechos...). Todo lo que se ve aqui se
// escribe de verdad en el archivo.
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
    void setExtendedVisible(bool visible);

private:
    // Campo de texto simple. Declarar uno aqui es todo lo que hace falta: de
    // esta tabla salen el formulario, la carga, el guardado y la comparacion
    // que decide si hay cambios sin guardar.
    struct TextField {
        const char*         label;
        QString TrackInfo::*member;
        bool                extended;
    };

    static const QVector<TextField>& textFields();

    void buildUi();
    QWidget* buildMainSection();
    QWidget* buildExtendedSection();

    TrackInfo m_original;
    bool m_dirty   = false;
    bool m_loading = false;

    CoverArtView* m_cover  = nullptr;
    RatingBar*    m_rating = nullptr;

    // Mismo orden que textFields().
    QVector<QLineEdit*> m_textEdits;

    QSpinBox*  m_year        = nullptr;
    QSpinBox*  m_trackNumber = nullptr;
    QSpinBox*  m_trackTotal  = nullptr;
    QSpinBox*  m_discNumber  = nullptr;
    QSpinBox*  m_discTotal   = nullptr;
    QCheckBox* m_compilation = nullptr;
    QPlainTextEdit* m_comment = nullptr;

    QLabel*  m_pathLabel = nullptr;
    QLabel*  m_techLabel = nullptr;
    QLabel*  m_warning   = nullptr;
    QLabel*  m_emptyHint = nullptr;
    QWidget* m_formArea  = nullptr;
    QWidget* m_extended  = nullptr;

    FlatButton* m_extendedButton = nullptr;
    FlatButton* m_saveButton     = nullptr;
    FlatButton* m_revertButton   = nullptr;
    FlatButton* m_revealButton   = nullptr;
};
