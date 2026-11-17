// Small wrappers around platform-native UI.
#pragma once

#include <QString>

class QWidget;

namespace platform {

// Asks a yes/no question, defaulting to Yes; Esc answers No. Uses the system
// task dialog on Windows and a QMessageBox elsewhere.
bool confirm(QWidget *parent, const QString &title, const QString &question, const QString &details);

// What the desktop calls its trash: "Recycle Bin" on Windows, else "Trash".
QString trashName();

// Whether deleting this file would go to the trash. False on Windows when the
// file's drive removes files immediately ("Don't move files to the Recycle
// Bin" or a policy), or has no Recycle Bin (network, removable). Elsewhere it
// is true, and moveToTrash() failing is how a missing trash shows up.
bool canTrash(const QString &path);

enum class TrashResult { Done, Cancelled, Failed };

// Moves a file to the trash. On Windows this goes through the shell, which
// itself warns (and lets the user cancel) when the file would be deleted
// permanently because its drive has no Recycle Bin.
TrashResult moveToTrash(QWidget *parent, const QString &path, QString *error);

} // namespace platform
