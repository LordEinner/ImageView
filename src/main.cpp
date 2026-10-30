#include <QApplication>
#include <QIcon>
#include <QImageReader>
#include <QUrl>

#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QImageReader::setAllocationLimit(4096);
    QApplication app(argc, argv);
    app.setApplicationName(MainWindow::AppName);
    app.setDesktopFileName("imageview");
    app.setWindowIcon(QIcon(":/icon.svg"));

    QString path;
    const QStringList args = app.arguments();
    if (args.size() > 1) {
        const QString &arg = args[1];
        path = arg.startsWith("file://") ? QUrl(arg).toLocalFile() : arg;
    }
    MainWindow win(path);
    win.show();
    return app.exec();
}
