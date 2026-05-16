// ======================================================================
//
// GodClientVirtualPath.h
// copyright 2026 Sony Online Entertainment
//
// Map picked disk files under God Client's local client data tree to
// canonical virtual paths (shader/foo.sht, object/.../*.apt).
//
// ======================================================================

#ifndef INCLUDED_GodClientVirtualPath_H
#define INCLUDED_GodClientVirtualPath_H

#include "ConfigGodClient.h"

#include <qdir.h>
#include <qfiledialog.h>
#include <qfileinfo.h>
#include <qstring.h>
#include <qwidget.h>

// Qt 3.3 has QDir::convertSeparators/canonicalPath but no QDir::cleanPath — normalize for comparisons.
inline QString godClientCleanPathQString(QString path)
{
	if (path.isNull())
		return path;

	path = path.stripWhiteSpace();
	if (path.isEmpty())
		return path;

	path = QDir::convertSeparators(path);

	QFileInfo const fi(path);
	QString out(fi.absFilePath());
	if (out.isEmpty())
		out = path;

	out.replace('\\', '/');

	while (!out.isEmpty() && out.at(out.length() - 1) == QChar('/'))
		out.truncate(out.length() - 1);

	return out;
}

// ----------------------------------------------------------------------

/// If \p absolutePath sits under `[GodClient]/localClientDataPath`, returns the depot-style
/// path with forward slashes (no drive). Otherwise returns QString::null.
inline QString godClientTryVirtualClientDataPath(QString const& absolutePath)
{
	QString const trimmedRoot = QString::fromLatin1(ConfigGodClient::getData().localClientDataPath).stripWhiteSpace();
	if (trimmedRoot.isEmpty())
		return QString::null;

	QString aa = godClientCleanPathQString(absolutePath);
	QString rr = godClientCleanPathQString(trimmedRoot);

	while (!rr.isEmpty() && rr.at(rr.length() - 1u) == '/')
		rr.truncate(rr.length() - 1);

#if defined(WIN32) || defined(_WIN32)
	if (!aa.lower().startsWith(rr.lower()))
		return QString::null;
#else
	if (!aa.startsWith(rr))
		return QString::null;
#endif

	QString rel = aa.mid(rr.length());
	while (!rel.isEmpty() && rel.at(0) == '/')
		rel = rel.mid(1);
	return rel;
}

/// Convenience file-picker starting in \p startDirHint (typically local client data).
inline QString godClientOpenAsset(QWidget* parent, char const* title, QString const& filter, QString const& startDirHint)
{
	QString dir = startDirHint;
	if (dir.isEmpty())
		dir = QString::fromLatin1(ConfigGodClient::getData().localClientDataPath);
	QString const pick = QFileDialog::getOpenFileName(dir, filter, parent, "godbrowse", title);
	if (pick.isEmpty())
		return QString::null;
	QString const virt = godClientTryVirtualClientDataPath(pick);
	if (!virt.isEmpty())
		return virt;
	QFileInfo const fi(pick);
	return fi.fileName();
}

#endif // INCLUDED_GodClientVirtualPath_H
