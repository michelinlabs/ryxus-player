#pragma once

#include "core/Effects.h"

#include <QVector>
#include <QWidget>

class QCheckBox;
class QLabel;
class QSlider;

// Rejilla de dispositivos de efecto, al estilo de la cadena de Ableton: cada
// modulo es una tarjeta con su casilla y sus mandos.
//
// Va a la derecha del ecualizador, ocupando el hueco que quedaba libre en esa
// franja, para no robarle alto al plot. Se reparte en dos filas de tres.
//
// Las tarjetas se construyen a partir de la tabla de Effects::info(), asi que
// anadir un efecto nuevo al motor lo hace aparecer aqui sin tocar esta clase.
class EffectsRack : public QWidget {
    Q_OBJECT

public:
    explicit EffectsRack(Effects* effects, QWidget* parent = nullptr);

    void loadFromSettings();
    void resetAll();

    // La rejilla entera. Sin esto el ecualizador se queda con su ancho maximo
    // y el rack acaba desplazandose aunque hubiera sitio de sobra.
    QSize sizeHint() const override;

signals:
    void activityChanged(bool anyEnabled);   // hay algun efecto encendido

private:
    struct ParamRow {
        int      device = 0;
        int      index  = 0;
        QSlider* slider = nullptr;
        QLabel*  value  = nullptr;
    };

    struct Card {
        int        device = 0;
        QWidget*   frame  = nullptr;
        QCheckBox* power  = nullptr;
    };

    void buildUi();
    QWidget* buildCard(int device);
    void setDeviceEnabled(int device, bool on, bool persist);
    void applyParam(const ParamRow& row, int sliderValue, bool persist);
    void refreshCardLook(const Card& card);

    // Los deslizadores de Qt son enteros: se trabaja en centesimas del rango
    // real del parametro y se convierte al leer y escribir.
    static int   toSlider(const Effects::ParamInfo& info, float value);
    static float fromSlider(const Effects::ParamInfo& info, int ticks);
    static QString formatValue(const Effects::ParamInfo& info, float value);

    Effects* m_effects = nullptr;
    QVector<Card>     m_cards;
    QVector<ParamRow> m_rows;
    bool m_loading = false;
};
