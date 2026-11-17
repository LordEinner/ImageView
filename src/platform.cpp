#include "platform.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QSettings>
#include <QVariant>
#include <QWidget>

#ifdef Q_OS_WIN
#include <string>
#include <windows.h>
#include <commctrl.h>
#include <shellapi.h>

namespace {

PCWSTR wide(const QString &s)
{
    return reinterpret_cast<PCWSTR>(s.utf16());
}

} // namespace

bool platform::confirm(QWidget *parent, const QString &title, const QString &question, const QString &details)
{
    const HWND owner = parent ? reinterpret_cast<HWND>(parent->window()->winId()) : nullptr;
    TASKDIALOGCONFIG config = {};
    config.cbSize = sizeof(config);
    config.hwndParent = owner;
    config.dwFlags = TDF_ALLOW_DIALOG_CANCELLATION | TDF_POSITION_RELATIVE_TO_WINDOW;
    config.dwCommonButtons = TDCBF_YES_BUTTON | TDCBF_NO_BUTTON;
    config.pszWindowTitle = wide(title);
    config.pszMainIcon = TD_WARNING_ICON;
    config.pszMainInstruction = wide(question);
    config.pszContent = details.isEmpty() ? nullptr : wide(details);
    config.nDefaultButton = IDYES;
    int button = 0;
    if (SUCCEEDED(TaskDialogIndirect(&config, &button, nullptr, nullptr)))
        return button == IDYES;
    // Task dialogs need Common Controls 6 (see the app manifest); fall back.
    const QString text = details.isEmpty() ? question : question + "\n\n" + details;
    return MessageBoxW(owner, wide(text), wide(title), MB_YESNO | MB_ICONWARNING | MB_DEFBUTTON1) == IDYES;
}

QString platform::trashName()
{
    return QStringLiteral("Recycle Bin");
}

bool platform::canTrash(const QString &path)
{
    const QString policy = "Software\\Microsoft\\Windows\\CurrentVersion\\Policies\\Explorer";
    for (const char *hive : {"HKEY_CURRENT_USER\\", "HKEY_LOCAL_MACHINE\\"})
        if (QSettings(hive + policy, QSettings::NativeFormat).value("NoRecycleFiles").toInt() == 1)
            return false;

    const std::wstring native = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toStdWString();
    wchar_t root[MAX_PATH], volume[MAX_PATH];
    if (!GetVolumePathNameW(native.c_str(), root, MAX_PATH))
        return false;
    if (GetDriveTypeW(root) != DRIVE_FIXED)
        return false;

    const QString bitBucket = "HKEY_CURRENT_USER\\Software\\Microsoft\\Windows\\CurrentVersion\\Explorer\\BitBucket";
    QVariant nuke;
    if (GetVolumeNameForVolumeMountPointW(root, volume, MAX_PATH)) {
        // "\\?\Volume{GUID}\" -> "{GUID}"
        const QString name = QString::fromWCharArray(volume);
        const qsizetype open = name.indexOf('{'), close = name.indexOf('}');
        if (open >= 0 && close > open)
            nuke = QSettings(bitBucket + "\\Volume\\" + name.mid(open, close - open + 1), QSettings::NativeFormat)
                       .value("NukeOnDelete");
    }
    if (!nuke.isValid()) // older Windows: one setting for all drives
        nuke = QSettings(bitBucket, QSettings::NativeFormat).value("NukeOnDelete");
    return nuke.toInt() != 1;
}

platform::TrashResult platform::moveToTrash(QWidget *parent, const QString &path, QString *error)
{
    // SHFileOperation wants a list ending in two NULs.
    std::wstring from = QDir::toNativeSeparators(QFileInfo(path).absoluteFilePath()).toStdWString();
    from.push_back(L'\0');
    SHFILEOPSTRUCTW op = {};
    op.hwnd = parent ? reinterpret_cast<HWND>(parent->window()->winId()) : nullptr;
    op.wFunc = FO_DELETE;
    op.pFrom = from.c_str();
    op.fFlags = FOF_ALLOWUNDO | FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_WANTNUKEWARNING;
    const int rc = SHFileOperationW(&op);
    if (op.fAnyOperationsAborted)
        return TrashResult::Cancelled;
    if (rc == 0 && !QFileInfo::exists(path))
        return TrashResult::Done;
    *error = rc ? QString("error 0x%1").arg(uint(rc), 0, 16) : QString("the file is still there");
    return TrashResult::Failed;
}

#else

bool platform::confirm(QWidget *parent, const QString &title, const QString &question, const QString &details)
{
    QMessageBox box(QMessageBox::Warning, title, question, QMessageBox::Yes | QMessageBox::No, parent);
    box.setInformativeText(details);
    box.setDefaultButton(QMessageBox::Yes);
    box.setEscapeButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

QString platform::trashName()
{
    return QStringLiteral("Trash");
}

bool platform::canTrash(const QString &)
{
    return true;
}

platform::TrashResult platform::moveToTrash(QWidget *, const QString &path, QString *error)
{
    QFile file(path);
    if (file.moveToTrash())
        return TrashResult::Done;
    *error = file.errorString();
    return TrashResult::Failed;
}

#endif
