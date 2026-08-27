#include "core/LibraryScanner.h"
#include "core/MetadataService.h"

#include <QCollator>
#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

#include <algorithm>

namespace {
constexpr int kBatchSize = 40;
}

LibraryScanner::LibraryScanner(QObject* parent)
    : QObject(parent)
{
}

void LibraryScanner::scanFolder(const QString& folder, bool recursive, quint64 requestId)
{
    const quint64 generation = m_generation.load();

    QStringList files;
    {
        QStringList filters;
        for (const QString& suffix : TrackInfo::supportedSuffixes())
            filters << QStringLiteral("*.") + suffix;

        QDirIterator it(folder, filters, QDir::Files | QDir::Readable,
                        recursive ? QDirIterator::Subdirectories
                                  : QDirIterator::NoIteratorFlags);
        while (it.hasNext()) {
            files << it.next();
            if (m_generation.load() != generation)
                return;
        }
    }

    // Orden natural: "10 - tema" va despues de "9 - tema", no antes.
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(files.begin(), files.end(),
              [&collator](const QString& a, const QString& b) {
                  return collator.compare(a, b) < 0;
              });

    QVector<TrackInfo> batch;
    batch.reserve(kBatchSize);

    for (const QString& file : std::as_const(files)) {
        if (m_generation.load() != generation)
            return;

        // Sin caratula: en un listado de cientos de pistas seria el 90 % del coste.
        TrackInfo info = MetadataService::read(file, false);
        if (!info.isValid())
            continue;

        batch.append(info);
        if (batch.size() >= kBatchSize) {
            emit batchReady(batch, requestId);
            batch.clear();
        }
    }

    if (!batch.isEmpty())
        emit batchReady(batch, requestId);

    if (m_generation.load() == generation)
        emit scanFinished(folder, int(files.size()), requestId);
}
