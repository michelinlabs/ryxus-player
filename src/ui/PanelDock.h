#pragma once

#include <QDockWidget>

class FlatButton;
class QLabel;

// Panel acoplable de la ventana principal, al estilo de los de Photoshop: se
// arrastra por su barra de titulo para acoplarlo en otro borde, junto a otro
// panel o en pestanas con el; se saca como ventana flotante y se estira con
// el raton por cualquiera de sus bordes.
//
// La barra de titulo es propia para que encaje con el skin: el nombre del
// panel y dos botones, uno para desacoplarlo (o volver a acoplarlo) y otro
// para minimizarlo, que lo esconde hasta volver a pedirlo desde el menu.
class PanelDock : public QDockWidget {
    Q_OBJECT

public:
    // `id` es el nombre con el que se guarda la disposicion: no debe cambiar
    // entre versiones o las disposiciones guardadas dejan de encontrarlo.
    PanelDock(const QString& title, const QString& id, QWidget* content,
              QWidget* parent = nullptr);

    // Sin barra de titulo (el video a pantalla completa): solo el contenido.
    void setTitleBarShown(bool shown);

signals:
    // El usuario lo cerro con su boton (no una disposicion al recolocar).
    void userClosed();

protected:
    void paintEvent(QPaintEvent* event) override;

    // Flotando, los bordes del panel se agarran para estirarlo como una
    // ventana de Windows: el marco del contenido (lados y abajo) y la barra
    // de titulo (arriba y esquinas superiores).
    bool eventFilter(QObject* watched, QEvent* event) override;

private:
    void onFloatingChanged(bool floating);
    Qt::Edges edgesAt(const QPoint& globalPos) const;
    void setEdgeCursor(QWidget* widget, Qt::Edges edges);

    QWidget*    m_frame = nullptr;   // envuelve el contenido; su margen es el borde
    QWidget*    m_bar   = nullptr;
    QWidget*    m_noBar = nullptr;   // titulo vacio: QDockWidget no admite ninguno
    QLabel*     m_title = nullptr;
    FlatButton* m_float = nullptr;
    FlatButton* m_hide  = nullptr;
};
