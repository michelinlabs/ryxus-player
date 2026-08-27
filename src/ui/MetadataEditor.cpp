#include "ui/MetadataEditor.h"
#include "core/Lang.h"

#include "core/MetadataService.h"
#include "ui/CoverArtView.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/RatingBar.h"
#include "ui/Theme.h"

#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QSpinBox>
#include <QVBoxLayout>

MetadataEditor::MetadataEditor(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("metadataEditor"));
    buildUi();
    setTrack(TrackInfo());
}

void MetadataEditor::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 14);
    root->setSpacing(12);

    // --- titulo de la seccion ---------------------------------------------
    auto* headerRow = new QHBoxLayout;
    auto* title = new QLabel(Lang::tr("EDITAR ETIQUETAS"), this);
    QFont titleFont = Theme::uiFont(9, QFont::DemiBold);
    titleFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    title->setFont(titleFont);
    title->setObjectName(QStringLiteral("sectionTitle"));
    headerRow->addWidget(title);
    headerRow->addStretch(1);

    m_techLabel = new QLabel(this);
    m_techLabel->setObjectName(QStringLiteral("statusText"));
    m_techLabel->setFont(Theme::uiFont(8));
    headerRow->addWidget(m_techLabel);
    root->addLayout(headerRow);

    // --- aviso cuando no hay pista ----------------------------------------
    m_emptyHint = new QLabel(this);
    m_emptyHint->setObjectName(QStringLiteral("placeholderText"));
    m_emptyHint->setAlignment(Qt::AlignCenter);
    m_emptyHint->setFont(Theme::uiFont(9));
    m_emptyHint->setText(Lang::tr(
        "Selecciona una pista en la lista y pulsa \"Editar etiquetas\"\n"
        "para modificar sus datos aqui."));
    root->addWidget(m_emptyHint, 1);

    // --- cuerpo: caratula + formulario ------------------------------------
    m_formArea = new QWidget(this);
    auto* body = new QHBoxLayout(m_formArea);
    body->setContentsMargins(0, 0, 0, 0);
    body->setSpacing(20);

    // Columna de la caratula.
    auto* coverColumn = new QVBoxLayout;
    coverColumn->setSpacing(8);

    m_cover = new CoverArtView(m_formArea);
    m_cover->setFixedSize(210, 210);
    coverColumn->addWidget(m_cover);

    auto* coverHint = new QLabel(Lang::tr("Doble clic para cambiar la caratula"),
                                 m_formArea);
    coverHint->setObjectName(QStringLiteral("detailKey"));
    coverHint->setFont(Theme::uiFont(8));
    coverHint->setWordWrap(true);
    coverHint->setAlignment(Qt::AlignHCenter);
    coverHint->setFixedWidth(210);
    coverColumn->addWidget(coverHint);

    auto* ratingRow = new QHBoxLayout;
    ratingRow->addStretch(1);
    m_rating = new RatingBar(m_formArea);
    m_rating->setStarSize(18);
    ratingRow->addWidget(m_rating);
    ratingRow->addStretch(1);
    coverColumn->addLayout(ratingRow);

    coverColumn->addStretch(1);
    body->addLayout(coverColumn);

    connect(m_cover, &CoverArtView::changeRequested, this, &MetadataEditor::coverChangeRequested);
    connect(m_cover, &CoverArtView::removeRequested, this, &MetadataEditor::coverRemoveRequested);
    connect(m_cover, &CoverArtView::exportRequested, this, &MetadataEditor::coverExportRequested);
    connect(m_rating, &RatingBar::ratingChanged, this, [this](int value) {
        m_original.rating = value;
        emit ratingChanged(value);
    });

    // Columna del formulario.
    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(7);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto makeLabel = [this](const QString& text) {
        auto* label = new QLabel(text, m_formArea);
        label->setObjectName(QStringLiteral("detailKey"));
        label->setFont(Theme::uiFont(9));
        return label;
    };

    const auto makeEdit = [this]() {
        auto* edit = new QLineEdit(m_formArea);
        edit->setObjectName(QStringLiteral("metaFormField"));
        edit->setFont(Theme::uiFont(9));
        edit->setMinimumHeight(24);
        connect(edit, &QLineEdit::textEdited, this, &MetadataEditor::refreshDirtyState);
        return edit;
    };

    const auto makeSpin = [this](int maximum) {
        auto* spin = new QSpinBox(m_formArea);
        spin->setObjectName(QStringLiteral("metaFormField"));
        spin->setFont(Theme::uiFont(9));
        spin->setRange(0, maximum);
        spin->setSpecialValueText(QStringLiteral("--"));
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setMinimumHeight(24);
        spin->setFixedWidth(90);
        connect(spin, &QSpinBox::valueChanged, this, &MetadataEditor::refreshDirtyState);
        return spin;
    };

    m_title       = makeEdit();
    m_artist      = makeEdit();
    m_albumArtist = makeEdit();
    m_album       = makeEdit();
    m_genre       = makeEdit();
    m_composer    = makeEdit();
    m_year        = makeSpin(2200);
    m_trackNumber = makeSpin(999);
    m_discNumber  = makeSpin(99);

    m_comment = new QPlainTextEdit(m_formArea);
    m_comment->setObjectName(QStringLiteral("metaFormField"));
    m_comment->setFont(Theme::uiFont(9));
    m_comment->setFixedHeight(64);
    connect(m_comment, &QPlainTextEdit::textChanged, this, &MetadataEditor::refreshDirtyState);

    form->addRow(makeLabel(Lang::tr("Titulo")),            m_title);
    form->addRow(makeLabel(Lang::tr("Artista")),           m_artist);
    form->addRow(makeLabel(Lang::tr("Artista del album")), m_albumArtist);
    form->addRow(makeLabel(Lang::tr("Album")),             m_album);
    form->addRow(makeLabel(Lang::tr("Genero")),            m_genre);
    form->addRow(makeLabel(Lang::tr("Compositor")),        m_composer);

    // Ano, pista y disco caben en una sola fila.
    auto* numbersRow = new QHBoxLayout;
    numbersRow->setSpacing(10);
    numbersRow->addWidget(m_year);
    numbersRow->addWidget(makeLabel(Lang::tr("N.o pista")));
    numbersRow->addWidget(m_trackNumber);
    numbersRow->addWidget(makeLabel(Lang::tr("Disco")));
    numbersRow->addWidget(m_discNumber);
    numbersRow->addStretch(1);
    form->addRow(makeLabel(Lang::tr("Ano")), numbersRow);

    form->addRow(makeLabel(Lang::tr("Comentario")), m_comment);

    auto* formColumn = new QVBoxLayout;
    formColumn->addLayout(form);
    formColumn->addStretch(1);
    body->addLayout(formColumn, 1);

    root->addWidget(m_formArea, 1);

    // --- pie: ruta, aviso y acciones --------------------------------------
    m_warning = new QLabel(this);
    m_warning->setObjectName(QStringLiteral("metaWarning"));
    m_warning->setFont(Theme::uiFont(8));
    m_warning->setText(Lang::tr("Este archivo es de solo lectura: no se puede guardar."));
    m_warning->hide();
    root->addWidget(m_warning);

    auto* footer = new QHBoxLayout;
    footer->setSpacing(8);

    m_pathLabel = new QLabel(this);
    m_pathLabel->setObjectName(QStringLiteral("statusText"));
    m_pathLabel->setFont(Theme::uiFont(8));
    m_pathLabel->setTextInteractionFlags(Qt::TextSelectableByMouse);
    footer->addWidget(m_pathLabel, 1);

    m_revealButton = new FlatButton(Lang::tr("Ubicacion"), this);
    m_revealButton->setIconId(Icons::FolderOpen);
    m_revealButton->setGlyphSize(14);
    m_revealButton->setFixedHeight(26);
    footer->addWidget(m_revealButton);
    connect(m_revealButton, &FlatButton::clicked, this, [this]() {
        if (m_original.isValid())
            emit revealRequested(m_original.path);
    });

    m_revertButton = new FlatButton(Lang::tr("Revertir"), this);
    m_revertButton->setIconId(Icons::Revert);
    m_revertButton->setGlyphSize(14);
    m_revertButton->setFixedHeight(26);
    footer->addWidget(m_revertButton);
    connect(m_revertButton, &FlatButton::clicked, this, &MetadataEditor::revert);

    m_saveButton = new FlatButton(Lang::tr("Guardar cambios"), this);
    m_saveButton->setIconId(Icons::Save);
    m_saveButton->setGlyphSize(14);
    m_saveButton->setFixedHeight(26);
    m_saveButton->setColors(Theme::AccentBright, Theme::Text, Theme::Text);
    footer->addWidget(m_saveButton);
    connect(m_saveButton, &FlatButton::clicked, this, &MetadataEditor::save);

    root->addLayout(footer);
}

void MetadataEditor::setTrack(const TrackInfo& info)
{
    m_loading = true;
    m_original = info;

    const bool valid = info.isValid();
    m_formArea->setVisible(valid);
    m_emptyHint->setVisible(!valid);
    m_revealButton->setEnabled(valid);

    m_title->setText(info.title);
    m_artist->setText(info.artist);
    m_albumArtist->setText(info.albumArtist);
    m_album->setText(info.album);
    m_genre->setText(info.genre);
    m_composer->setText(info.composer);
    m_year->setValue(info.year);
    m_trackNumber->setValue(info.trackNumber);
    m_discNumber->setValue(info.discNumber);
    m_comment->setPlainText(info.comment);
    m_rating->setRating(info.rating);
    m_cover->setCover(info.cover);
    m_cover->setEditable(valid);

    m_techLabel->setText(valid ? info.techLine() : QString());
    m_pathLabel->setText(valid
        ? m_pathLabel->fontMetrics().elidedText(info.path, Qt::ElideMiddle, 520)
        : QString());
    m_pathLabel->setToolTip(info.path);

    const bool writable = valid && MetadataService::isWritable(info.path);
    m_warning->setVisible(valid && !writable);
    m_rating->setReadOnly(!writable);

    const QList<QWidget*> fields = {
        m_title, m_artist, m_albumArtist, m_album, m_genre, m_composer,
        m_year, m_trackNumber, m_discNumber, m_comment
    };
    for (QWidget* field : fields)
        field->setEnabled(writable);

    m_loading = false;
    m_dirty = false;
    m_saveButton->setEnabled(false);
    m_revertButton->setEnabled(false);
    emit dirtyChanged(false);
}

TrackInfo MetadataEditor::editedTrack() const
{
    TrackInfo edited = m_original;
    edited.title       = m_title->text().trimmed();
    edited.artist      = m_artist->text().trimmed();
    edited.albumArtist = m_albumArtist->text().trimmed();
    edited.album       = m_album->text().trimmed();
    edited.genre       = m_genre->text().trimmed();
    edited.composer    = m_composer->text().trimmed();
    edited.year        = m_year->value();
    edited.trackNumber = m_trackNumber->value();
    edited.discNumber  = m_discNumber->value();
    edited.comment     = m_comment->toPlainText().trimmed();
    edited.rating      = m_rating->rating();
    return edited;
}

void MetadataEditor::refreshDirtyState()
{
    if (m_loading || !m_original.isValid())
        return;

    const TrackInfo edited = editedTrack();
    const bool dirty =
        edited.title       != m_original.title       ||
        edited.artist      != m_original.artist      ||
        edited.albumArtist != m_original.albumArtist ||
        edited.album       != m_original.album       ||
        edited.genre       != m_original.genre       ||
        edited.composer    != m_original.composer    ||
        edited.comment     != m_original.comment     ||
        edited.year        != m_original.year        ||
        edited.trackNumber != m_original.trackNumber ||
        edited.discNumber  != m_original.discNumber;

    if (dirty == m_dirty)
        return;

    m_dirty = dirty;
    m_saveButton->setEnabled(dirty);
    m_revertButton->setEnabled(dirty);
    emit dirtyChanged(dirty);
}

void MetadataEditor::revert()
{
    setTrack(m_original);
}

void MetadataEditor::save()
{
    if (!m_original.isValid())
        return;
    emit saveRequested(editedTrack());
}
