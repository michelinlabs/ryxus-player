#pragma once

#include "ui/Icons.h"

#include <QAbstractButton>
#include <QColor>

// Boton plano del skin: sin marco, el icono cambia de color al pasar el raton
// y las variantes conmutables se marcan con el violeta de acento.
class FlatButton : public QAbstractButton {
    Q_OBJECT

public:
    explicit FlatButton(Icons::Icon icon, QWidget* parent = nullptr);
    explicit FlatButton(const QString& text, QWidget* parent = nullptr);

    void setIconId(Icons::Icon icon);
    Icons::Icon iconId() const { return m_icon; }

    void setGlyphSize(int px);
    void setFixedDiameter(int px);
    void setStrokeWidth(qreal width);

    void setColors(const QColor& normal, const QColor& hover, const QColor& checked);
    void setDrawsRing(bool on);          // circulo alrededor, como el boton de play
    void setFilledWhenChecked(bool on);  // fondo violeta al estar activo

    QSize sizeHint() const override;

protected:
    void paintEvent(QPaintEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    Icons::Icon m_icon = Icons::None;
    int    m_glyphSize   = 18;
    int    m_diameter    = 30;
    qreal  m_strokeWidth = 1.6;
    bool   m_hovered     = false;
    bool   m_ring        = false;
    bool   m_fillChecked = false;

    QColor m_normal;
    QColor m_hover;
    QColor m_checked;
};
