#include "icons.h"

#include <QHash>
#include <QPainter>
#include <QPixmap>
#include <QSvgRenderer>
#include <cstdlib>

namespace {

const QString Floppy = R"(<path d="M5 3h11l5 5v11a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2z"/><path d="M7 3v5h8V3"/>)";
const QString Lens = R"(<circle cx="10.5" cy="10.5" r="7"/><path d="M21 21l-5.5-5.5"/>)";

const QHash<QString, QString> &shapes()
{
    static const QHash<QString, QString> s = {
        {"open", R"(<path d="M3 7a2 2 0 0 1 2-2h4l2 2h8a2 2 0 0 1 2 2v8a2 2 0 0 1-2 2H5a2 2 0 0 1-2-2z"/>)"},
        {"save", Floppy + R"(<path d="M7 21v-7h10v7"/>)"},
        {"save-as", R"(<path d="M12 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h11l5 5v3"/><path d="M7 3v5h8V3"/>)"
                    R"(<path d="M7 21v-7h5"/><path d="M14 22v-2.5l5.5-5.5 2.5 2.5-5.5 5.5z"/>)"},
        {"previous", R"(<path d="M19 12H5"/><path d="M12 19l-7-7 7-7"/>)"},
        {"next", R"(<path d="M5 12h14"/><path d="M12 5l7 7-7 7"/>)"},
        {"undo", R"(<path d="M9 14L4 9l5-5"/><path d="M4 9h10.5a5.5 5.5 0 0 1 0 11H11"/>)"},
        {"copy",R"(<rect x="9" y="9" width="12" height="12" rx="2"/>)"
                 R"(<path d="M5 15H4a1 1 0 0 1-1-1V4a1 1 0 0 1 1-1h10a1 1 0 0 1 1 1v1"/>)"},
        {"crop", R"(<path d="M6 2v14a2 2 0 0 0 2 2h14"/><path d="M18 22V8a2 2 0 0 0-2-2H2"/>)"},
        {"resize", R"(<path d="M15 3h6v6"/><path d="M9 21H3v-6"/><path d="M21 3l-7 7"/><path d="M3 21l7-7"/>)"},
        {"rotate-left", R"(<path d="M3 12a9 9 0 1 0 2.6-6.4L3 8"/><path d="M3 3v5h5"/>)"},
        {"rotate-right", R"(<path d="M21 12a9 9 0 1 1-2.6-6.4L21 8"/><path d="M21 3v5h-5"/>)"},
        {"flip-horizontal", R"(<path d="M12 2v2M12 7v3M12 13v3M12 19v3"/><path d="M9 6L3 18h6z"/><path d="M15 6l6 12h-6z"/>)"},
        {"flip-vertical", R"(<path d="M2 12h2M7 12h3M13 12h3M19 12h3"/><path d="M6 9L18 3v6z"/><path d="M6 15l12 6v-6z"/>)"},
        {"zoom-in", Lens + R"(<path d="M10.5 7.5v6M7.5 10.5h6"/>)"},
        {"zoom-out", Lens + R"(<path d="M7.5 10.5h6"/>)"},
        {"zoom-reset", Lens + R"(<rect x="8" y="8" width="5" height="5" rx="0.5"/>)"},
        {"fullscreen", R"(<path d="M8 3H5a2 2 0 0 0-2 2v3"/><path d="M21 8V5a2 2 0 0 0-2-2h-3"/>)"
                       R"(<path d="M3 16v3a2 2 0 0 0 2 2h3"/><path d="M16 21h3a2 2 0 0 0 2-2v-3"/>)"},
        {"exit", R"(<path d="M9 21H5a2 2 0 0 1-2-2V5a2 2 0 0 1 2-2h4"/><path d="M16 17l5-5-5-5"/><path d="M21 12H9"/>)"},
    };
    return s;
}

QPixmap render(const QString &name, const QColor &color, int size)
{
    const QString svg = QStringLiteral(
        R"(<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 24 24" fill="none" stroke="%1" )"
        R"(stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">%2</svg>)")
        .arg(color.name(), shapes().value(name));
    QPixmap pixmap(size, size);
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    QSvgRenderer(svg.toUtf8()).render(&painter, QRectF(0, 0, size, size));
    painter.end();
    return pixmap;
}

} // namespace

QIcon icons::icon(const QString &name, const QPalette &palette)
{
    const QColor normal = palette.color(QPalette::Active, QPalette::WindowText);
    QColor disabled = palette.color(QPalette::Disabled, QPalette::WindowText);
    if (std::abs(disabled.lightness() - normal.lightness()) < 80) {
        // Theme's disabled color is too close to normal; blend toward the background.
        const QColor bg = palette.color(QPalette::Window);
        auto mix = [](int n, int b) { return qRound(n * 0.4 + b * 0.6); };
        disabled = QColor(mix(normal.red(), bg.red()), mix(normal.green(), bg.green()), mix(normal.blue(), bg.blue()));
    }
    QIcon result;
    for (int size : {22, 44}) { // toolbar size, and 2× for HiDPI
        result.addPixmap(render(name, normal, size), QIcon::Normal);
        result.addPixmap(render(name, disabled, size), QIcon::Disabled);
    }
    return result;
}
