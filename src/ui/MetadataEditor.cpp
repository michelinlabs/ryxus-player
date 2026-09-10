#include "ui/MetadataEditor.h"
#include "core/Lang.h"

#include "core/MetadataService.h"
#include "ui/CoverArtView.h"
#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/RatingBar.h"
#include "ui/Theme.h"

#include <QCheckBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPlainTextEdit>
#include <QScrollArea>
#include <QSpinBox>
#include <QVBoxLayout>

#include <utility>

const QVector<MetadataEditor::TextField>& MetadataEditor::textFields()
{
    // El orden es el del formulario. Los marcados como extendidos van a la
    // seccion desplegable.
    static const QVector<TextField> fields = {
        {"Titulo",            &TrackInfo::title,          false},
        {"Artista",           &TrackInfo::artist,         false},
        {"Album",             &TrackInfo::album,          false},
        {"Artista del album", &TrackInfo::albumArtist,    false},
        {"Genero",            &TrackInfo::genre,          false},
        {"BPM",               &TrackInfo::bpm,            false},
        {"Clave",             &TrackInfo::key,            false},

        {"Artista original",  &TrackInfo::originalArtist, true},
        {"Mezcla por",        &TrackInfo::remixer,        true},
        {"Compositor",        &TrackInfo::composer,       true},
        {"Director",          &TrackInfo::conductor,      true},
        {"Agrupacion",        &TrackInfo::grouping,       true},
        {"Subtitulo",         &TrackInfo::subtitle,       true},
        {"ISRC",              &TrackInfo::isrc,           true},
        {"Editora",           &TrackInfo::label,          true},
        {"Derechos",          &TrackInfo::copyright,      true},
        {"URL",               &TrackInfo::url,            true},
        {"Codificador",       &TrackInfo::encodedBy,      true},
    };
    return fields;
}

namespace {

QLabel* formLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    label->setObjectName(QStringLiteral("detailKey"));
    label->setFont(Theme::uiFont(9));
    return label;
}

QLabel* sectionLabel(const QString& text, QWidget* parent)
{
    auto* label = new QLabel(text, parent);
    QFont font = Theme::uiFont(8, QFont::DemiBold);
    font.setLetterSpacing(QFont::AbsoluteSpacing, 1.2);
    label->setFont(font);
    label->setObjectName(QStringLiteral("sectionTitle"));
    return label;
}

} // namespace

MetadataEditor::MetadataEditor(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("metadataEditor"));
    buildUi();
    setTrack(TrackInfo());
}

// --------------------------------------------------------------- construccion

QWidget* MetadataEditor::buildMainSection()
{
    auto* section = new QWidget(m_formArea);
    auto* column = new QVBoxLayout(section);
    column->setContentsMargins(0, 0, 0, 0);
    column->setSpacing(6);

    column->addWidget(sectionLabel(Lang::tr("PRINCIPAL"), section));

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const auto makeSpin = [&](int maximum, int width) {
        auto* spin = new QSpinBox(section);
        spin->setObjectName(QStringLiteral("metaFormField"));
        spin->setFont(Theme::uiFont(9));
        spin->setRange(0, maximum);
        spin->setSpecialValueText(QStringLiteral("--"));
        spin->setButtonSymbols(QAbstractSpinBox::NoButtons);
        spin->setMinimumHeight(24);
        spin->setFixedWidth(width);
        connect(spin, &QSpinBox::valueChanged, this, &MetadataEditor::refreshDirtyState);
        return spin;
    };

    // --- pista y disco, cada uno con su total -----------------------------
    m_trackNumber = makeSpin(999, 70);
    m_trackTotal  = makeSpin(999, 70);
    m_discNumber  = makeSpin(99, 70);
    m_discTotal   = makeSpin(99, 70);

    auto* numbers = new QHBoxLayout;
    numbers->setSpacing(6);
    numbers->addWidget(m_trackNumber);
    numbers->addWidget(new QLabel(QStringLiteral("/"), section));
    numbers->addWidget(m_trackTotal);
    numbers->addSpacing(14);
    numbers->addWidget(formLabel(Lang::tr("Disco"), section));
    numbers->addWidget(m_discNumber);
    numbers->addWidget(new QLabel(QStringLiteral("/"), section));
    numbers->addWidget(m_discTotal);
    numbers->addStretch(1);
    form->addRow(formLabel(Lang::tr("Pista"), section), numbers);

    // --- campos de texto principales --------------------------------------
    const QVector<TextField>& fields = textFields();
    int bpmIndex = -1, keyIndex = -1;

    for (int i = 0; i < fields.size(); ++i) {
        if (fields.at(i).extended)
            continue;

        // BPM y clave comparten fila: sueltos ocupaban todo el ancho para dos
        // valores de cuatro caracteres.
        if (fields.at(i).member == &TrackInfo::bpm) {
            bpmIndex = i;
            continue;
        }
        if (fields.at(i).member == &TrackInfo::key) {
            keyIndex = i;
            continue;
        }

        form->addRow(formLabel(Lang::tr(fields.at(i).label), section), m_textEdits.at(i));
    }

    // --- ano y compilacion -------------------------------------------------
    m_year = makeSpin(2200, 90);

    m_compilation = new QCheckBox(Lang::tr("Parte de una compilacion"), section);
    m_compilation->setFont(Theme::uiFont(9));
    connect(m_compilation, &QCheckBox::toggled, this, &MetadataEditor::refreshDirtyState);

    auto* yearRow = new QHBoxLayout;
    yearRow->setSpacing(14);
    yearRow->addWidget(m_year);
    yearRow->addWidget(m_compilation);
    yearRow->addStretch(1);
    form->addRow(formLabel(Lang::tr("Ano"), section), yearRow);

    // --- BPM y clave -------------------------------------------------------
    if (bpmIndex >= 0 && keyIndex >= 0) {
        m_textEdits.at(bpmIndex)->setFixedWidth(90);
        m_textEdits.at(keyIndex)->setFixedWidth(90);

        auto* tempoRow = new QHBoxLayout;
        tempoRow->setSpacing(14);
        tempoRow->addWidget(m_textEdits.at(bpmIndex));
        tempoRow->addWidget(formLabel(Lang::tr("Clave"), section));
        tempoRow->addWidget(m_textEdits.at(keyIndex));
        tempoRow->addStretch(1);
        form->addRow(formLabel(Lang::tr("BPM"), section), tempoRow);
    }

    // --- comentario --------------------------------------------------------
    m_comment = new QPlainTextEdit(section);
    m_comment->setObjectName(QStringLiteral("metaFormField"));
    m_comment->setFont(Theme::uiFont(9));
    m_comment->setFixedHeight(56);
    connect(m_comment, &QPlainTextEdit::textChanged, this, &MetadataEditor::refreshDirtyState);
    form->addRow(formLabel(Lang::tr("Comentario"), section), m_comment);

    column->addLayout(form);
    return section;
}

QWidget* MetadataEditor::buildExtendedSection()
{
    auto* section = new QWidget(m_formArea);
    auto* form = new QFormLayout(section);
    form->setContentsMargins(0, 2, 0, 0);
    form->setHorizontalSpacing(12);
    form->setVerticalSpacing(6);
    form->setLabelAlignment(Qt::AlignRight | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);

    const QVector<TextField>& fields = textFields();
    for (int i = 0; i < fields.size(); ++i) {
        if (!fields.at(i).extended)
            continue;
        form->addRow(formLabel(Lang::tr(fields.at(i).label), section), m_textEdits.at(i));
    }

    return section;
}

void MetadataEditor::buildUi()
{
    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(20, 16, 20, 14);
    root->setSpacing(10);

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

    // Los campos de texto se crean antes que las secciones: cada seccion solo
    // reparte los que le tocan.
    const QVector<TextField>& fields = textFields();
    m_textEdits.reserve(fields.size());
    for (int i = 0; i < fields.size(); ++i) {
        auto* edit = new QLineEdit(m_formArea);
        edit->setObjectName(QStringLiteral("metaFormField"));
        edit->setFont(Theme::uiFont(9));
        edit->setMinimumHeight(24);
        connect(edit, &QLineEdit::textEdited, this, &MetadataEditor::refreshDirtyState);
        m_textEdits.append(edit);
    }

    // Columna del formulario, desplazable: con la seccion extendida abierta no
    // caben todos los campos de una vez.
    auto* scroll = new QScrollArea(m_formArea);
    scroll->setObjectName(QStringLiteral("metaScroll"));
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    auto* formHost = new QWidget(scroll);
    auto* formColumn = new QVBoxLayout(formHost);
    formColumn->setContentsMargins(0, 0, 10, 0);
    formColumn->setSpacing(8);

    formColumn->addWidget(buildMainSection());

    // --- interruptor de la seccion extendida ------------------------------
    auto* extendedRow = new QHBoxLayout;
    extendedRow->setSpacing(8);

    m_extendedButton = new FlatButton(Lang::tr("Extendida"), formHost);
    m_extendedButton->setIconId(Icons::ChevronRight);
    m_extendedButton->setGlyphSize(14);
    m_extendedButton->setFixedHeight(24);
    m_extendedButton->setCheckable(true);
    m_extendedButton->setToolTip(
        Lang::tr("Mostrar el resto de etiquetas que admite el archivo"));
    extendedRow->addWidget(m_extendedButton);
    extendedRow->addStretch(1);
    formColumn->addLayout(extendedRow);
    connect(m_extendedButton, &FlatButton::toggled, this, &MetadataEditor::setExtendedVisible);

    m_extended = buildExtendedSection();
    m_extended->hide();
    formColumn->addWidget(m_extended);

    formColumn->addStretch(1);
    scroll->setWidget(formHost);
    body->addWidget(scroll, 1);

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

void MetadataEditor::setExtendedVisible(bool visible)
{
    m_extended->setVisible(visible);
    m_extendedButton->setIconId(visible ? Icons::ChevronDown : Icons::ChevronRight);
}

// -------------------------------------------------------------------- estado

void MetadataEditor::setTrack(const TrackInfo& info)
{
    m_loading = true;
    m_original = info;

    const bool valid = info.isValid();
    m_formArea->setVisible(valid);
    m_emptyHint->setVisible(!valid);
    m_revealButton->setEnabled(valid);

    const QVector<TextField>& fields = textFields();
    for (int i = 0; i < fields.size(); ++i)
        m_textEdits.at(i)->setText(info.*(fields.at(i).member));

    m_year->setValue(info.year);
    m_trackNumber->setValue(info.trackNumber);
    m_trackTotal->setValue(info.trackTotal);
    m_discNumber->setValue(info.discNumber);
    m_discTotal->setValue(info.discTotal);
    m_compilation->setChecked(info.compilation);
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

    for (QLineEdit* edit : std::as_const(m_textEdits))
        edit->setEnabled(writable);
    const QList<QWidget*> others = {
        m_year, m_trackNumber, m_trackTotal, m_discNumber, m_discTotal,
        m_compilation, m_comment
    };
    for (QWidget* field : others)
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

    const QVector<TextField>& fields = textFields();
    for (int i = 0; i < fields.size(); ++i)
        edited.*(fields.at(i).member) = m_textEdits.at(i)->text().trimmed();

    edited.year        = m_year->value();
    edited.trackNumber = m_trackNumber->value();
    edited.trackTotal  = m_trackTotal->value();
    edited.discNumber  = m_discNumber->value();
    edited.discTotal   = m_discTotal->value();
    edited.compilation = m_compilation->isChecked();
    edited.comment     = m_comment->toPlainText().trimmed();
    edited.rating      = m_rating->rating();
    return edited;
}

void MetadataEditor::refreshDirtyState()
{
    if (m_loading || !m_original.isValid())
        return;

    const TrackInfo edited = editedTrack();

    bool dirty = edited.comment     != m_original.comment
              || edited.year        != m_original.year
              || edited.trackNumber != m_original.trackNumber
              || edited.trackTotal  != m_original.trackTotal
              || edited.discNumber  != m_original.discNumber
              || edited.discTotal   != m_original.discTotal
              || edited.compilation != m_original.compilation;

    const QVector<TextField>& fields = textFields();
    for (int i = 0; i < fields.size() && !dirty; ++i)
        dirty = edited.*(fields.at(i).member) != m_original.*(fields.at(i).member);

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
