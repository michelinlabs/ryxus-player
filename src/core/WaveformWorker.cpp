#include "core/WaveformWorker.h"

#include <QDir>

#include <algorithm>
#include <cmath>
#include <vector>

#include "miniaudio.h"

WaveformWorker::WaveformWorker(QObject* parent)
    : QObject(parent)
{
}

void WaveformWorker::cancel()
{
    m_pending.clear();
}

void WaveformWorker::analyze(const QString& path, int buckets)
{
    m_pending = path;
    buckets = std::clamp(buckets, 64, 8192);

    // Se decodifica a mono y a baja frecuencia: solo interesa la envolvente,
    // asi que 8 kHz basta y hace el analisis varias veces mas rapido.
    constexpr int kAnalysisRate = 8000;

    ma_decoder_config config = ma_decoder_config_init(ma_format_f32, 1, kAnalysisRate);

    ma_decoder decoder;
    const std::wstring native = QDir::toNativeSeparators(path).toStdWString();
    if (ma_decoder_init_file_w(native.c_str(), &config, &decoder) != MA_SUCCESS) {
        emit failed(path);
        return;
    }

    ma_uint64 totalFrames = 0;
    ma_decoder_get_length_in_pcm_frames(&decoder, &totalFrames);

    QVector<float> peaks(buckets, 0.0f);

    if (totalFrames == 0) {
        ma_decoder_uninit(&decoder);
        emit failed(path);
        return;
    }

    const double framesPerBucket = double(totalFrames) / double(buckets);

    std::vector<float> chunk(4096);
    ma_uint64 processed = 0;
    float bucketPeak = 0.0f;
    int   bucketIndex = 0;

    for (;;) {
        if (m_pending != path) {          // el usuario ya cambio de pista
            ma_decoder_uninit(&decoder);
            return;
        }

        ma_uint64 framesRead = 0;
        ma_decoder_read_pcm_frames(&decoder, chunk.data(), chunk.size(), &framesRead);
        if (framesRead == 0)
            break;

        for (ma_uint64 i = 0; i < framesRead; ++i) {
            bucketPeak = std::max(bucketPeak, std::fabs(chunk[size_t(i)]));
            ++processed;

            const int target = std::min(int(double(processed) / framesPerBucket), buckets - 1);
            if (target != bucketIndex) {
                peaks[bucketIndex] = bucketPeak;
                bucketIndex = target;
                bucketPeak = 0.0f;
            }
        }
    }

    if (bucketIndex < buckets)
        peaks[bucketIndex] = std::max(peaks[bucketIndex], bucketPeak);

    ma_decoder_uninit(&decoder);

    // Normalizacion suave: los archivos muy comprimidos quedarian planos a 1.0
    // y los muy silenciosos, invisibles.
    const float maxPeak = *std::max_element(peaks.constBegin(), peaks.constEnd());
    if (maxPeak > 0.0001f) {
        const float scale = 1.0f / maxPeak;
        for (float& value : peaks)
            value = std::pow(std::min(value * scale, 1.0f), 0.75f);
    }

    if (m_pending == path)
        emit ready(path, peaks);
}
