// Toolbar zoom field: shows the zoom level, opens a slider popup on click,
// and accepts a typed percentage on double-click.
#pragma once

#include <QFrame>
#include <QLineEdit>
#include <QSlider>

class QAction;
class QTimer;

// Slider with a large round handle that jumps to wherever it is clicked.
class JumpSlider : public QSlider {
    Q_OBJECT
public:
    explicit JumpSlider(Qt::Orientation orientation, QWidget *parent = nullptr);

protected:
    void mousePressEvent(QMouseEvent *e) override;
};

class ZoomPopup : public QFrame {
    Q_OBJECT
public:
    explicit ZoomPopup(QAction *resetAction, QWidget *parent = nullptr);
    void setZoom(double zoom);

Q_SIGNALS:
    void zoomRequested(double zoom);

protected:
    void keyPressEvent(QKeyEvent *e) override;

private:
    JumpSlider *m_slider;
};

class ZoomField : public QLineEdit {
    Q_OBJECT
public:
    explicit ZoomField(QAction *resetAction, QWidget *parent = nullptr);
    void setZoom(double zoom);

Q_SIGNALS:
    void zoomRequested(double zoom);
    void editingDone();

protected:
    bool eventFilter(QObject *obj, QEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseDoubleClickEvent(QMouseEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;

private:
    void showPopup();
    void beginEdit();
    void endEdit();
    void commit();

    double m_zoom = 1.0;
    bool m_editing = false;
    ZoomPopup *m_popup;
    QTimer *m_clickTimer;
};
