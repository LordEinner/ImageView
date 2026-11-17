// Build-time helper: renders an SVG into a multi-size Windows .ico
// (PNG-compressed entries, supported since Windows Vista).
// Usage: make_ico <input.svg> <output.ico>
#include <QBuffer>
#include <QFile>
#include <QImage>
#include <QPainter>
#include <QSvgRenderer>
#include <QtEndian>
#include <cstdio>

int main(int argc, char *argv[])
{
    if (argc != 3) {
        std::fprintf(stderr, "usage: make_ico <input.svg> <output.ico>\n");
        return 2;
    }
    QSvgRenderer svg(QString::fromLocal8Bit(argv[1]));
    if (!svg.isValid()) {
        std::fprintf(stderr, "make_ico: cannot read %s\n", argv[1]);
        return 1;
    }

    const int sizes[] = {16, 20, 24, 32, 40, 48, 64, 128, 256};
    QList<QByteArray> pngs;
    for (int s : sizes) {
        QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
        img.fill(Qt::transparent);
        QPainter p(&img);
        p.setRenderHint(QPainter::Antialiasing);
        svg.render(&p);
        p.end();
        QByteArray png;
        QBuffer buf(&png);
        buf.open(QIODevice::WriteOnly);
        img.convertToFormat(QImage::Format_ARGB32).save(&buf, "PNG");
        pngs << png;
    }

    QByteArray ico;
    auto u16 = [&ico](quint16 v) { v = qToLittleEndian(v); ico.append(reinterpret_cast<const char *>(&v), 2); };
    auto u32 = [&ico](quint32 v) { v = qToLittleEndian(v); ico.append(reinterpret_cast<const char *>(&v), 4); };
    const int count = int(pngs.size());
    u16(0); // reserved
    u16(1); // type: icon
    u16(quint16(count));
    quint32 offset = 6 + 16 * count;
    for (int i = 0; i < count; ++i) {
        const int s = sizes[i];
        ico.append(char(s >= 256 ? 0 : s)); // width, 0 means 256
        ico.append(char(s >= 256 ? 0 : s)); // height
        ico.append(char(0));                // palette colours
        ico.append(char(0));                // reserved
        u16(1);                             // colour planes
        u16(32);                            // bits per pixel
        u32(quint32(pngs[i].size()));
        u32(offset);
        offset += quint32(pngs[i].size());
    }
    for (const QByteArray &png : pngs)
        ico += png;

    QFile out(QString::fromLocal8Bit(argv[2]));
    if (!out.open(QIODevice::WriteOnly) || out.write(ico) != ico.size()) {
        std::fprintf(stderr, "make_ico: cannot write %s\n", argv[2]);
        return 1;
    }
    return 0;
}
