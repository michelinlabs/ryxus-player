#pragma once

#include <QWidget>

// Barra de 5 estrellas, editable con el raton.
class RatingBar : public QWidget {
    Q_OBJECT

public:
    explicit RatingBar(QWidget* parent = nullptr);

    int  rating() const { return m_rating; }
    void setRating(int rating);
    void setReadOnly(bool readOnly);
    void setStarSize(int px);

    QSize sizeHint() const override;

signals:
    void ratingChanged(int rating);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    int starAt(int x) const;

    int  m_rating   = 0;
    int  m_hovered  = -1;
    int  m_starSize = 14;
    bool m_readOnly = false;
};
