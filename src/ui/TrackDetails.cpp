#include "ui/TrackDetails.h"
#include "core/Lang.h"

#include "ui/FlatButton.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QFileInfo>
#include <QFormLayout>
#include <QLabel>
#include <QStyle>
#include <QVBoxLayout>

TrackDetails::TrackDetails(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("trackDetails"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 6, 14, 4);
    root->setSpacing(4);

    m_header = new QLabel(Lang::tr("DETALLES"), this);
    QFont headerFont = Theme::uiFont(8, QFont::DemiBold);
    headerFont.setLetterSpacing(QFont::AbsoluteSpacing, 1.1);
    m_header->setFont(headerFont);
    m_header->setObjectName(QStringLiteral("sectionTitle"));
    root->addWidget(m_header);

    auto* form = new QFormLayout;
    form->setContentsMargins(0, 0, 0, 0);
    form->setHorizontalSpacing(8);
    form->setVerticalSpacing(3);
    form->setLabelAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
    root->addLayout(form);

    // Se guarda el QFormLayout en una lambda local: addRow lo necesita.
    const auto make = [this, form](const QString& title) {
        Row row;
        row.key = new QLabel(title, this);
        row.key->setObjectName(QStringLiteral("detailKey"));
        row.key->setFont(Theme::uiFont(8));

        row.value = new QLabel(this);
        row.value->setObjectName(QStringLiteral("detailValue"));
        row.value->setFont(Theme::uiFont(8));
        row.value->setTextInteractionFlags(Qt::TextSelectableByMouse);

        form->addRow(row.key, row.value);
        m_rows.append(row);
        return row;
    };

    m_artist      = make(Lang::tr("Artista"));
    m_albumArtist = make(Lang::tr("Artista alb."));
    m_album       = make(Lang::tr("Album"));
    m_genre       = make(Lang::tr("Genero"));
    m_year        = make(Lang::tr("Ano"));
    m_track       = make(Lang::tr("Pista"));
    m_duration    = make(Lang::tr("Duracion"));
    m_size        = make(Lang::tr("Tamano"));
    m_sampleRate  = make(Lang::tr("Audio"));

    root->addStretch(1);
    setTrack(TrackInfo());
}

void TrackDetails::setRow(const Row& row, const QString& value)
{
    if (!row.value)
        return;

    const int available = Theme::Metrics::LeftPanelWidth - 110;
    const bool empty = value.trimmed().isEmpty();

    row.value->setText(empty
        ? QStringLiteral("--")
        : row.value->fontMetrics().elidedText(value, Qt::ElideRight, available));
    row.value->setToolTip(empty ? QString() : value);
    row.value->setProperty("empty", empty);

    // Reaplica la hoja de estilos para que el selector [empty="true"] entre.
    row.value->style()->unpolish(row.value);
    row.value->style()->polish(row.value);
}

void TrackDetails::setTrack(const TrackInfo& info)
{
    m_track_ = info;

    setRow(m_artist,      info.artist);
    setRow(m_albumArtist, info.albumArtist);
    setRow(m_album,       info.album);
    setRow(m_genre,       info.genre);
    setRow(m_year,        info.year > 0 ? QString::number(info.year) : QString());

    QString track;
    if (info.trackNumber > 0) {
        track = QString::number(info.trackNumber);
        if (info.discNumber > 0)
            track += Lang::tr("  (disco %1)").arg(info.discNumber);
    }
    setRow(m_track, track);

    setRow(m_duration, info.durationMs > 0
                           ? TrackInfo::formatDuration(info.durationMs)
                           : QString());
    setRow(m_size, info.fileSize > 0 ? TrackInfo::formatSize(info.fileSize) : QString());

    // Mismo formato que la linea de la cabecera ("FLAC, 44 kHz, ..."), para
    // no mezclar dos unidades a dos centimetros de distancia.
    QString audio;
    if (info.sampleRate > 0) {
        audio = QStringLiteral("%1 kHz").arg(info.sampleRate / 1000);
        if (info.bitrateKbps > 0)
            audio += QStringLiteral(", %1 kbps").arg(info.bitrateKbps);
        if (info.channels == 1)      audio += Lang::tr(", mono");
        else if (info.channels == 2) audio += Lang::tr(", estereo");
        else if (info.channels > 2)  audio += Lang::tr(", %1 canales").arg(info.channels);
    }
    setRow(m_sampleRate, audio);
}
