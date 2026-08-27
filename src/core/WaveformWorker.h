#pragma once

#include <QObject>
#include <QString>
#include <QVector>

// Calcula la envolvente (picos) de un archivo completo en un hilo aparte,
// para que la barra de posicion pueda dibujar la forma de onda como en el
// skin de referencia sin congelar la interfaz.
class WaveformWorker : public QObject {
    Q_OBJECT

public:
    static constexpr int kDefaultBuckets = 1200;

    explicit WaveformWorker(QObject* parent = nullptr);

public slots:
    void analyze(const QString& path, int buckets = kDefaultBuckets);
    void cancel();

signals:
    void ready(const QString& path, const QVector<float>& peaks);
    void failed(const QString& path);

private:
    // Se compara contra la ruta pedida para descartar resultados obsoletos
    // cuando el usuario cambia de pista antes de que termine el analisis.
    QString m_pending;
};
