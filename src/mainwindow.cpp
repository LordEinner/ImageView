#include "mainwindow.h"

#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QDragEnterEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QGuiApplication>
#include <QInputDialog>
#include <QKeyEvent>
#include <QLabel>
#include <QMenu>
#include <QMessageBox>
#include <QMimeData>
#include <QScreen>
#include <QStatusBar>
#include <QToolBar>
#include <QTransform>
#include <algorithm>
#include <numeric>

#include "canvas.h"
#include "icons.h"
#include "imageio.h"
#include "resizedialog.h"
#include "zoomfield.h"

namespace {

constexpr double ZoomStep = 1.25;
constexpr int DefaultQuality = 90;
// Undo history is trimmed, oldest first, to stay under this many bytes.
constexpr qint64 MaxUndoBytes = qint64(1) << 30;

// Position of the extension's dot in path, like Python's os.path.splitext
// (leading dots of the file name don't count), or -1 if there is none.
qsizetype extensionDot(const QString &path)
{
    qsizetype start = path.lastIndexOf('/') + 1;
    while (start < path.size() && path[start] == '.')
        ++start;
    const qsizetype dot = path.lastIndexOf('.');
    return dot >= start ? dot : -1;
}

QString extension(const QString &path)
{
    const qsizetype dot = extensionDot(path);
    return dot < 0 ? QString() : path.mid(dot + 1).toLower();
}

// Natural order on case-folded names: runs of digits compare as numbers.
int naturalCompare(QStringView a, QStringView b)
{
    qsizetype i = 0, j = 0;
    for (;;) {
        const qsizetype si = i, sj = j;
        while (i < a.size() && !a[i].isDigit())
            ++i;
        while (j < b.size() && !b[j].isDigit())
            ++j;
        if (int c = a.sliced(si, i - si).compare(b.sliced(sj, j - sj)))
            return c < 0 ? -1 : 1;
        const bool endA = i == a.size(), endB = j == b.size();
        if (endA || endB)
            return endA == endB ? 0 : (endA ? -1 : 1);
        while (i < a.size() && a[i].digitValue() == 0)
            ++i;
        while (j < b.size() && b[j].digitValue() == 0)
            ++j;
        const qsizetype di = i, dj = j;
        while (i < a.size() && a[i].isDigit())
            ++i;
        while (j < b.size() && b[j].isDigit())
            ++j;
        if (i - di != j - dj)
            return i - di < j - dj ? -1 : 1;
        for (qsizetype k = 0; k < i - di; ++k)
            if (int d = a[di + k].digitValue() - b[dj + k].digitValue())
                return d < 0 ? -1 : 1;
    }
}

QStringList listImages(const QString &directory)
{
    const QSet<QString> &exts = imageio::readableExtensions();
    QStringList names;
    for (const QString &n : QDir(directory).entryList(QDir::Files | QDir::Hidden, QDir::Unsorted)) {
        const qsizetype dot = extensionDot(n);
        if (dot >= 0 && exts.contains(n.mid(dot).toLower()))
            names << n;
    }
    QStringList keys;
    keys.reserve(names.size());
    for (const QString &n : names)
        keys << n.toCaseFolded();
    std::vector<qsizetype> order(names.size());
    std::iota(order.begin(), order.end(), 0);
    std::stable_sort(order.begin(), order.end(),
                     [&](qsizetype x, qsizetype y) { return naturalCompare(keys[x], keys[y]) < 0; });
    QStringList sorted;
    sorted.reserve(names.size());
    for (qsizetype k : order)
        sorted << names[k];
    return sorted;
}

QString humanSize(double n)
{
    for (const char *unit : {"B", "KB", "MB", "GB"}) {
        if (n < 1024 || qstrcmp(unit, "GB") == 0)
            return qstrcmp(unit, "B") == 0 ? QString("%1 %2").arg(n, 0, 'f', 0).arg(unit)
                                           : QString("%1 %2").arg(n, 0, 'f', 1).arg(unit);
        n /= 1024;
    }
    return QString();
}

} // namespace

MainWindow::MainWindow(const QString &path, QWidget *parent) : QMainWindow(parent), m_quality(DefaultQuality)
{
    m_canvas = new ImageCanvas(this);
    connect(m_canvas, &ImageCanvas::viewChanged, this, &MainWindow::updateStatus);
    connect(m_canvas, &ImageCanvas::doubleClicked, this, &MainWindow::toggleFullscreen);
    connect(m_canvas, &ImageCanvas::contextMenuRequested, this, &MainWindow::showContextMenu);
    setCentralWidget(m_canvas);
    m_canvas->setFocus();

    m_status = new QLabel;
    statusBar()->addWidget(m_status, 1);
    buildToolbar();
    setAcceptDrops(true);
    // Right-drag pans the image; don't pop up the toolbar context menu.
    setContextMenuPolicy(Qt::NoContextMenu);

    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    resize(std::min(1280, int(screen.width() * 0.8)), std::min(860, int(screen.height() * 0.8)));

    if (!path.isEmpty()) {
        openPath(path);
    } else {
        m_canvas->clear("Open an image with Ctrl+O or drop one here");
        updateTitle();
        updateStatus();
    }
}

// --- toolbar ---------------------------------------------------------------

void MainWindow::buildToolbar()
{
    auto bar = new QToolBar(this);
    bar->setMovable(false);
    bar->setFloatable(false);
    bar->setIconSize(QSize(22, 22));
    bar->setStyleSheet("QToolBar::separator { background: palette(mid); width: 1px; margin: 3px 9px; }");
    bar->toggleViewAction()->setVisible(false);
    addToolBar(bar);
    m_toolbar = bar;
    const QPalette pal = palette();

    auto action = [&](const QString &iconName, const QString &text, auto slot, const QKeySequence &shortcut = {},
                      QString keyHint = {}, bool needsImage = true) {
        auto a = new QAction(icons::icon(iconName, pal), text, this);
        if (!shortcut.isEmpty()) {
            a->setShortcut(shortcut);
            keyHint = a->shortcut().toString(QKeySequence::NativeText);
        }
        a->setToolTip(keyHint.isEmpty() ? text : QString("%1 (%2)").arg(text, keyHint));
        connect(a, &QAction::triggered, this, slot);
        bar->addAction(a);
        // Keep shortcuts working while the toolbar is hidden in fullscreen.
        addAction(a);
        if (needsImage)
            m_imageActions << a;
        m_actions[iconName] = a;
        return a;
    };
    auto rotate = [this](double angle) {
        return [this, angle] { edit([angle](const QImage &img) { return img.transformed(QTransform().rotate(angle)); }); };
    };
    auto flip = [this](Qt::Orientation o) {
        return [this, o] { edit([o](const QImage &img) { return img.flipped(o); }); };
    };

    // Keys handled in keyPressEvent get only a tooltip hint, not a real
    // shortcut, so both main-keyboard and numpad variants work.
    action("open", "Open", [this] { openDialog(); }, QKeySequence::Open, {}, false);
    action("save", "Save", [this] { save(); }, QKeySequence::Save);
    action("save-as", "Save As", [this] { saveAs(); }, QKeySequence::SaveAs);
    bar->addSeparator();
    action("previous", "Previous Image", [this] { navigate(-1); }, {}, "Left", false);
    action("next", "Next Image", [this] { navigate(1); }, {}, "Right", false);
    bar->addSeparator();
    action("undo", "Undo", [this] { undo(); }, QKeySequence::Undo);
    action("copy", "Copy", [this] { copy(); }, QKeySequence::Copy);
    action("crop", "Crop to Selection", [this] { crop(); }, QKeySequence("Ctrl+Y"));
    action("resize", "Resize", [this] { resizeImage(); }, QKeySequence("Ctrl+R"));
    bar->addSeparator();
    action("rotate-left", "Rotate Left", rotate(-90), QKeySequence("L"));
    action("rotate-right", "Rotate Right", rotate(90), QKeySequence("R"));
    action("flip-horizontal", "Flip Horizontal", flip(Qt::Horizontal), QKeySequence("H"));
    action("flip-vertical", "Flip Vertical", flip(Qt::Vertical), QKeySequence("V"));
    bar->addSeparator();
    action("zoom-out", "Zoom Out", [this] { m_canvas->zoomBy(1 / ZoomStep); }, {}, "-");
    auto reset = new QAction(icons::icon("zoom-reset", pal), "Reset Zoom", this);
    reset->setToolTip("Reset Zoom (0)");
    connect(reset, &QAction::triggered, m_canvas, &ImageCanvas::resetZoom);
    m_zoomField = new ZoomField(reset, this);
    connect(m_zoomField, &ZoomField::zoomRequested, m_canvas, &ImageCanvas::setZoom);
    connect(m_zoomField, &ZoomField::editingDone, m_canvas, [this] { m_canvas->setFocus(); });
    bar->addWidget(m_zoomField);
    action("zoom-in", "Zoom In", [this] { m_canvas->zoomBy(ZoomStep); }, {}, "+");
    auto spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    bar->addWidget(spacer);
    action("fullscreen", "Fullscreen", [this] { toggleFullscreen(); }, {}, "Enter", false);
}

void MainWindow::showContextMenu(QPoint pos)
{
    QMenu menu(this);
    menu.setStyleSheet("QMenu::separator { height: 1px; background: palette(mid); margin: 5px 8px; }");
    if (!m_undo.empty())
        menu.addAction(m_actions["undo"]);
    menu.addAction(m_actions["copy"]);
    if (!m_canvas->selection().isEmpty())
        menu.addAction(m_actions["crop"]);
    menu.addSeparator();
    for (const char *name : {"rotate-left", "rotate-right", "flip-horizontal", "flip-vertical"})
        menu.addAction(m_actions[name]);
    menu.addSeparator();
    const QPalette pal = palette();
    if (isFullScreen())
        menu.addAction(icons::icon("fullscreen", pal), "Exit Fullscreen\tEsc", this, &MainWindow::toggleFullscreen);
    else
        menu.addAction(icons::icon("exit", pal), "Exit\tEsc", this, &QWidget::close);
    menu.exec(pos);
}

void MainWindow::updateActions()
{
    const bool hasImage = m_canvas->hasImage();
    for (QAction *a : std::as_const(m_imageActions))
        a->setEnabled(hasImage);
    m_actions["undo"]->setEnabled(hasImage && !m_undo.empty());
    m_actions["crop"]->setEnabled(hasImage && !m_canvas->selection().isEmpty());
    m_zoomField->setEnabled(hasImage);
    m_zoomField->setZoom(m_canvas->zoom());
}

// --- keyboard --------------------------------------------------------------

void MainWindow::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Return:
    case Qt::Key_Enter:
        toggleFullscreen();
        break;
    case Qt::Key_Escape:
        if (isFullScreen())
            toggleFullscreen();
        else
            close();
        break;
    case Qt::Key_Right:
    case Qt::Key_Space:
    case Qt::Key_PageDown:
        navigate(1);
        break;
    case Qt::Key_Left:
    case Qt::Key_Backspace:
    case Qt::Key_PageUp:
        navigate(-1);
        break;
    case Qt::Key_Plus:
    case Qt::Key_Equal:
        m_canvas->zoomBy(ZoomStep);
        break;
    case Qt::Key_Minus:
    case Qt::Key_Underscore:
        m_canvas->zoomBy(1 / ZoomStep);
        break;
    case Qt::Key_0:
        m_canvas->resetZoom();
        break;
    default:
        QMainWindow::keyPressEvent(e);
    }
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen()) {
        m_toolbar->show();
        statusBar()->show();
        m_canvas->setFullscreen(false);
        if (m_wasMaximized)
            showMaximized();
        else
            showNormal();
    } else {
        m_wasMaximized = isMaximized();
        m_toolbar->hide();
        statusBar()->hide();
        m_canvas->setFullscreen(true);
        showFullScreen();
    }
}

// --- files -----------------------------------------------------------------

void MainWindow::openPath(const QString &path, std::optional<QStringList> files)
{
    m_path = QFileInfo(path).absoluteFilePath();
    m_files = files ? *files : listImages(QFileInfo(m_path).path());
    m_modified = false;
    m_undo.clear();
    m_savedDepth = 0;
    m_format.clear();
    imageio::Loaded loaded;
    QString error;
    if (!imageio::load(m_path, loaded, &error)) {
        m_canvas->clear(QString("Cannot open %1\n\n%2").arg(QFileInfo(m_path).fileName(), error));
    } else {
        m_format = loaded.format;
        m_quality = DefaultQuality;
        if (loaded.movie)
            m_canvas->setMovie(loaded.movie);
        else
            m_canvas->setImage(loaded.image);
    }
    updateTitle();
    updateStatus();
}

void MainWindow::navigate(int step)
{
    if (m_path.isEmpty())
        return;
    const QFileInfo info(m_path);
    const QString directory = info.path(), name = info.fileName();
    const QStringList files = listImages(directory);
    if (files.isEmpty())
        return;
    const QString key = name.toCaseFolded();
    auto it = std::lower_bound(files.begin(), files.end(), key, [](const QString &f, const QString &k) {
        return naturalCompare(f.toCaseFolded(), k) < 0;
    });
    qsizetype i = it - files.begin();
    if (i < files.size() && files[i] == name)
        i += step;
    else if (step < 0)
        --i; // current file is gone; lower_bound already points at the next one
    const qsizetype n = files.size();
    openPath(QDir(directory).filePath(files[((i % n) + n) % n]), files);
}

void MainWindow::openDialog()
{
    QStringList exts;
    for (const QString &e : imageio::readableExtensions())
        exts << "*" + e;
    exts.sort();
    const QString start = m_path.isEmpty() ? QDir::homePath() : QFileInfo(m_path).path();
    const QString path = QFileDialog::getOpenFileName(this, "Open Image", start,
                                                      QString("Images (%1);;All files (*)").arg(exts.join(' ')));
    if (!path.isEmpty())
        openPath(path);
}

void MainWindow::save()
{
    if (!m_canvas->hasImage())
        return;
    if (!imageio::canSave(extension(m_path))) {
        saveAs();
        return;
    }
    QString msg = QString("Overwrite %1?").arg(QFileInfo(m_path).fileName());
    if (m_canvas->isAnimated())
        msg += "\n\nOnly the current frame of the animation will be saved.";
    if (QMessageBox::question(this, "Save", msg) == QMessageBox::Yes)
        write(m_canvas->image(), m_path, fileQuality());
}

void MainWindow::saveAs()
{
    if (!m_canvas->hasImage())
        return;
    const QImage img = m_canvas->image();
    const auto &filters = imageio::saveFilters();
    if (filters.isEmpty())
        return;
    const qsizetype dot = extensionDot(m_path);
    const QString base = dot < 0 ? m_path : m_path.left(dot);
    QString ext = extension(m_path);
    if (!imageio::canSave(ext))
        ext = "png";
    QString selected = filters.first().first;
    for (const auto &[f, e] : filters) {
        if (e == ext || f.contains("*." + ext)) {
            selected = f;
            break;
        }
    }
    QStringList names;
    for (const auto &f : filters)
        names << f.first;
    QString path = QFileDialog::getSaveFileName(this, "Save As", base + "." + ext, names.join(";;"), &selected);
    if (path.isEmpty())
        return;
    ext = extension(path);
    if (ext.isEmpty()) {
        ext = "png";
        for (const auto &[f, e] : filters)
            if (f == selected)
                ext = e;
        path += "." + ext;
    }
    int quality = fileQuality();
    if (imageio::LossyExtensions.contains(ext)) {
        bool ok = false;
        quality = QInputDialog::getInt(this, "Save As", "Quality (1–100):", quality, 1, 100, 1, &ok);
        if (!ok)
            return;
        m_quality = quality;
    }
    write(img, path, quality);
}

// Quality of the file on disk, else the last one chosen in Save As.
int MainWindow::fileQuality() const
{
    const int q = imageio::estimateQuality(m_path);
    return q ? q : m_quality;
}

void MainWindow::write(const QImage &img, const QString &path, int quality)
{
    QString error;
    if (!imageio::save(img, path, quality, &error)) {
        QMessageBox::critical(this, "Save failed", QString("Could not save %1:\n%2").arg(path, error));
        return;
    }
    m_path = path;
    m_files = listImages(QFileInfo(path).path());
    m_format = extension(path).toUpper();
    m_modified = false;
    m_savedDepth = qsizetype(m_undo.size());
    updateTitle();
    updateStatus();
    statusBar()->showMessage(QString("Saved %1").arg(path), 3000);
}

// --- editing ---------------------------------------------------------------

void MainWindow::edit(const std::function<QImage(const QImage &)> &transform)
{
    if (!m_canvas->hasImage())
        return;
    if (m_savedDepth > qsizetype(m_undo.size()))
        m_savedDepth = -1; // the saved state was undone and is now replaced
    m_undo.push_back(m_canvas->image());
    m_canvas->setImage(transform(m_canvas->image()));
    qint64 bytes = 0;
    for (const QImage &img : m_undo)
        bytes += img.sizeInBytes();
    while (m_undo.size() > 1 && bytes > MaxUndoBytes) {
        bytes -= m_undo.front().sizeInBytes();
        m_undo.pop_front();
        m_savedDepth = m_savedDepth > 0 ? m_savedDepth - 1 : -1;
    }
    m_modified = true;
    updateTitle();
    updateStatus();
}

void MainWindow::undo()
{
    if (m_undo.empty())
        return;
    m_canvas->setImage(m_undo.back());
    m_undo.pop_back();
    m_modified = qsizetype(m_undo.size()) != m_savedDepth;
    updateTitle();
    updateStatus();
}

void MainWindow::crop()
{
    const QRect sel = m_canvas->selection();
    if (sel.isEmpty()) {
        statusBar()->showMessage("Select an area with the mouse first", 3000);
        return;
    }
    edit([sel](const QImage &img) { return img.copy(sel); });
}

void MainWindow::resizeImage()
{
    if (!m_canvas->hasImage())
        return;
    ResizeDialog dlg(m_canvas->image().size(), this);
    if (dlg.exec()) {
        const QSize size = dlg.newSize();
        edit([size](const QImage &img) { return img.scaled(size, Qt::IgnoreAspectRatio, Qt::SmoothTransformation); });
    }
}

void MainWindow::copy()
{
    if (!m_canvas->hasImage())
        return;
    QImage img = m_canvas->image();
    const QRect sel = m_canvas->selection();
    if (!sel.isEmpty())
        img = img.copy(sel);
    QApplication::clipboard()->setImage(img);
    statusBar()->showMessage(QString("Copied %1 × %2 to clipboard").arg(img.width()).arg(img.height()), 3000);
}

// --- status ----------------------------------------------------------------

void MainWindow::updateTitle()
{
    if (m_path.isEmpty())
        setWindowTitle(AppName);
    else
        setWindowTitle(QString("%1%2 — %3").arg(m_modified ? "*" : "", QFileInfo(m_path).fileName(), AppName));
}

void MainWindow::updateStatus()
{
    updateActions();
    if (m_path.isEmpty()) {
        m_status->clear();
        return;
    }
    const QFileInfo info(m_path);
    const QString name = info.fileName();
    QStringList parts{name};
    if (const qsizetype i = m_files.indexOf(name); i >= 0)
        parts << QString("%1 / %2").arg(i + 1).arg(m_files.size());
    if (m_canvas->hasImage()) {
        const QImage &img = m_canvas->image();
        parts << QString("%1 × %2").arg(img.width()).arg(img.height());
        const QString fmt = m_format + (m_canvas->isAnimated() ? " (animated)" : "");
        if (!fmt.isEmpty())
            parts << fmt;
        if (info.exists())
            parts << humanSize(double(info.size()));
        parts << QString::number(m_canvas->zoom() * 100, 'f', 0) + "%";
        const QRect sel = m_canvas->selection();
        if (!sel.isEmpty())
            parts << QString("Selection: %1, %2  %3 × %4").arg(sel.x()).arg(sel.y()).arg(sel.width()).arg(sel.height());
    }
    if (m_modified)
        parts << "modified";
    m_status->setText(parts.join("   |   "));
}

// --- drag & drop -----------------------------------------------------------

void MainWindow::dragEnterEvent(QDragEnterEvent *e)
{
    if (e->mimeData()->hasUrls())
        e->acceptProposedAction();
}

void MainWindow::dropEvent(QDropEvent *e)
{
    for (const QUrl &url : e->mimeData()->urls()) {
        if (url.isLocalFile()) {
            openPath(url.toLocalFile());
            break;
        }
    }
}
