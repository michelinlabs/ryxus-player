#include "ui/PlaylistDelegate.h"
#include "core/PlaylistModel.h"
#include "core/TrackInfo.h"
#include "ui/Icons.h"
#include "ui/Theme.h"

#include <QMouseEvent>
#include <QPainter>

namespace {
constexpr int kCheckSize   = 13;
constexpr int kCheckLeft   = 8;
constexpr int kTextLeft    = 28;
constexpr int kRightMargin = 10;
constexpr int kStarSize    = 11;
constexpr int kStarGap     = 3;
constexpr int kRowHeight   = 48;
}

PlaylistDelegate::PlaylistDelegate(QObject* parent)
    : QStyledItemDelegate(parent)
{
}

QSize PlaylistDelegate::sizeHint(const QStyleOptionViewItem&, const QModelIndex&) const
{
    return QSize(0, kRowHeight);
}

QRect PlaylistDelegate::checkRect(const QStyleOptionViewItem& option) const
{
    return QRect(option.rect.left() + kCheckLeft,
                 option.rect.top() + 8,
                 kCheckSize, kCheckSize);
}

QRect PlaylistDelegate::starsRect(const QStyleOptionViewItem& option) const
{
    const int width = 5 * kStarSize + 4 * kStarGap;
    return QRect(option.rect.right() - kRightMargin - width,
                 option.rect.top() + kRowHeight - kStarSize - 9,
                 width, kStarSize);
}

int PlaylistDelegate::starAt(const QStyleOptionViewItem& option, const QPoint& pos) const
{
    const QRect stars = starsRect(option);
    if (!stars.adjusted(-2, -4, 2, 4).contains(pos))
        return -1;
    const int index = (pos.x() - stars.left()) / (kStarSize + kStarGap);
    return (index >= 0 && index < 5) ? index + 1 : -1;
}

void PlaylistDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option,
                             const QModelIndex& index) const
{
    const TrackInfo track = index.data(PlaylistModel::TrackRole).value<TrackInfo>();
    const bool selected  = option.state & QStyle::State_Selected;
    const bool hovered   = option.state & QStyle::State_MouseOver;
    const bool isCurrent = index.data(PlaylistModel::IsCurrentRole).toBool();
    const bool checked   = index.data(PlaylistModel::CheckedRole).toBool();

    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    // --- fondo -------------------------------------------------------------
    // La vista ya pinto su capa de fondo, asi que aqui solo se anaden los
    // matices: de lo contrario la transparencia se duplicaria por fila.
    if (selected)
        painter->fillRect(option.rect, Theme::selectionBg());
    else if (hovered)
        painter->fillRect(option.rect, Theme::selectionSoftBg());
    else if (index.row() % 2)
        painter->fillRect(option.rect, QColor(0, 0, 0, 24));

    // Barra violeta en el borde izquierdo de la pista en curso.
    if (isCurrent) {
        painter->setPen(Qt::NoPen);
        painter->setBrush(Theme::AccentBright);
        painter->drawRect(QRect(option.rect.left(), option.rect.top(), 3, option.rect.height()));
    }

    // --- casilla -----------------------------------------------------------
    const QRect check = checkRect(option);
    painter->setPen(QPen(checked ? Theme::AccentBright : Theme::TextFaint, 1.2));
    painter->setBrush(Qt::NoBrush);
    painter->drawRoundedRect(check, 2, 2);
    if (checked)
        Icons::paint(*painter, Icons::Check, check.adjusted(1, 1, -1, -1),
                     Theme::AccentBright, 1.6);

    // --- primera linea: numero, artista - titulo, duracion ----------------
    const QString duration = TrackInfo::formatDuration(track.durationMs);
    QFont titleFont = Theme::uiFont(9);
    painter->setFont(titleFont);
    const QFontMetrics titleMetrics(titleFont);

    const int durationWidth = titleMetrics.horizontalAdvance(duration) + 6;
    const QRect titleRect(option.rect.left() + kTextLeft, option.rect.top() + 6,
                          option.rect.width() - kTextLeft - kRightMargin - durationWidth,
                          titleMetrics.height() + 2);

    const QString headline = QStringLiteral("%1. %2")
                                 .arg(index.row() + 1)
                                 .arg(track.displayName());

    painter->setPen(isCurrent ? Theme::AccentBright
                              : (checked ? Theme::Text : Theme::TextDim));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter,
                      titleMetrics.elidedText(headline, Qt::ElideRight, titleRect.width()));

    painter->setPen(isCurrent ? Theme::AccentBright : Theme::TextDim);
    painter->drawText(QRect(option.rect.right() - kRightMargin - durationWidth,
                            titleRect.top(), durationWidth, titleRect.height()),
                      Qt::AlignRight | Qt::AlignVCenter, duration);

    // --- segunda linea: datos tecnicos y estrellas -------------------------
    QFont techFont = Theme::uiFont(8);
    painter->setFont(techFont);
    const QFontMetrics techMetrics(techFont);

    const QRect stars = starsRect(option);
    const QRect techRect(option.rect.left() + kTextLeft, option.rect.top() + 26,
                         stars.left() - (option.rect.left() + kTextLeft) - 8,
                         techMetrics.height() + 2);

    painter->setPen(Theme::TextFaint);
    painter->drawText(techRect, Qt::AlignLeft | Qt::AlignVCenter,
                      techMetrics.elidedText(track.techLine(), Qt::ElideRight,
                                             qMax(0, techRect.width())));

    for (int i = 0; i < 5; ++i) {
        const QRect box(stars.left() + i * (kStarSize + kStarGap), stars.top(),
                        kStarSize, kStarSize);
        const bool filled = i < track.rating;
        Icons::paint(*painter, filled ? Icons::Star : Icons::StarOutline, box,
                     filled ? Theme::AccentBright : Theme::TextFaint, 1.0);
    }

    // Separador inferior de 1 px, como en el skin.
    painter->setPen(QPen(QColor(0, 0, 0, 90), 1));
    painter->drawLine(option.rect.bottomLeft(), option.rect.bottomRight());

    painter->restore();
}

bool PlaylistDelegate::editorEvent(QEvent* event, QAbstractItemModel* model,
                                   const QStyleOptionViewItem& option,
                                   const QModelIndex& index)
{
    if (event->type() != QEvent::MouseButtonRelease)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    auto* mouse = static_cast<QMouseEvent*>(event);
    if (mouse->button() != Qt::LeftButton)
        return QStyledItemDelegate::editorEvent(event, model, option, index);

    const QPoint pos = mouse->position().toPoint();

    // Casilla: alterna la marca sin cambiar la seleccion.
    if (checkRect(option).adjusted(-4, -4, 4, 4).contains(pos)) {
        const bool checked = index.data(PlaylistModel::CheckedRole).toBool();
        model->setData(index, !checked, PlaylistModel::CheckedRole);
        return true;
    }

    // Estrellas: calificar directamente desde la lista.
    const int star = starAt(option, pos);
    if (star > 0) {
        const int current = index.data(PlaylistModel::RatingRole).toInt();
        model->setData(index, star == current ? 0 : star, PlaylistModel::RatingRole);
        return true;
    }

    return QStyledItemDelegate::editorEvent(event, model, option, index);
}
