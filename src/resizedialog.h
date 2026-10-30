// Dialog for resizing an image.
#pragma once

#include <QDialog>
#include <QSize>
#include <optional>

class QCheckBox;
class QDoubleSpinBox;
class QSpinBox;

class ResizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit ResizeDialog(QSize size, QWidget *parent = nullptr);
    QSize newSize() const;

private:
    void setValues(std::optional<double> w, std::optional<double> h, std::optional<double> pct);
    void widthChanged(int w);
    void heightChanged(int h);
    void percentChanged(double pct);

    QSize m_orig;
    bool m_updating = false;
    QSpinBox *m_width;
    QSpinBox *m_height;
    QDoubleSpinBox *m_percent;
    QCheckBox *m_keepAspect;
};
