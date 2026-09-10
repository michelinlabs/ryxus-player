#pragma once

#include <QDialog>
#include <QImage>

class QComboBox;
class QLabel;

// Recorte de caratula antes de escribirla en el archivo.
//
// Las caratulas se muestran siempre cuadradas, asi que una foto apaisada
// metida tal cual sale deformada o con bandas. Aqui se elige que cuadrado de
// la imagen se queda y a que resolucion se guarda.
class CoverCropDialog : public QDialog {
    Q_OBJECT

public:
    explicit CoverCropDialog(const QImage& source, QWidget* parent = nullptr);

    // Recorte aplicado y reescalado a la resolucion elegida.
    QImage result() const;

private:
    class CropView;

    void buildUi(const QImage& source);

    CropView*  m_view       = nullptr;
    QComboBox* m_resolution = nullptr;
    QLabel*    m_summary    = nullptr;
};
