#include "ui/NowPlayingHeader.h"
#include "core/Lang.h"

#include "ui/RatingBar.h"
#include "ui/Theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QVBoxLayout>

NowPlayingHeader::NowPlayingHeader(QWidget* parent)
    : QWidget(parent)
{
    setObjectName(QStringLiteral("metaHeaderBlock"));

    auto* root = new QVBoxLayout(this);
    root->setContentsMargins(14, 6, 14, 4);
    root->setSpacing(2);

    const auto makeLabel = [this](const char* objectName, int pointSize) {
        auto* label = new QLabel(this);
        label->setObjectName(QString::fromLatin1(objectName));
        label->setFont(Theme::uiFont(pointSize));
        label->setAlignment(Qt::AlignHCenter);
        // Elide manual en setTrack: los titulos largos no deben ensanchar el
        // panel ni salirse por los lados.
        label->setTextInteractionFlags(Qt::TextSelectableByMouse);
        return label;
    };

    m_title  = makeLabel("metaTitleLabel",  11);
    m_artist = makeLabel("metaArtistLabel", 9);
    m_album  = makeLabel("metaAlbumLabel",  9);
    m_format = makeLabel("metaFormat",      8);

    root->addWidget(m_title);
    root->addWidget(m_artist);
    root->addWidget(m_album);
    root->addWidget(m_format);

    auto* ratingRow = new QHBoxLayout;
    ratingRow->setContentsMargins(0, 2, 0, 0);
    ratingRow->addStretch(1);
    m_rating = new RatingBar(this);
    ratingRow->addWidget(m_rating);
    ratingRow->addStretch(1);
    root->addLayout(ratingRow);

    connect(m_rating, &RatingBar::ratingChanged, this, &NowPlayingHeader::ratingChanged);
}

void NowPlayingHeader::setTrack(const TrackInfo& info)
{
    const int available = Theme::Metrics::LeftPanelWidth - 32;

    const auto setElided = [available](QLabel* label, const QString& text,
                                       const QString& fallback) {
        const QString value = text.isEmpty() ? fallback : text;
        label->setText(label->fontMetrics().elidedText(value, Qt::ElideRight, available));
        label->setToolTip(value == fallback ? QString() : value);
    };

    if (!info.isValid()) {
        setElided(m_title,  QString(), Lang::tr("Sin titulo"));
        setElided(m_artist, QString(), Lang::tr("Artista"));
        setElided(m_album,  QString(), Lang::tr("Album"));
        m_format->clear();
        m_rating->setRating(0);
        m_rating->setReadOnly(true);
        return;
    }

    setElided(m_title,  info.displayTitle(), Lang::tr("Sin titulo"));
    setElided(m_artist, info.artist,         Lang::tr("Artista"));
    setElided(m_album,  info.album,          Lang::tr("Album"));
    m_format->setText(info.formatLine());

    m_rating->setRating(info.rating);
    m_rating->setReadOnly(false);
}
