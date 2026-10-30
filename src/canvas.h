// Widget that displays an image with zoom, panning and rectangular selection.
#pragma once

#include <QImage>
#include <QPixmap>
#include <QPointF>
#include <QRect>
#include <QWidget>
#include <optional>

class QMovie;
class QTimer;

class ImageCanvas : public QWidget {
    Q_OBJECT
public:
    static constexpr double MinZoom = 0.01;
    static constexpr double MaxZoom = 64.0;

    explicit ImageCanvas(QWidget *parent = nullptr);

    const QImage &image() const { return m_image; }
    bool hasImage() const { return !m_image.isNull(); }
    bool isAnimated() const { return m_movie != nullptr; }
    void setImage(const QImage &image);
    void setMovie(QMovie *movie); // takes ownership
    void clear(const QString &message = QString());

    double zoom() const { return m_zoom; }
    QRect selection() const { return m_selection; }
    void setFullscreen(bool fullscreen);
    void resetZoom();
    void zoomBy(double factor);
    void zoomBy(double factor, QPointF anchor);
    void setZoom(double zoom);

Q_SIGNALS:
    void viewChanged();
    void doubleClicked();
    void contextMenuRequested(QPoint globalPos);

protected:
    void paintEvent(QPaintEvent *) override;
    void resizeEvent(QResizeEvent *) override;
    void wheelEvent(QWheelEvent *e) override;
    bool event(QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;

private:
    struct CacheKey {
        qint64 image;
        int w, h;
        qreal dpr;
        bool operator==(const CacheKey &) const = default;
    };

    void stopMovie();
    void onFrame();
    void reset();
    double scale() const; // logical widget pixels per image pixel
    void applyAuto();
    void clampOffset();
    QPoint toImage(QPointF pos) const;
    void paintSelection(QPainter &p, const QRectF &target, double s);

    QImage m_image;
    QMovie *m_movie = nullptr;
    QString m_message;
    bool m_fullscreen = false;
    // Zoom is image pixels -> device pixels, so 1.0 is a true 1:1 on HiDPI too.
    double m_zoom = 1.0;
    bool m_auto = true;
    QPointF m_offset;
    QRect m_selection;
    std::optional<QPoint> m_selStart;
    std::optional<QPointF> m_panStart;
    QPointF m_panOffset;
    Qt::MouseButton m_panButton = Qt::NoButton;
    bool m_panned = false;
    QPixmap m_cache;
    std::optional<CacheKey> m_cacheKey;
    QTimer *m_cursorTimer;
};
