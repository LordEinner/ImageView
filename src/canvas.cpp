#include "canvas.h"

#include <QApplication>
#include <QMouseEvent>
#include <QMovie>
#include <QNativeGestureEvent>
#include <QPainter>
#include <QPainterPath>
#include <QTimer>
#include <cmath>

namespace {
// Above this many pixels the downscaled pixmap cache is skipped.
constexpr qint64 MaxCachePixels = 50'000'000;
constexpr int CursorHideMs = 1500;
} // namespace

ImageCanvas::ImageCanvas(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    // The whole widget is repainted with an opaque fill.
    setAttribute(Qt::WA_OpaquePaintEvent);
    m_cursorTimer = new QTimer(this);
    m_cursorTimer->setSingleShot(true);
    m_cursorTimer->setInterval(CursorHideMs);
    connect(m_cursorTimer, &QTimer::timeout, this, [this] { setCursor(Qt::BlankCursor); });
}

// --- content ---------------------------------------------------------------

void ImageCanvas::setImage(const QImage &image)
{
    stopMovie();
    m_image = image;
    m_message.clear();
    reset();
}

void ImageCanvas::setMovie(QMovie *movie)
{
    stopMovie();
    m_movie = movie;
    movie->setParent(this);
    connect(movie, &QMovie::frameChanged, this, &ImageCanvas::onFrame);
    movie->jumpToFrame(0);
    m_image = movie->currentImage();
    m_message.clear();
    reset();
    movie->start();
}

void ImageCanvas::clear(const QString &message)
{
    stopMovie();
    m_image = QImage();
    m_message = message;
    reset();
}

void ImageCanvas::stopMovie()
{
    if (m_movie) {
        m_movie->stop();
        disconnect(m_movie, nullptr, this, nullptr);
        m_movie->deleteLater();
        m_movie = nullptr;
    }
}

void ImageCanvas::onFrame()
{
    m_image = m_movie->currentImage();
    update();
}

void ImageCanvas::reset()
{
    m_selection = QRect();
    m_selStart.reset();
    m_cache = QPixmap();
    m_cacheKey.reset();
    resetZoom();
}

// --- view ------------------------------------------------------------------

void ImageCanvas::setFullscreen(bool fullscreen)
{
    m_fullscreen = fullscreen;
    if (fullscreen) {
        m_cursorTimer->start();
    } else {
        m_cursorTimer->stop();
        unsetCursor();
    }
    resetZoom();
}

void ImageCanvas::resetZoom()
{
    m_auto = true;
    applyAuto();
    update();
    Q_EMIT viewChanged();
}

void ImageCanvas::zoomBy(double factor)
{
    zoomBy(factor, QPointF(width() / 2.0, height() / 2.0));
}

void ImageCanvas::zoomBy(double factor, QPointF anchor)
{
    if (!hasImage())
        return;
    const QPointF imagePt = (anchor - m_offset) / scale();
    m_zoom = std::clamp(m_zoom * factor, MinZoom, MaxZoom);
    m_auto = false;
    m_offset = anchor - imagePt * scale();
    clampOffset();
    update();
    Q_EMIT viewChanged();
}

void ImageCanvas::setZoom(double zoom)
{
    zoomBy(zoom / m_zoom);
}

double ImageCanvas::scale() const
{
    return m_zoom / devicePixelRatioF();
}

void ImageCanvas::applyAuto()
{
    if (!hasImage())
        return;
    const double dpr = devicePixelRatioF();
    const double fit = std::min(width() * dpr / m_image.width(), height() * dpr / m_image.height());
    m_zoom = m_fullscreen ? fit : std::min(1.0, fit);
    clampOffset();
}

void ImageCanvas::clampOffset()
{
    if (!hasImage())
        return;
    const double s = scale();
    double x = m_offset.x(), y = m_offset.y();
    const double w = m_image.width() * s, h = m_image.height() * s;
    x = w <= width() ? (width() - w) / 2 : std::min(0.0, std::max(width() - w, x));
    y = h <= height() ? (height() - h) / 2 : std::min(0.0, std::max(height() - h, y));
    m_offset = QPointF(x, y);
}

QPoint ImageCanvas::toImage(QPointF pos) const
{
    const QPointF p = (pos - m_offset) / scale();
    return QPoint(std::clamp(qRound(p.x()), 0, m_image.width()), std::clamp(qRound(p.y()), 0, m_image.height()));
}

// --- painting --------------------------------------------------------------

void ImageCanvas::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), m_fullscreen ? QColor(0, 0, 0) : QColor(40, 40, 40));
    if (!hasImage()) {
        p.setPen(QColor(170, 170, 170));
        p.drawText(rect(), Qt::AlignCenter, m_message);
        return;
    }
    const double s = scale();
    const QRectF target(m_offset, QSizeF(m_image.width() * s, m_image.height() * s));
    const int scaledW = qRound(m_image.width() * m_zoom), scaledH = qRound(m_image.height() * m_zoom);
    if (m_zoom < 1 && !m_movie && qint64(scaledW) * scaledH <= MaxCachePixels) {
        const qreal dpr = devicePixelRatioF();
        const CacheKey key{m_image.cacheKey(), scaledW, scaledH, dpr};
        if (key != m_cacheKey) {
            m_cache = QPixmap::fromImage(m_image.scaled(std::max(1, scaledW), std::max(1, scaledH),
                                                        Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
            m_cache.setDevicePixelRatio(dpr);
            m_cacheKey = key;
        }
        p.drawPixmap(target.topLeft(), m_cache);
    } else {
        const QRectF visible = QRectF(rect()).intersected(target);
        const QRectF source((visible.topLeft() - m_offset) / s, visible.size() / s);
        p.setRenderHint(QPainter::SmoothPixmapTransform, m_zoom < 4);
        p.drawImage(visible, m_image, source);
    }
    if (!m_selection.isEmpty())
        paintSelection(p, target, s);
}

void ImageCanvas::paintSelection(QPainter &p, const QRectF &target, double s)
{
    const QRectF r(m_offset + QPointF(m_selection.topLeft()) * s, QSizeF(m_selection.size()) * s);
    QPainterPath shade;
    shade.addRect(target);
    shade.addRect(r);
    p.fillPath(shade, QColor(0, 0, 0, 110));
    p.setRenderHint(QPainter::Antialiasing, false);
    p.setPen(QPen(QColor(0, 0, 0), 1));
    p.drawRect(r);
    p.setPen(QPen(QColor(255, 255, 255), 1, Qt::DashLine));
    p.drawRect(r);
}

// --- input -----------------------------------------------------------------

void ImageCanvas::resizeEvent(QResizeEvent *)
{
    if (m_auto)
        applyAuto();
    else
        clampOffset();
    Q_EMIT viewChanged();
}

void ImageCanvas::wheelEvent(QWheelEvent *e)
{
    const int delta = e->angleDelta().y();
    if (delta)
        zoomBy(std::pow(1.25, delta / 120.0), e->position());
}

bool ImageCanvas::event(QEvent *e)
{
    // Touchpad pinch (Wayland pointer gestures / X11 XInput 2.4).
    if (e->type() == QEvent::NativeGesture) {
        auto g = static_cast<QNativeGestureEvent *>(e);
        if (g->gestureType() == Qt::ZoomNativeGesture) {
            zoomBy(1 + g->value(), g->position());
            return true;
        }
    }
    return QWidget::event(e);
}

void ImageCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::RightButton || e->button() == Qt::MiddleButton) {
        // Becomes a pan once the mouse moves; a right click without moving opens the menu.
        m_panStart = e->position();
        m_panOffset = m_offset;
        m_panButton = e->button();
        m_panned = false;
    } else if (e->button() == Qt::LeftButton && hasImage()) {
        m_selStart = toImage(e->position());
        m_selection = QRect();
        update();
        Q_EMIT viewChanged();
    }
}

void ImageCanvas::mouseMoveEvent(QMouseEvent *e)
{
    if (m_fullscreen && !m_panStart) {
        unsetCursor();
        m_cursorTimer->start();
    }
    if (!hasImage())
        return;
    if (m_selStart) {
        const QPoint a = *m_selStart, b = toImage(e->position());
        m_selection = QRect(std::min(a.x(), b.x()), std::min(a.y(), b.y()), std::abs(a.x() - b.x()),
                            std::abs(a.y() - b.y()));
        update();
        Q_EMIT viewChanged();
    } else if (m_panStart) {
        const QPointF delta = e->position() - *m_panStart;
        if (!m_panned) {
            if (delta.manhattanLength() < QApplication::startDragDistance())
                return;
            m_panned = true;
            setCursor(Qt::ClosedHandCursor);
        }
        m_offset = m_panOffset + delta;
        clampOffset();
        update();
    }
}

void ImageCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_selStart) {
        m_selStart.reset();
        if (m_selection.width() < 2 || m_selection.height() < 2) {
            m_selection = QRect();
            update();
            Q_EMIT viewChanged();
        }
    } else if (e->button() == m_panButton) {
        if (e->button() == Qt::RightButton && !m_panned)
            Q_EMIT contextMenuRequested(e->globalPosition().toPoint());
        m_panStart.reset();
        m_panButton = Qt::NoButton;
        unsetCursor();
    }
}

void ImageCanvas::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton)
        Q_EMIT doubleClicked();
    else
        QWidget::mouseDoubleClickEvent(e);
}
