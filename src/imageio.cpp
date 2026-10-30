#include "imageio.h"

#include <QFile>
#include <QFileInfo>
#include <QHash>
#include <QImageReader>
#include <QImageWriter>
#include <QMovie>
#include <QPainter>
#include <algorithm>
#include <numeric>
#include <vector>

namespace {

// Extra file extensions for formats the plugins register under another name.
// The reader decides the format from content, so these open fine.
const QHash<QString, QString> ExtensionAliases = {
    {"jpe", "jpeg"}, {"jfif", "jpeg"}, {"mpo", "jpeg"}, {"apng", "png"}, {"dib", "bmp"},
    {"hif", "heif"}, {"heics", "heic"}, {"heifs", "heif"}, {"avifs", "avif"},
    {"j2k", "jp2"}, {"j2c", "jp2"}, {"jpc", "jp2"}, {"jpf", "jp2"}, {"jpx", "jp2"},
    {"pnm", "ppm"}, {"rgba", "rgb"}, {"bw", "rgb"},
};

// (filter name, extensions) offered in the Save As dialog, in display order.
const QList<QPair<QString, QStringList>> SaveChoices = {
    {"PNG", {"png"}},
    {"JPEG", {"jpg", "jpeg"}},
    {"WebP", {"webp"}},
    {"AVIF", {"avif"}},
    {"HEIC", {"heic"}},
    {"JPEG XL", {"jxl"}},
    {"TIFF", {"tif", "tiff"}},
    {"BMP", {"bmp"}},
    {"GIF", {"gif"}},
    {"ICO", {"ico"}},
    {"TGA", {"tga"}},
    {"QOI", {"qoi"}},
    {"PPM", {"ppm"}},
};

// Standard IJG luminance quantization table (quality 50).
constexpr int IjgLuma[64] = {
    16, 11, 10, 16, 24, 40, 51, 61, 12, 12, 14, 19, 26, 58, 60, 55,
    14, 13, 16, 24, 40, 57, 69, 56, 14, 17, 22, 29, 51, 87, 80, 62,
    18, 22, 37, 56, 68, 109, 103, 77, 24, 35, 55, 64, 81, 104, 113, 92,
    49, 64, 78, 87, 103, 121, 120, 101, 72, 92, 95, 98, 112, 100, 103, 99,
};

QSet<QString> names(const QList<QByteArray> &formats)
{
    QSet<QString> result;
    for (const QByteArray &f : formats)
        result.insert(QString::fromLatin1(f).toLower());
    return result;
}

const QSet<QString> &writable()
{
    static const QSet<QString> w = names(QImageWriter::supportedImageFormats());
    return w;
}

QString extension(const QString &path)
{
    return QFileInfo(path).suffix().toLower();
}

// Composite onto white, for formats without alpha support.
QImage flatten(const QImage &img)
{
    if (!img.hasAlphaChannel())
        return img;
    QImage out(img.size(), QImage::Format_RGB32);
    out.fill(Qt::white);
    QPainter painter(&out);
    painter.drawImage(0, 0, img);
    painter.end();
    return out;
}

// Luminance (or first) quantization table of a JPEG file; empty if none.
std::vector<int> jpegQuantTable(QFile &f)
{
    std::vector<std::pair<int, std::vector<int>>> tables;
    auto byte = [&f]() -> int {
        char c;
        return f.getChar(&c) ? uchar(c) : -1;
    };
    if (byte() != 0xFF || byte() != 0xD8)
        return {};
    for (;;) {
        if (byte() != 0xFF)
            break;
        int marker;
        do
            marker = byte();
        while (marker == 0xFF);
        if (marker < 0 || marker == 0xDA || marker == 0xD9) // start of scan / end of image
            break;
        if (marker == 0x01 || (marker >= 0xD0 && marker <= 0xD7))
            continue;
        const int hi = byte(), lo = byte();
        if (hi < 0 || lo < 0)
            break;
        const int length = (hi << 8 | lo) - 2;
        if (length < 0)
            break;
        if (marker != 0xDB) {
            if (!f.seek(f.pos() + length))
                break;
            continue;
        }
        const QByteArray seg = f.read(length);
        const auto *d = reinterpret_cast<const uchar *>(seg.constData());
        qsizetype i = 0;
        while (i < seg.size()) {
            const int precision = d[i] >> 4, id = d[i] & 15;
            const int step = precision ? 2 : 1;
            ++i;
            if (i + 64 * step > seg.size())
                return {};
            std::vector<int> table(64);
            for (int k = 0; k < 64; ++k, i += step)
                table[k] = precision ? (d[i] << 8 | d[i + 1]) : d[i];
            auto it = std::find_if(tables.begin(), tables.end(), [id](const auto &t) { return t.first == id; });
            if (it != tables.end())
                it->second = std::move(table);
            else
                tables.emplace_back(id, std::move(table));
        }
    }
    if (tables.empty())
        return {};
    auto it = std::find_if(tables.begin(), tables.end(), [](const auto &t) { return t.first == 0; });
    return (it != tables.end() ? it : tables.begin())->second;
}

} // namespace

const QSet<QString> &imageio::readableExtensions()
{
    static const QSet<QString> exts = [] {
        const QSet<QString> formats = names(QImageReader::supportedImageFormats());
        QSet<QString> result;
        for (const QString &f : formats)
            result.insert("." + f);
        for (auto it = ExtensionAliases.cbegin(); it != ExtensionAliases.cend(); ++it)
            if (formats.contains(it.value()))
                result.insert("." + it.key());
        return result;
    }();
    return exts;
}

const QList<QPair<QString, QString>> &imageio::saveFilters()
{
    static const QList<QPair<QString, QString>> filters = [] {
        QList<QPair<QString, QString>> result;
        for (const auto &[name, choices] : SaveChoices) {
            QStringList exts;
            for (const QString &e : choices)
                if (writable().contains(e))
                    exts << e;
            if (!exts.isEmpty()) {
                QStringList patterns;
                for (const QString &e : exts)
                    patterns << "*." + e;
                result.append({QString("%1 (%2)").arg(name, patterns.join(' ')), exts.first()});
            }
        }
        return result;
    }();
    return filters;
}

bool imageio::canSave(QString ext)
{
    if (ext.startsWith('.'))
        ext.remove(0, 1);
    return writable().contains(ext.toLower());
}

bool imageio::load(const QString &path, Loaded &out, QString *error)
{
    QImageReader reader(path);
    reader.setAutoTransform(true);
    reader.setDecideFormatFromContent(true);
    if (!reader.canRead()) {
        *error = reader.errorString();
        return false;
    }
    out.format = QString::fromLatin1(reader.format()).toUpper();
    if (reader.supportsAnimation() && reader.imageCount() != 1) {
        auto movie = new QMovie(path);
        if (movie->isValid() && movie->frameCount() != 1) {
            out.movie = movie;
            return true;
        }
        delete movie;
    }
    out.image = reader.read();
    if (out.image.isNull()) {
        *error = reader.errorString();
        return false;
    }
    return true;
}

int imageio::estimateQuality(const QString &path)
{
    // JPEG quality is derived from the luminance quantization table, inverting
    // the IJG scaling formula. Lossless WebP maps to 100 (Qt's lossless setting).
    // Other formats don't record their quality.
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return 0;
    const QByteArray head = f.peek(16);
    if (head.startsWith("\xFF\xD8")) {
        const std::vector<int> table = jpegQuantTable(f);
        if (table.empty())
            return 0;
        if (*std::max_element(table.begin(), table.end()) == 1)
            return 100;
        const double scale = std::accumulate(table.begin(), table.end(), 0.0) * 100
            / std::accumulate(std::begin(IjgLuma), std::end(IjgLuma), 0);
        const double quality = scale <= 100 ? (200 - scale) / 2 : 5000 / scale;
        return std::clamp(qRound(quality), 1, 100);
    }
    if (head.size() == 16 && head.startsWith("RIFF") && head.mid(8, 4) == "WEBP" && head.mid(12, 4) == "VP8L")
        return 100;
    return 0;
}

bool imageio::save(const QImage &img, const QString &path, int quality, QString *error)
{
    const QString ext = extension(path);
    if (!writable().contains(ext)) {
        *error = QString("Unsupported format: .%1").arg(ext);
        return false;
    }
    const QImage source = (ext == "jpg" || ext == "jpeg") ? flatten(img) : img;
    QImageWriter writer(path, ext.toLatin1());
    writer.setQuality(quality);
    if (writer.write(source))
        return true;
    if (source.hasAlphaChannel()) {
        // Format can't store alpha; retry flattened.
        QImageWriter retry(path, ext.toLatin1());
        retry.setQuality(quality);
        if (retry.write(flatten(source)))
            return true;
    }
    *error = writer.errorString();
    return false;
}
