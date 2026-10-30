// Main window: toolbar, status bar, file navigation and editing commands.
#pragma once

#include <QHash>
#include <QImage>
#include <QMainWindow>
#include <deque>
#include <functional>
#include <optional>

class ImageCanvas;
class QLabel;
class QToolBar;
class ZoomField;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    static constexpr const char *AppName = "ImageView";

    explicit MainWindow(const QString &path = QString(), QWidget *parent = nullptr);

    void openPath(const QString &path, std::optional<QStringList> files = std::nullopt);
    void navigate(int step);

protected:
    void keyPressEvent(QKeyEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dropEvent(QDropEvent *e) override;

private:
    void buildToolbar();
    void showContextMenu(QPoint pos);
    void updateActions();
    void toggleFullscreen();

    void openDialog();
    void save();
    void saveAs();
    int fileQuality() const;
    void write(const QImage &img, const QString &path, int quality);

    void edit(const std::function<QImage(const QImage &)> &transform);
    void undo();
    void crop();
    void resizeImage();
    void copy();

    void updateTitle();
    void updateStatus();

    QString m_path;
    QStringList m_files;
    QString m_format;
    bool m_modified = false;
    // Images before each edit, oldest first, and the undo depth matching the
    // file on disk (-1 once that state has been dropped or overwritten).
    std::deque<QImage> m_undo;
    qsizetype m_savedDepth = 0;
    bool m_wasMaximized = false;
    int m_quality;

    ImageCanvas *m_canvas;
    QLabel *m_status;
    QToolBar *m_toolbar = nullptr;
    ZoomField *m_zoomField = nullptr;
    QList<QAction *> m_imageActions;
    QHash<QString, QAction *> m_actions;
};
