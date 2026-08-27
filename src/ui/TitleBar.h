#pragma once

#include <QWidget>

class FlatButton;

// Barra de titulo propia (la ventana es sin marco), con el logotipo a la
// izquierda y los botones de ventana a la derecha, como en la referencia.
class TitleBar : public QWidget {
    Q_OBJECT

public:
    explicit TitleBar(QWidget* parent = nullptr);

    void setMaximized(bool maximized);

signals:
    void minimizeRequested();
    void maximizeRequested();
    void closeRequested();
    void menuRequested(const QPoint& globalPos);
    void settingsRequested();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    FlatButton* m_settings = nullptr;
    FlatButton* m_menu     = nullptr;
    FlatButton* m_minimize = nullptr;
    FlatButton* m_maximize = nullptr;
    FlatButton* m_close    = nullptr;
    bool m_isMaximized = false;
};
