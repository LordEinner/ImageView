// Image loading and saving through Qt's image plugins (plus KImageFormats,
// when installed, for AVIF, JPEG XL, PSD, QOI and more).
#pragma once

#include <QImage>
#include <QList>
#include <QPair>
#include <QSet>
#include <QString>

class QMovie;

namespace imageio {

struct Loaded {
    QImage image;
    QMovie *movie = nullptr; // set instead of image for animations; caller takes ownership
    QString format;
};

inline const QSet<QString> LossyExtensions = {"jpg", "jpeg", "webp", "avif", "jxl"};

// Lowercase extensions with a leading dot, e.g. ".png".
const QSet<QString> &readableExtensions();
// (filter string, default extension) for writable formats, in display order.
const QList<QPair<QString, QString>> &saveFilters();
bool canSave(QString ext);

bool load(const QString &path, Loaded &out, QString *error);
// Best guess of the quality a file was saved with, or 0 if unknown.
int estimateQuality(const QString &path);
bool save(const QImage &img, const QString &path, int quality, QString *error);

} // namespace imageio
