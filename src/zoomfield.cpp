#include "zoomfield.h"

#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QStyle>
#include <QStyleOptionSlider>
#include <QTimer>
#include <QToolButton>
#include <cmath>

#include "canvas.h"

namespace {

int toSlider(double zoom)
{
    return qRound(100 * std::log2(zoom));
}

double fromSlider(int value)
{
    return std::exp2(value / 100.0);
}

QString percent(double zoom)
{
    return QString::number(zoom * 100, 'f', 0);
}

} // namespace

// --- JumpSlider ------------------------------------------------------------

JumpSlider::JumpSlider(Qt::Orientation orientation, QWidget *parent) : QSlider(orientation, parent)
{
    setStyleSheet(R"(
        QSlider { min-height: 26px; }
        QSlider::groove:horizontal { height: 6px; border-radius: 3px; background: palette(mid); }
        QSlider::sub-page:horizontal { border-radius: 3px; background: palette(highlight); }
        QSlider::handle:horizontal {
            width: 20px; height: 20px; margin: -8px 0; border-radius: 11px;
            background: palette(button); border: 1px solid palette(dark);
        }
        QSlider::handle:horizontal:hover { background: palette(light); }
    )");
}

void JumpSlider::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        QStyleOptionSlider opt;
        initStyleOption(&opt);
        const QRect groove = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderGroove, this);
        const QRect handle = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
        if (!handle.contains(e->position().toPoint())) {
            const int pos = qRound(e->position().x()) - groove.x() - handle.width() / 2;
            setValue(QStyle::sliderValueFromPosition(minimum(), maximum(), pos, groove.width() - handle.width()));
        }
    }
    // The handle is now under the cursor, so the base class starts a drag.
    QSlider::mousePressEvent(e);
}

// --- ZoomPopup -------------------------------------------------------------

ZoomPopup::ZoomPopup(QAction *resetAction, QWidget *parent) : QFrame(parent, Qt::Popup)
{
    setFrameShape(QFrame::StyledPanel);
    m_slider = new JumpSlider(Qt::Horizontal);
    m_slider->setRange(toSlider(ImageCanvas::MinZoom), toSlider(ImageCanvas::MaxZoom));
    m_slider->setSingleStep(32);
    m_slider->setPageStep(100);
    m_slider->setFixedWidth(240);
    connect(m_slider, &QSlider::valueChanged, this, [this](int v) { Q_EMIT zoomRequested(fromSlider(v)); });
    auto reset = new QToolButton;
    reset->setAutoRaise(true);
    reset->setDefaultAction(resetAction);
    reset->setIconSize(QSize(22, 22));
    auto layout = new QHBoxLayout(this);
    layout->setContentsMargins(8, 4, 4, 4);
    layout->addWidget(m_slider);
    layout->addWidget(reset);
}

void ZoomPopup::setZoom(double zoom)
{
    const QSignalBlocker blocker(m_slider);
    m_slider->setValue(toSlider(zoom));
}

void ZoomPopup::keyPressEvent(QKeyEvent *e)
{
    if (e->key() == Qt::Key_Escape)
        close();
    else
        QFrame::keyPressEvent(e);
}

// --- ZoomField -------------------------------------------------------------

ZoomField::ZoomField(QAction *resetAction, QWidget *parent) : QLineEdit(parent)
{
    setFixedWidth(68);
    setAlignment(Qt::AlignCenter);
    setToolTip("Zoom: click for slider, double-click to type a percentage");
    endEdit();

    m_popup = new ZoomPopup(resetAction, this);
    connect(m_popup, &ZoomPopup::zoomRequested, this, &ZoomField::zoomRequested);
    // Wait out the double-click interval so a double-click doesn't also open the popup.
    m_clickTimer = new QTimer(this);
    m_clickTimer->setSingleShot(true);
    m_clickTimer->setInterval(QApplication::doubleClickInterval());
    connect(m_clickTimer, &QTimer::timeout, this, &ZoomField::showPopup);
}

void ZoomField::setZoom(double zoom)
{
    m_zoom = zoom;
    m_popup->setZoom(zoom);
    if (!m_editing)
        setText(percent(zoom) + "%");
}

void ZoomField::showPopup()
{
    m_popup->setZoom(m_zoom);
    m_popup->adjustSize();
    const QPoint pos = mapToGlobal(QPoint(0, height()));
    m_popup->move(pos.x() + (width() - m_popup->width()) / 2, pos.y() + 2);
    m_popup->show();
}

// --- editing ---------------------------------------------------------------

void ZoomField::beginEdit()
{
    m_editing = true;
    setReadOnly(false);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::IBeamCursor);
    setText(percent(m_zoom));
    setFocus();
    selectAll();
    qApp->installEventFilter(this);
}

void ZoomField::endEdit()
{
    if (m_editing)
        qApp->removeEventFilter(this);
    m_editing = false;
    setReadOnly(true);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    deselect();
    setText(percent(m_zoom) + "%");
    Q_EMIT editingDone();
}

void ZoomField::commit()
{
    QString t = text().trimmed();
    while (t.endsWith('%'))
        t.chop(1);
    t = t.trimmed().replace(',', '.');
    endEdit();
    bool ok = false;
    const double pct = t.toDouble(&ok);
    if (ok && pct > 0 && std::isfinite(pct))
        Q_EMIT zoomRequested(std::clamp(pct / 100, ImageCanvas::MinZoom, ImageCanvas::MaxZoom));
}

bool ZoomField::eventFilter(QObject *, QEvent *e)
{
    // A click anywhere outside the field accepts the typed value.
    if (m_editing && e->type() == QEvent::MouseButtonPress) {
        auto me = static_cast<QMouseEvent *>(e);
        if (!rect().contains(mapFromGlobal(me->globalPosition().toPoint())))
            commit();
    }
    return false;
}

// --- events ----------------------------------------------------------------

void ZoomField::mousePressEvent(QMouseEvent *e)
{
    if (m_editing)
        QLineEdit::mousePressEvent(e);
    else if (e->button() == Qt::LeftButton)
        m_clickTimer->start();
}

void ZoomField::mouseDoubleClickEvent(QMouseEvent *e)
{
    if (m_editing) {
        QLineEdit::mouseDoubleClickEvent(e);
    } else if (e->button() == Qt::LeftButton) {
        m_clickTimer->stop();
        beginEdit();
    }
}

void ZoomField::keyPressEvent(QKeyEvent *e)
{
    if (!m_editing)
        QLineEdit::keyPressEvent(e);
    else if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter)
        commit();
    else if (e->key() == Qt::Key_Escape)
        endEdit();
    else
        QLineEdit::keyPressEvent(e);
}

void ZoomField::focusOutEvent(QFocusEvent *e)
{
    QLineEdit::focusOutEvent(e);
    if (m_editing)
        commit();
}
