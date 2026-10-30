#include "resizedialog.h"

#include <QCheckBox>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QLabel>
#include <QSpinBox>

ResizeDialog::ResizeDialog(QSize size, QWidget *parent) : QDialog(parent), m_orig(size)
{
    setWindowTitle("Resize Image");

    auto spin = [](int value) {
        auto box = new QSpinBox;
        box->setRange(1, 65535);
        box->setValue(value);
        box->setSuffix(" px");
        return box;
    };
    m_width = spin(size.width());
    m_height = spin(size.height());
    m_percent = new QDoubleSpinBox;
    m_percent->setRange(0.1, 10000);
    m_percent->setDecimals(1);
    m_percent->setValue(100);
    m_percent->setSuffix(" %");
    m_keepAspect = new QCheckBox("Keep aspect ratio");
    m_keepAspect->setChecked(true);

    connect(m_width, &QSpinBox::valueChanged, this, &ResizeDialog::widthChanged);
    connect(m_height, &QSpinBox::valueChanged, this, &ResizeDialog::heightChanged);
    connect(m_percent, &QDoubleSpinBox::valueChanged, this, &ResizeDialog::percentChanged);

    auto buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttons, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    auto form = new QFormLayout(this);
    form->addRow("Original:", new QLabel(QString("%1 × %2 px").arg(size.width()).arg(size.height())));
    form->addRow("Width:", m_width);
    form->addRow("Height:", m_height);
    form->addRow("Scale:", m_percent);
    form->addRow(m_keepAspect);
    form->addRow(buttons);
}

QSize ResizeDialog::newSize() const
{
    return QSize(m_width->value(), m_height->value());
}

void ResizeDialog::setValues(std::optional<double> w, std::optional<double> h, std::optional<double> pct)
{
    m_updating = true;
    if (w)
        m_width->setValue(std::max(1, qRound(*w)));
    if (h)
        m_height->setValue(std::max(1, qRound(*h)));
    if (pct)
        m_percent->setValue(*pct);
    m_updating = false;
}

void ResizeDialog::widthChanged(int w)
{
    if (m_updating)
        return;
    const double ratio = double(w) / m_orig.width();
    if (m_keepAspect->isChecked())
        setValues({}, m_orig.height() * ratio, ratio * 100);
}

void ResizeDialog::heightChanged(int h)
{
    if (m_updating)
        return;
    const double ratio = double(h) / m_orig.height();
    if (m_keepAspect->isChecked())
        setValues(m_orig.width() * ratio, {}, ratio * 100);
}

void ResizeDialog::percentChanged(double pct)
{
    if (m_updating)
        return;
    setValues(m_orig.width() * pct / 100, m_orig.height() * pct / 100, {});
}
