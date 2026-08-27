#pragma once

#include <QStyledItemDelegate>

// Fila de dos lineas de la lista de reproduccion:
//
//   [x] 7. fhana - Ao no Rhapsody                              4:38
//       MP3 :: 44 kHz, 320 kbps, 10,85 MB               * * * . .
//
// La casilla y las estrellas son interactivas dentro del propio delegado.
class PlaylistDelegate : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit PlaylistDelegate(QObject* parent = nullptr);

    void  paint(QPainter* painter, const QStyleOptionViewItem& option,
                const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option,
                   const QModelIndex& index) const override;

    bool editorEvent(QEvent* event, QAbstractItemModel* model,
                     const QStyleOptionViewItem& option,
                     const QModelIndex& index) override;

private:
    QRect checkRect(const QStyleOptionViewItem& option) const;
    QRect starsRect(const QStyleOptionViewItem& option) const;
    int   starAt(const QStyleOptionViewItem& option, const QPoint& pos) const;
};
