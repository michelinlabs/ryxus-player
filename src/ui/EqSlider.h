#pragma once

#include <QWidget>

// Deslizador vertical del ecualizador, dibujado a mano: rail hundido, relleno
// violeta desde el centro (0 dB) y lectura numerica bajo la etiqueta.
class EqSlider : public QWidget {
    Q_OBJECT

public:
    explicit EqSlider(const QString& label, QWidget* parent = nullptr);

    void  setRange(float minDb, float maxDb);
    void  setValue(float db, bool notify = true);
    float value() const { return m_value; }

    void setLabel(const QString& label);
    QString label() const { return m_label; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void valueChanged(float db);

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void enterEvent(QEnterEvent* event) override;
    void leaveEvent(QEvent* event) override;

private:
    QRect  railRect() const;
    float  valueAt(int y) const;
    int    yForValue(float db) const;

    QString m_label;
    float m_value  = 0.0f;
    float m_minDb  = -12.0f;
    float m_maxDb  = 12.0f;
    bool  m_hovered  = false;
    bool  m_dragging = false;
};
